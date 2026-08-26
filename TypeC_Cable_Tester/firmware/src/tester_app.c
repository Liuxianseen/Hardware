#include "tester_app.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define KEY_DEBOUNCE_MS 30u

static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static bool usb_sink(void *context, const uint8_t *data, size_t data_length)
{
    tester_app_t *app = (tester_app_t *)context;

    if ((app == NULL) || (app->platform == NULL) || (app->platform->usb_write == NULL)) {
        return false;
    }
    return app->platform->usb_write(app->platform->context, data, data_length);
}

static bool usb_text(tester_app_t *app, const char *text)
{
    return (text != NULL) && usb_sink(app, (const uint8_t *)text, strlen(text));
}

static bool usb_format(tester_app_t *app, const char *format, ...)
{
    char buffer[160];
    int length;
    va_list arguments;

    va_start(arguments, format);
    length = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    if ((length < 0) || ((size_t)length >= sizeof(buffer))) {
        return false;
    }
    return usb_sink(app, (const uint8_t *)buffer, (size_t)length);
}

static void set_status_leds(tester_app_t *app, bool pass_on, bool short_on, bool open_on)
{
    if ((app != NULL) && (app->platform != NULL) && (app->platform->set_status_leds != NULL)) {
        app->platform->set_status_leds(app->platform->context, pass_on, short_on, open_on);
    }
}

static void set_buzzer(tester_app_t *app, bool enabled)
{
    if ((app != NULL) && (app->platform != NULL) && (app->platform->set_buzzer != NULL)) {
        app->platform->set_buzzer(app->platform->context, enabled);
    }
}

static bool clear_operator_outputs(tester_app_t *app)
{
    static const uint8_t no_faults[PCAL6524_PORT_COUNT] = {0u, 0u, 0u};

    set_status_leds(app, false, false, false);
    set_buzzer(app, false);
    app->buzzer_segment_count = 0u;
    app->buzzer_segment_index = 0u;
    app->displayed_completed_source_count = 0u;
    return pcal6524_write_led_bitmap(&app->led_driver, no_faults);
}

void tester_app_build_scan_progress_bitmap(
    uint8_t completed_source_count,
    uint8_t bitmap[TESTER_CONTACT_BITMAP_BYTES])
{
    uint8_t contact;
    uint8_t j2_completed;

    if (bitmap == NULL) {
        return;
    }
    memset(bitmap, 0, TESTER_CONTACT_BITMAP_BYTES);
    if (completed_source_count > TESTER_ENDPOINT_COUNT) {
        completed_source_count = TESTER_ENDPOINT_COUNT;
    }
    if (completed_source_count <= TESTER_CONTACT_COUNT) {
        for (contact = 0u; contact < completed_source_count; ++contact) {
            tester_bitmap_set(bitmap, contact, true);
        }
        return;
    }

    j2_completed = (uint8_t)(completed_source_count - TESTER_CONTACT_COUNT);
    for (contact = j2_completed; contact < TESTER_CONTACT_COUNT; ++contact) {
        tester_bitmap_set(bitmap, contact, true);
    }
}

static bool update_scan_progress(tester_app_t *app, uint32_t now_ms)
{
    uint8_t progress_bitmap[TESTER_CONTACT_BITMAP_BYTES];
    uint8_t completed_source_count = app->scan.completed_source_count;

    if (completed_source_count <= app->displayed_completed_source_count) {
        return true;
    }
    tester_app_build_scan_progress_bitmap(completed_source_count, progress_bitmap);
    /* Latch before I2C so a NACK can never create a per-tick retry loop. */
    app->displayed_completed_source_count = completed_source_count;
    if (pcal6524_write_led_bitmap(&app->led_driver, progress_bitmap)) {
        return true;
    }
    app->led_output_fault = true;
    tester_scan_fail_external(&app->scan, TESTER_SCAN_ERROR_LED_OUTPUT, now_ms);
    return false;
}

static void add_buzzer_segment(tester_app_t *app, bool enabled, uint16_t duration_ms)
{
    uint8_t index = app->buzzer_segment_count;

    if ((duration_ms == 0u) || (index >= TESTER_BUZZER_SEGMENT_CAPACITY)) {
        return;
    }
    app->buzzer_segments[index].enabled = enabled;
    app->buzzer_segments[index].duration_ms = duration_ms;
    app->buzzer_segment_count = (uint8_t)(index + 1u);
}

static void add_short_pattern(tester_app_t *app)
{
    uint8_t pulse;

    for (pulse = 0u; pulse < 3u; ++pulse) {
        add_buzzer_segment(app, true, 150u);
        if (pulse < 2u) {
            add_buzzer_segment(app, false, 150u);
        }
    }
}

static void add_open_pattern(tester_app_t *app)
{
    add_buzzer_segment(app, true, 300u);
    add_buzzer_segment(app, false, 200u);
    add_buzzer_segment(app, true, 300u);
}

static void start_buzzer_pattern(tester_app_t *app, uint32_t now_ms)
{
    bool has_short;
    bool has_open;
    uint8_t pulse;

    app->buzzer_segment_count = 0u;
    app->buzzer_segment_index = 0u;
    if (app->last_result.code == CABLE_RESULT_NO_CONNECTION) {
        set_buzzer(app, false);
        return;
    }
    has_short = (app->last_result.unexpected_pair_count != 0u) ||
                (app->last_result.code == CABLE_RESULT_POWER_CROSS_FAULT);
    has_open = app->last_result.missing_pair_count != 0u;

    if ((app->last_result.code == CABLE_RESULT_UNSTABLE) ||
        (app->last_result.code == CABLE_RESULT_POWER_CROSS_SUSPECT) ||
        (app->last_result.code == CABLE_RESULT_HARDWARE_ERROR)) {
        for (pulse = 0u; pulse < 5u; ++pulse) {
            add_buzzer_segment(app, true, 100u);
            if (pulse < 4u) {
                add_buzzer_segment(app, false, 100u);
            }
        }
    } else {
        if (has_short) {
            add_short_pattern(app);
        }
        if (has_short && has_open) {
            add_buzzer_segment(app, false, 500u);
        }
        if (has_open) {
            add_open_pattern(app);
        }
    }

    if (app->buzzer_segment_count == 0u) {
        set_buzzer(app, false);
        return;
    }
    set_buzzer(app, app->buzzer_segments[0].enabled);
    app->buzzer_deadline_ms = now_ms + app->buzzer_segments[0].duration_ms;
}

static void tick_buzzer(tester_app_t *app, uint32_t now_ms)
{
    if ((app->buzzer_segment_count == 0u) || !deadline_reached(now_ms, app->buzzer_deadline_ms)) {
        return;
    }
    ++app->buzzer_segment_index;
    if (app->buzzer_segment_index >= app->buzzer_segment_count) {
        app->buzzer_segment_count = 0u;
        set_buzzer(app, false);
        return;
    }
    set_buzzer(app, app->buzzer_segments[app->buzzer_segment_index].enabled);
    app->buzzer_deadline_ms = now_ms + app->buzzer_segments[app->buzzer_segment_index].duration_ms;
}

static bool apply_result_outputs(tester_app_t *app)
{
    static const uint8_t no_signals[PCAL6524_PORT_COUNT] = {0u, 0u, 0u};
    const uint8_t *signal_bitmap = app->last_result.conductive_contact_bitmap;
    bool pass_on = false;
    bool short_on = false;
    bool open_on = false;

    if (app->last_result.code == CABLE_RESULT_PASS) {
        pass_on = true;
    } else if (app->last_result.code == CABLE_RESULT_CONDUCTORS_PASS_EMARKER_UNVERIFIED) {
        /* Conductors passed. This hardware cannot verify the eMarker over USB-PD. */
        pass_on = true;
    } else if (app->last_result.code == CABLE_RESULT_HARDWARE_ERROR) {
        /* The scan is invalid: use the buzzer/report without fabricating cable faults. */
    } else if (app->last_result.code == CABLE_RESULT_UNSTABLE) {
        /* Temporal/asymmetric evidence is not a confirmed short or open. */
    } else if (app->last_result.code == CABLE_RESULT_POWER_CROSS_FAULT) {
        short_on = true;
        open_on = app->last_result.missing_pair_count != 0u;
    } else if (app->last_result.code == CABLE_RESULT_POWER_CROSS_SUSPECT) {
        /* Directional/temporal evidence is an alarm, not a confirmed short/open. */
    } else if (app->last_result.code == CABLE_RESULT_NO_CONNECTION) {
        /* An empty fixture is an idle condition: all status outputs remain off. */
    } else if (app->last_result.code == CABLE_RESULT_ONE_END_ONLY) {
        open_on = app->last_result.missing_pair_count != 0u;
    } else {
        short_on = app->last_result.unexpected_pair_count != 0u;
        open_on = app->last_result.missing_pair_count != 0u;
    }

    /* Only an invalid/incomplete hardware scan suppresses verified conductors. */
    if (app->last_result.code == CABLE_RESULT_HARDWARE_ERROR) {
        signal_bitmap = no_signals;
    }

    set_status_leds(app, pass_on, short_on, open_on);
    return pcal6524_write_led_bitmap(&app->led_driver, signal_bitmap);
}

static void write_last_report(tester_app_t *app)
{
    tester_report_t report;

    if ((app == NULL) || !app->has_result || (app->platform->usb_write == NULL)) {
        return;
    }
    report.firmware_version = TESTER_FIRMWARE_VERSION;
    report.duration_ms = tester_scan_duration_ms(&app->scan);
    report.scan_error = app->scan.error;
    report.requested_kind = app->last_requested_kind;
    report.scan = &app->scan;
    report.observation = &app->scan.observation;
    report.analysis = &app->last_result;
    (void)tester_report_write(&report, usb_sink, app);
}

static void mark_settle_timeout_faults(tester_app_t *app)
{
    uint8_t end;
    uint8_t pin;
    uint8_t port;
    uint8_t bit;
    tester_contact_t contact;
    uint8_t endpoint;

    for (end = 0u; end < TESTER_END_COUNT; ++end) {
        for (pin = 0u; pin < PCAL6524_PIN_COUNT; ++pin) {
            port = (uint8_t)(pin >> 3u);
            bit = (uint8_t)(pin & 7u);
            if ((app->scan.release_new_low[end][port] & (uint8_t)(1u << bit)) == 0u) {
                continue;
            }
            contact = (tester_contact_t)app->scan.pin_to_contact[end][pin];
            endpoint = tester_endpoint((tester_end_t)end, contact);
            tester_bitmap_set(app->last_result.unstable_endpoint_bitmap, endpoint, true);
            tester_bitmap_set(app->last_result.fault_contact_bitmap, (uint8_t)contact, true);
        }
    }
    app->last_result.unstable_pair_count = 1u;
    app->last_result.score = 50u;
}

static void complete_scan(tester_app_t *app, uint32_t now_ms)
{
    memset(&app->last_result, 0, sizeof(app->last_result));
    app->last_result.kind = (app->active_requested_kind == CABLE_KIND_AUTO)
                                ? CABLE_KIND_COUNT
                                : app->active_requested_kind;

    if ((app->scan.state == TESTER_SCAN_ERROR) &&
        (app->scan.error == TESTER_SCAN_ERROR_DUT_SETTLE_TIMEOUT)) {
        app->last_result.code = CABLE_RESULT_UNSTABLE;
        mark_settle_timeout_faults(app);
        app->state = TESTER_APP_RESULT;
    } else if (app->scan.state == TESTER_SCAN_ERROR) {
        app->last_result.code = CABLE_RESULT_HARDWARE_ERROR;
        memset(app->last_result.fault_contact_bitmap, 0xFF, TESTER_CONTACT_BITMAP_BYTES);
        app->state = TESTER_APP_HARDWARE_ERROR;
    } else if (((app->active_requested_kind == CABLE_KIND_AUTO) &&
                !cable_analyze_auto(&app->scan.observation, &app->last_result)) ||
               ((app->active_requested_kind != CABLE_KIND_AUTO) &&
                !cable_analyze_best_orientation(
                    &app->scan.observation,
                    app->active_requested_kind,
                    &app->last_result))) {
        app->last_result.code = CABLE_RESULT_HARDWARE_ERROR;
        memset(app->last_result.fault_contact_bitmap, 0xFF, TESTER_CONTACT_BITMAP_BYTES);
        app->state = TESTER_APP_HARDWARE_ERROR;
    } else {
        app->state = TESTER_APP_RESULT;
    }

    app->has_result = true;
    if (!apply_result_outputs(app)) {
        app->led_output_fault = true;
        if (app->scan.error == TESTER_SCAN_ERROR_NONE) {
            tester_scan_fail_external(&app->scan, TESTER_SCAN_ERROR_LED_OUTPUT, now_ms);
        }
        app->last_result.code = CABLE_RESULT_HARDWARE_ERROR;
        memset(app->last_result.fault_contact_bitmap, 0xFF, TESTER_CONTACT_BITMAP_BYTES);
        app->state = TESTER_APP_HARDWARE_ERROR;
        set_status_leds(app, false, false, false);
    }
    start_buzzer_pattern(app, now_ms);
    write_last_report(app);
}

static bool request_start_with_kind(tester_app_t *app, cable_kind_t requested_kind, uint32_t now_ms)
{
    if ((app == NULL) || (app->state == TESTER_APP_SCANNING) ||
        ((uint8_t)requested_kind >= (uint8_t)CABLE_KIND_COUNT)) {
        return false;
    }
    app->active_requested_kind = requested_kind;
    app->last_requested_kind = requested_kind;
    app->led_output_fault = false;
    if (!clear_operator_outputs(app)) {
        app->led_output_fault = true;
        tester_scan_fail_external(&app->scan, TESTER_SCAN_ERROR_LED_OUTPUT, now_ms);
        complete_scan(app, now_ms);
        return false;
    }
    if (!tester_scan_start(&app->scan, now_ms)) {
        complete_scan(app, now_ms);
        return false;
    }
    app->state = TESTER_APP_SCANNING;
    return true;
}

static void update_key(tester_app_t *app, uint32_t now_ms, bool pressed)
{
    if (pressed != app->key_raw_pressed) {
        app->key_raw_pressed = pressed;
        app->key_changed_at_ms = now_ms;
    }
    if ((pressed != app->key_debounced_pressed) &&
        deadline_reached(now_ms, app->key_changed_at_ms + KEY_DEBOUNCE_MS)) {
        app->key_debounced_pressed = pressed;
        if (pressed) {
            /* The physical production workflow is always automatic. */
            (void)request_start_with_kind(app, CABLE_KIND_AUTO, now_ms);
        }
    }
}

bool tester_app_init(tester_app_t *app, const tester_platform_t *platform, uint32_t now_ms)
{
    return tester_app_init_with_scan_config(app, platform, now_ms, NULL);
}

bool tester_app_init_with_scan_config(
    tester_app_t *app,
    const tester_platform_t *platform,
    uint32_t now_ms,
    const tester_scan_config_t *scan_config)
{
    if ((app == NULL) || (platform == NULL) || (platform->i2c_write == NULL) ||
        (platform->i2c_read == NULL)) {
        return false;
    }

    memset(app, 0, sizeof(*app));
    app->platform = platform;
    app->selected_kind = CABLE_KIND_AUTO;
    app->active_requested_kind = CABLE_KIND_AUTO;
    app->last_requested_kind = CABLE_KIND_AUTO;
    app->key_changed_at_ms = now_ms;

    set_status_leds(app, false, false, false);
    set_buzzer(app, false);
    pcal6524_bind(&app->led_driver, platform, TESTER_I2C_ADDRESS_LED);

    if (!pcal6524_probe(&app->led_driver) || !pcal6524_init_led_outputs(&app->led_driver) ||
        !tester_scan_init(
            &app->scan,
            platform,
            TESTER_I2C_ADDRESS_END_J1,
            TESTER_I2C_ADDRESS_END_J2,
            scan_config)) {
        app->state = TESTER_APP_HARDWARE_ERROR;
        app->last_result.code = CABLE_RESULT_HARDWARE_ERROR;
        app->last_result.kind = app->selected_kind;
        memset(app->last_result.fault_contact_bitmap, 0xFF, TESTER_CONTACT_BITMAP_BYTES);
        app->has_result = true;
        (void)apply_result_outputs(app);
        start_buzzer_pattern(app, now_ms);
        return false;
    }

    app->state = TESTER_APP_IDLE;
    if (!clear_operator_outputs(app)) {
        app->led_output_fault = true;
        tester_scan_fail_external(&app->scan, TESTER_SCAN_ERROR_LED_OUTPUT, now_ms);
        complete_scan(app, now_ms);
        return false;
    }
    return true;
}

void tester_app_tick(tester_app_t *app, uint32_t now_ms, bool start_key_pressed)
{
    if ((app == NULL) || (app->platform == NULL)) {
        return;
    }

    update_key(app, now_ms, start_key_pressed);
    tick_buzzer(app, now_ms);

    if (app->state == TESTER_APP_SCANNING) {
        tester_scan_tick(&app->scan, now_ms);
        if (!update_scan_progress(app, now_ms)) {
            complete_scan(app, now_ms);
            return;
        }
        if (tester_scan_finished(&app->scan)) {
            complete_scan(app, now_ms);
        }
    }
}

bool tester_app_request_start(tester_app_t *app, uint32_t now_ms)
{
    return (app != NULL) && request_start_with_kind(app, app->selected_kind, now_ms);
}

void tester_app_handle_command(tester_app_t *app, const char *command_line, uint32_t now_ms)
{
    char local[96];
    char *command;
    char *argument;
    char *value;
    char source_name[12];
    cable_kind_t kind;
    size_t length;

    if ((app == NULL) || (command_line == NULL)) {
        return;
    }
    length = strlen(command_line);
    if (length >= sizeof(local)) {
        (void)usb_text(app, "ERR COMMAND_TOO_LONG\r\n");
        return;
    }
    memcpy(local, command_line, length + 1u);
    command = strtok(local, " \t\r\n");
    argument = strtok(NULL, " \t\r\n");
    value = strtok(NULL, " \t\r\n");

    if (command == NULL) {
        return;
    }
    if (cable_kind_parse(command, &kind)) {
        app->selected_kind = kind;
        (void)usb_format(app, "OK PROFILE %s\r\n", cable_kind_name(kind));
    } else if (strcmp(command, "START") == 0) {
        if (argument != NULL) {
            if (!cable_kind_parse(argument, &kind)) {
                (void)usb_text(app, "ERR UNKNOWN_PROFILE\r\n");
                return;
            }
            app->selected_kind = kind;
        }
        (void)usb_text(app, tester_app_request_start(app, now_ms) ? "OK STARTED\r\n" : "ERR START_FAILED\r\n");
    } else if (strcmp(command, "PROFILE") == 0) {
        if ((argument != NULL) && (strcmp(argument, "LIST") == 0)) {
            (void)usb_text(app, "AUTO DISCOVERY USB2_UNMARKED USB2_EMARKED FULL_UNMARKED FULL_EMARKED STRAIGHT24\r\n");
        } else if ((argument != NULL) && (strcmp(argument, "SET") == 0) && (value != NULL) &&
                   cable_kind_parse(value, &kind)) {
            app->selected_kind = kind;
            (void)usb_format(app, "OK PROFILE %s\r\n", cable_kind_name(kind));
        } else if (argument == NULL) {
            (void)usb_format(app, "PROFILE %s\r\n", cable_kind_name(app->selected_kind));
        } else {
            (void)usb_text(app, "ERR PROFILE_SYNTAX\r\n");
        }
    } else if (strcmp(command, "STATUS") == 0) {
        if (app->state == TESTER_APP_SCANNING) {
            (void)usb_format(
                app,
                "STATE=SCANNING PROFILE=%s DISPLAY_MODE=PROGRESS\r\n",
                cable_kind_name(app->active_requested_kind));
            (void)usb_format(
                app,
                "SCAN_PROGRESS=%u/%u SCAN_SOURCE=%s\r\n",
                (unsigned int)app->scan.completed_source_count,
                (unsigned int)TESTER_ENDPOINT_COUNT,
                tester_endpoint_name(app->scan.source_endpoint, source_name, sizeof(source_name)));
        } else {
            (void)usb_format(
                app,
                "STATE=%s PROFILE=%s DISPLAY_MODE=RESULT\r\n",
                tester_app_state_name(app->state),
                cable_kind_name(app->selected_kind));
            (void)usb_format(
                app,
                "RESULT=%s DETECTED_PROFILE=%s\r\n",
                app->has_result ? cable_result_name(app->last_result.code) : "NONE",
                app->has_result ? cable_kind_name(app->last_result.kind) : "NONE");
        }
    } else if (strcmp(command, "REPORT") == 0) {
        if (app->state == TESTER_APP_SCANNING) {
            (void)usb_text(app, "ERR SCANNING\r\n");
        } else if (app->has_result) {
            write_last_report(app);
        } else {
            (void)usb_text(app, "ERR NO_REPORT\r\n");
        }
    } else if (strcmp(command, "ABORT") == 0) {
        if (app->state == TESTER_APP_SCANNING) {
            tester_scan_abort(&app->scan, now_ms);
            (void)usb_text(app, "OK ABORTED\r\n");
        } else {
            (void)usb_text(app, "OK IDLE\r\n");
        }
    } else if (strcmp(command, "VERSION") == 0) {
        (void)usb_format(app, "FW %s\r\n", TESTER_FIRMWARE_VERSION);
    } else if (strcmp(command, "HELP") == 0) {
        (void)usb_text(app, "START [profile] | PROFILE [LIST|SET name] | STATUS | REPORT | ABORT | VERSION | HELP\r\n");
    } else {
        (void)usb_text(app, "ERR UNKNOWN_COMMAND\r\n");
    }
}

const char *tester_app_state_name(tester_app_state_t state)
{
    switch (state) {
    case TESTER_APP_UNINITIALIZED:
        return "UNINITIALIZED";
    case TESTER_APP_IDLE:
        return "IDLE";
    case TESTER_APP_SCANNING:
        return "SCANNING";
    case TESTER_APP_RESULT:
        return "RESULT";
    case TESTER_APP_HARDWARE_ERROR:
        return "HARDWARE_ERROR";
    default:
        return "UNKNOWN";
    }
}
