#include "tester_scan.h"

#include <string.h>

static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static void release_all_inputs(tester_scan_t *scan)
{
    if (scan != NULL) {
        (void)pcal6524_set_all_inputs(&scan->end[TESTER_END_J1]);
        (void)pcal6524_set_all_inputs(&scan->end[TESTER_END_J2]);
    }
}

static void fail_scan(tester_scan_t *scan, tester_scan_error_t error, uint32_t now_ms)
{
    if (scan == NULL) {
        return;
    }
    release_all_inputs(scan);
    scan->error = error;
    scan->state = TESTER_SCAN_ERROR;
    scan->finished_at_ms = now_ms;
}

static bool endpoint_is_baseline_low(const tester_scan_t *scan, uint8_t endpoint)
{
    tester_end_t end = tester_endpoint_end(endpoint);
    tester_contact_t contact = tester_endpoint_contact(endpoint);
    uint8_t pin = scan->config.contact_to_pcal_pin[end][contact];

    return (scan->baseline_inputs[end][pin >> 3u] & (uint8_t)(1u << (pin & 7u))) == 0u;
}

static bool baseline_is_valid(const tester_scan_t *scan)
{
    uint8_t endpoint;
    tester_contact_t contact;

    for (endpoint = 0u; endpoint < TESTER_ENDPOINT_COUNT; ++endpoint) {
        if (!endpoint_is_baseline_low(scan, endpoint)) {
            continue;
        }
        contact = tester_endpoint_contact(endpoint);
        /* An unpowered eMarker may legitimately expose Ra on physical A5/B5. */
        if ((contact != TESTER_CONTACT_A5) && (contact != TESTER_CONTACT_B5)) {
            return false;
        }
    }
    return true;
}

static bool ports_are_all_inputs(const uint8_t configuration[PCAL6524_PORT_COUNT])
{
    return (configuration[0] == 0xFFu) && (configuration[1] == 0xFFu) &&
           (configuration[2] == 0xFFu);
}

static bool contact_needs_power_settle(tester_contact_t contact)
{
    switch (contact) {
    case TESTER_CONTACT_A1:
    case TESTER_CONTACT_A4:
    case TESTER_CONTACT_A5:
    case TESTER_CONTACT_A9:
    case TESTER_CONTACT_A12:
    case TESTER_CONTACT_B1:
    case TESTER_CONTACT_B4:
    case TESTER_CONTACT_B5:
    case TESTER_CONTACT_B9:
    case TESTER_CONTACT_B12:
        return true;
    default:
        return false;
    }
}

static bool release_has_new_low(tester_scan_t *scan)
{
    uint8_t end;
    uint8_t port;
    bool has_new_low = false;

    for (end = 0u; end < TESTER_END_COUNT; ++end) {
        for (port = 0u; port < PCAL6524_PORT_COUNT; ++port) {
            scan->release_new_low[end][port] =
                (uint8_t)(scan->baseline_inputs[end][port] &
                          (uint8_t)~scan->release_inputs[end][port]);
            scan->release_new_high[end][port] =
                (uint8_t)((uint8_t)~scan->baseline_inputs[end][port] &
                          scan->release_inputs[end][port]);
            has_new_low = has_new_low || (scan->release_new_low[end][port] != 0u);
        }
    }
    return has_new_low;
}

static void advance_source(tester_scan_t *scan, uint32_t now_ms)
{
    scan->completed_source_count = (uint8_t)(scan->source_endpoint + 1u);
    if ((uint8_t)(scan->source_endpoint + 1u) >= TESTER_ENDPOINT_COUNT) {
        scan->finished_at_ms = now_ms;
        scan->state = TESTER_SCAN_DONE;
    } else {
        ++scan->source_endpoint;
        scan->state = TESTER_SCAN_BREAK;
    }
}

static void collect_sample(tester_scan_t *scan, const uint8_t j1[PCAL6524_PORT_COUNT], const uint8_t j2[PCAL6524_PORT_COUNT])
{
    uint8_t endpoint;
    uint8_t pin;
    tester_end_t end;
    tester_contact_t contact;
    const uint8_t *ports;

    end = tester_endpoint_end(scan->source_endpoint);
    contact = tester_endpoint_contact(scan->source_endpoint);
    pin = scan->config.contact_to_pcal_pin[end][contact];
    ports = (end == TESTER_END_J1) ? j1 : j2;
    if ((ports[pin >> 3u] & (uint8_t)(1u << (pin & 7u))) == 0u) {
        if (scan->source_low_votes < UINT8_MAX) {
            ++scan->source_low_votes;
        }
    }

    for (endpoint = 0u; endpoint < TESTER_ENDPOINT_COUNT; ++endpoint) {
        if ((endpoint == scan->source_endpoint) || endpoint_is_baseline_low(scan, endpoint)) {
            continue;
        }
        end = tester_endpoint_end(endpoint);
        contact = tester_endpoint_contact(endpoint);
        pin = scan->config.contact_to_pcal_pin[end][contact];
        ports = (end == TESTER_END_J1) ? j1 : j2;
        if ((ports[pin >> 3u] & (uint8_t)(1u << (pin & 7u))) == 0u) {
            if (scan->vote_count[endpoint] < UINT8_MAX) {
                ++scan->vote_count[endpoint];
            }
        }
    }
}

static void commit_source_votes(tester_scan_t *scan)
{
    uint8_t endpoint;
    uint8_t votes;

    for (endpoint = 0u; endpoint < TESTER_ENDPOINT_COUNT; ++endpoint) {
        if (endpoint == scan->source_endpoint) {
            continue;
        }
        votes = scan->vote_count[endpoint];
        /* Only 5/5 is a stable DC path; partial votes remain diagnostic instability. */
        if (votes == scan->config.samples_per_source) {
            tester_matrix_set(&scan->observation.low, scan->source_endpoint, endpoint, true);
        }
        if ((votes != 0u) && (votes != scan->config.samples_per_source)) {
            tester_matrix_set(&scan->observation.unstable, scan->source_endpoint, endpoint, true);
        }
    }
}

tester_scan_config_t tester_scan_default_config(void)
{
    tester_scan_config_t config;
    uint8_t end;
    uint8_t contact;

    config.samples_per_source = 5u;
    config.vote_threshold = 3u;
    /* Weak PCAL pull-ups need time to reject cable/eMarker capacitance. */
    config.settle_time_ms = 50u;
    config.power_settle_time_ms = 250u;
    config.sample_interval_ms = 1u;
    config.release_settle_time_ms = 10u;
    config.release_retry_interval_ms = 5u;
    config.release_timeout_ms = 100u;
    for (end = 0u; end < TESTER_END_COUNT; ++end) {
        for (contact = 0u; contact < TESTER_CONTACT_COUNT; ++contact) {
            config.contact_to_pcal_pin[end][contact] = contact;
        }
    }
    return config;
}

static bool prepare_channel_maps(tester_scan_t *scan)
{
    uint8_t end;
    uint8_t contact;
    uint8_t pin;
    uint32_t seen;

    for (end = 0u; end < TESTER_END_COUNT; ++end) {
        seen = 0u;
        for (contact = 0u; contact < TESTER_CONTACT_COUNT; ++contact) {
            pin = scan->config.contact_to_pcal_pin[end][contact];
            if ((pin >= TESTER_CONTACT_COUNT) || ((seen & (1ul << pin)) != 0u)) {
                return false;
            }
            seen |= (1ul << pin);
            scan->pin_to_contact[end][pin] = contact;
        }
    }
    return true;
}

bool tester_scan_init(
    tester_scan_t *scan,
    const tester_platform_t *platform,
    uint8_t end_j1_address,
    uint8_t end_j2_address,
    const tester_scan_config_t *config)
{
    tester_scan_config_t selected;

    if ((scan == NULL) || (platform == NULL) || (platform->i2c_write == NULL) ||
        (platform->i2c_read == NULL)) {
        return false;
    }

    selected = (config != NULL) ? *config : tester_scan_default_config();
    if ((selected.samples_per_source == 0u) || (selected.vote_threshold == 0u) ||
        (selected.vote_threshold > selected.samples_per_source) ||
        (selected.settle_time_ms == 0u) || (selected.power_settle_time_ms == 0u) ||
        (selected.release_settle_time_ms == 0u) ||
        (selected.release_retry_interval_ms == 0u) ||
        (selected.release_timeout_ms < selected.release_settle_time_ms)) {
        memset(scan, 0, sizeof(*scan));
        scan->state = TESTER_SCAN_ERROR;
        scan->error = TESTER_SCAN_ERROR_INVALID_CONFIG;
        return false;
    }

    memset(scan, 0, sizeof(*scan));
    scan->config = selected;
    if (!prepare_channel_maps(scan)) {
        scan->state = TESTER_SCAN_ERROR;
        scan->error = TESTER_SCAN_ERROR_INVALID_CONFIG;
        return false;
    }
    pcal6524_bind(&scan->end[TESTER_END_J1], platform, end_j1_address);
    pcal6524_bind(&scan->end[TESTER_END_J2], platform, end_j2_address);

    if (!pcal6524_probe(&scan->end[TESTER_END_J1]) || !pcal6524_probe(&scan->end[TESTER_END_J2]) ||
        !pcal6524_init_test_inputs(&scan->end[TESTER_END_J1]) ||
        !pcal6524_init_test_inputs(&scan->end[TESTER_END_J2]) ||
        !pcal6524_verify_test_inputs(&scan->end[TESTER_END_J1]) ||
        !pcal6524_verify_test_inputs(&scan->end[TESTER_END_J2])) {
        scan->state = TESTER_SCAN_ERROR;
        scan->error = TESTER_SCAN_ERROR_EXPANDER_INIT;
        release_all_inputs(scan);
        return false;
    }

    scan->state = TESTER_SCAN_IDLE;
    scan->error = TESTER_SCAN_ERROR_NONE;
    return true;
}

bool tester_scan_start(tester_scan_t *scan, uint32_t now_ms)
{
    if ((scan == NULL) || tester_scan_busy(scan) || (scan->state == TESTER_SCAN_UNINITIALIZED)) {
        return false;
    }

    if (!pcal6524_init_test_inputs(&scan->end[TESTER_END_J1]) ||
        !pcal6524_init_test_inputs(&scan->end[TESTER_END_J2])) {
        fail_scan(scan, TESTER_SCAN_ERROR_EXPANDER_INIT, now_ms);
        return false;
    }
    if (!pcal6524_verify_test_inputs(&scan->end[TESTER_END_J1]) ||
        !pcal6524_verify_test_inputs(&scan->end[TESTER_END_J2])) {
        fail_scan(scan, TESTER_SCAN_ERROR_EXPANDER_CONFIG, now_ms);
        return false;
    }

    memset(&scan->observation, 0, sizeof(scan->observation));
    memset(scan->vote_count, 0, sizeof(scan->vote_count));
    memset(scan->baseline_inputs, 0xFF, sizeof(scan->baseline_inputs));
    memset(scan->release_inputs, 0xFF, sizeof(scan->release_inputs));
    memset(scan->release_new_low, 0, sizeof(scan->release_new_low));
    memset(scan->release_new_high, 0, sizeof(scan->release_new_high));
    memset(scan->release_configuration, 0, sizeof(scan->release_configuration));
    scan->source_endpoint = 0u;
    scan->completed_source_count = 0u;
    scan->sample_index = 0u;
    scan->release_check_count = 0u;
    scan->release_stable_count = 0u;
    scan->release_recovery_ms = 0u;
    scan->started_at_ms = now_ms;
    scan->finished_at_ms = now_ms;
    scan->deadline_ms = now_ms + scan->config.settle_time_ms;
    scan->error = TESTER_SCAN_ERROR_NONE;
    scan->state = TESTER_SCAN_BASELINE_SETTLE;
    return true;
}

void tester_scan_tick(tester_scan_t *scan, uint32_t now_ms)
{
    uint8_t j1_inputs[PCAL6524_PORT_COUNT];
    uint8_t j2_inputs[PCAL6524_PORT_COUNT];
    tester_end_t source_end;
    tester_contact_t source_contact;
    uint8_t source_pin;

    if (scan == NULL) {
        return;
    }

    switch (scan->state) {
    case TESTER_SCAN_BASELINE_SETTLE:
        if (deadline_reached(now_ms, scan->deadline_ms)) {
            scan->state = TESTER_SCAN_BASELINE_CHECK;
        }
        break;

    case TESTER_SCAN_BASELINE_CHECK:
        if (!pcal6524_read_inputs(
                &scan->end[TESTER_END_J1],
                scan->baseline_inputs[TESTER_END_J1]) ||
            !pcal6524_read_inputs(
                &scan->end[TESTER_END_J2],
                scan->baseline_inputs[TESTER_END_J2])) {
            fail_scan(scan, TESTER_SCAN_ERROR_I2C_READ, now_ms);
            return;
        }
        if (!baseline_is_valid(scan)) {
            fail_scan(scan, TESTER_SCAN_ERROR_BASELINE_STUCK_LOW, now_ms);
            return;
        }
        scan->state = TESTER_SCAN_BREAK;
        break;

    case TESTER_SCAN_BREAK:
        if (!pcal6524_set_all_inputs(&scan->end[TESTER_END_J1]) ||
            !pcal6524_set_all_inputs(&scan->end[TESTER_END_J2])) {
            fail_scan(scan, TESTER_SCAN_ERROR_I2C_WRITE, now_ms);
            return;
        }
        if (endpoint_is_baseline_low(scan, scan->source_endpoint)) {
            advance_source(scan, now_ms);
            break;
        }
        scan->state = TESTER_SCAN_DRIVE;
        break;

    case TESTER_SCAN_DRIVE:
        source_end = tester_endpoint_end(scan->source_endpoint);
        source_contact = tester_endpoint_contact(scan->source_endpoint);
        source_pin = scan->config.contact_to_pcal_pin[source_end][source_contact];
        if (!pcal6524_drive_one_low(&scan->end[source_end], source_pin)) {
            fail_scan(scan, TESTER_SCAN_ERROR_I2C_WRITE, now_ms);
            return;
        }
        memset(scan->vote_count, 0, sizeof(scan->vote_count));
        scan->sample_index = 0u;
        scan->source_low_votes = 0u;
        scan->deadline_ms = now_ms +
                            (contact_needs_power_settle(source_contact)
                                 ? scan->config.power_settle_time_ms
                                 : scan->config.settle_time_ms);
        scan->state = TESTER_SCAN_SETTLE;
        break;

    case TESTER_SCAN_SETTLE:
        if (deadline_reached(now_ms, scan->deadline_ms)) {
            scan->state = TESTER_SCAN_SAMPLE;
        }
        break;

    case TESTER_SCAN_SAMPLE:
        if (!deadline_reached(now_ms, scan->deadline_ms)) {
            break;
        }
        if (!pcal6524_read_inputs(&scan->end[TESTER_END_J1], j1_inputs) ||
            !pcal6524_read_inputs(&scan->end[TESTER_END_J2], j2_inputs)) {
            fail_scan(scan, TESTER_SCAN_ERROR_I2C_READ, now_ms);
            return;
        }
        collect_sample(scan, j1_inputs, j2_inputs);
        ++scan->sample_index;
        if (scan->sample_index >= scan->config.samples_per_source) {
            if (scan->source_low_votes != scan->config.samples_per_source) {
                fail_scan(scan, TESTER_SCAN_ERROR_SOURCE_DRIVE_FAILED, now_ms);
                return;
            }
            scan->state = TESTER_SCAN_RELEASE;
        } else {
            scan->deadline_ms = now_ms + scan->config.sample_interval_ms;
        }
        break;

    case TESTER_SCAN_RELEASE:
        if (!pcal6524_set_all_inputs(&scan->end[TESTER_END_J1]) ||
            !pcal6524_set_all_inputs(&scan->end[TESTER_END_J2])) {
            fail_scan(scan, TESTER_SCAN_ERROR_I2C_WRITE, now_ms);
            return;
        }
        if (!pcal6524_read_configuration(
                &scan->end[TESTER_END_J1],
                scan->release_configuration[TESTER_END_J1]) ||
            !pcal6524_read_configuration(
                &scan->end[TESTER_END_J2],
                scan->release_configuration[TESTER_END_J2])) {
            fail_scan(scan, TESTER_SCAN_ERROR_I2C_READ, now_ms);
            return;
        }
        if (!ports_are_all_inputs(scan->release_configuration[TESTER_END_J1]) ||
            !ports_are_all_inputs(scan->release_configuration[TESTER_END_J2])) {
            fail_scan(scan, TESTER_SCAN_ERROR_RELEASE_CONFIG_FAILED, now_ms);
            return;
        }
        scan->release_started_at_ms = now_ms;
        scan->release_check_count = 0u;
        scan->release_stable_count = 0u;
        scan->release_recovery_ms = 0u;
        scan->deadline_ms = now_ms + scan->config.release_settle_time_ms;
        scan->state = TESTER_SCAN_RELEASE_SETTLE;
        break;

    case TESTER_SCAN_RELEASE_SETTLE:
        if (deadline_reached(now_ms, scan->deadline_ms)) {
            scan->state = TESTER_SCAN_RELEASE_CHECK;
        }
        break;

    case TESTER_SCAN_RELEASE_CHECK:
        if (!pcal6524_read_inputs(
                &scan->end[TESTER_END_J1],
                scan->release_inputs[TESTER_END_J1]) ||
            !pcal6524_read_inputs(
                &scan->end[TESTER_END_J2],
                scan->release_inputs[TESTER_END_J2])) {
            fail_scan(scan, TESTER_SCAN_ERROR_I2C_READ, now_ms);
            return;
        }
        if (scan->release_check_count < UINT8_MAX) {
            ++scan->release_check_count;
        }
        scan->release_recovery_ms = (uint16_t)(now_ms - scan->release_started_at_ms);
        if (release_has_new_low(scan)) {
            scan->release_stable_count = 0u;
            if (scan->release_recovery_ms >= scan->config.release_timeout_ms) {
                fail_scan(scan, TESTER_SCAN_ERROR_DUT_SETTLE_TIMEOUT, now_ms);
                return;
            }
        } else if (++scan->release_stable_count >= 2u) {
            /* Do not publish a source row until its output is proven released. */
            commit_source_votes(scan);
            advance_source(scan, now_ms);
            return;
        }
        scan->deadline_ms = now_ms + scan->config.release_retry_interval_ms;
        scan->state = TESTER_SCAN_RELEASE_SETTLE;
        break;

    case TESTER_SCAN_UNINITIALIZED:
    case TESTER_SCAN_IDLE:
    case TESTER_SCAN_DONE:
    case TESTER_SCAN_ERROR:
    default:
        break;
    }
}

void tester_scan_abort(tester_scan_t *scan, uint32_t now_ms)
{
    if ((scan != NULL) && tester_scan_busy(scan)) {
        fail_scan(scan, TESTER_SCAN_ERROR_ABORTED, now_ms);
    }
}

void tester_scan_fail_external(
    tester_scan_t *scan,
    tester_scan_error_t error,
    uint32_t now_ms)
{
    if ((scan != NULL) && (error != TESTER_SCAN_ERROR_NONE)) {
        fail_scan(scan, error, now_ms);
    }
}

bool tester_scan_busy(const tester_scan_t *scan)
{
    if (scan == NULL) {
        return false;
    }
    return (scan->state == TESTER_SCAN_BREAK) || (scan->state == TESTER_SCAN_DRIVE) ||
           (scan->state == TESTER_SCAN_BASELINE_SETTLE) ||
           (scan->state == TESTER_SCAN_BASELINE_CHECK) ||
           (scan->state == TESTER_SCAN_SETTLE) || (scan->state == TESTER_SCAN_SAMPLE) ||
           (scan->state == TESTER_SCAN_RELEASE) ||
           (scan->state == TESTER_SCAN_RELEASE_SETTLE) ||
           (scan->state == TESTER_SCAN_RELEASE_CHECK);
}

bool tester_scan_finished(const tester_scan_t *scan)
{
    return (scan != NULL) && ((scan->state == TESTER_SCAN_DONE) || (scan->state == TESTER_SCAN_ERROR));
}

uint32_t tester_scan_duration_ms(const tester_scan_t *scan)
{
    if (scan == NULL) {
        return 0u;
    }
    return scan->finished_at_ms - scan->started_at_ms;
}

const char *tester_scan_error_name(tester_scan_error_t error)
{
    switch (error) {
    case TESTER_SCAN_ERROR_NONE:
        return "NONE";
    case TESTER_SCAN_ERROR_INVALID_CONFIG:
        return "INVALID_CONFIG";
    case TESTER_SCAN_ERROR_EXPANDER_INIT:
        return "EXPANDER_INIT";
    case TESTER_SCAN_ERROR_EXPANDER_CONFIG:
        return "EXPANDER_CONFIG";
    case TESTER_SCAN_ERROR_I2C_WRITE:
        return "I2C_WRITE";
    case TESTER_SCAN_ERROR_I2C_READ:
        return "I2C_READ";
    case TESTER_SCAN_ERROR_BASELINE_STUCK_LOW:
        return "BASELINE_STUCK_LOW";
    case TESTER_SCAN_ERROR_SOURCE_DRIVE_FAILED:
        return "SOURCE_DRIVE_FAILED";
    case TESTER_SCAN_ERROR_RELEASE_CONFIG_FAILED:
        return "RELEASE_CONFIG_FAILED";
    case TESTER_SCAN_ERROR_DUT_SETTLE_TIMEOUT:
        return "DUT_SETTLE_TIMEOUT";
    case TESTER_SCAN_ERROR_RELEASE_STUCK_LOW:
        return "RELEASE_STUCK_LOW";
    case TESTER_SCAN_ERROR_LED_OUTPUT:
        return "LED_OUTPUT";
    case TESTER_SCAN_ERROR_ABORTED:
        return "ABORTED";
    default:
        return "UNKNOWN";
    }
}
