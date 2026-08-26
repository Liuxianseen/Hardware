#ifndef TESTER_SCAN_H
#define TESTER_SCAN_H

#include "pcal6524.h"
#include "tester_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TESTER_SCAN_UNINITIALIZED = 0,
    TESTER_SCAN_IDLE,
    TESTER_SCAN_BASELINE_SETTLE,
    TESTER_SCAN_BASELINE_CHECK,
    TESTER_SCAN_BREAK,
    TESTER_SCAN_DRIVE,
    TESTER_SCAN_SETTLE,
    TESTER_SCAN_SAMPLE,
    TESTER_SCAN_RELEASE,
    TESTER_SCAN_RELEASE_SETTLE,
    TESTER_SCAN_RELEASE_CHECK,
    TESTER_SCAN_DONE,
    TESTER_SCAN_ERROR
} tester_scan_state_t;

typedef enum {
    TESTER_SCAN_ERROR_NONE = 0,
    TESTER_SCAN_ERROR_INVALID_CONFIG,
    TESTER_SCAN_ERROR_EXPANDER_INIT,
    TESTER_SCAN_ERROR_EXPANDER_CONFIG,
    TESTER_SCAN_ERROR_I2C_WRITE,
    TESTER_SCAN_ERROR_I2C_READ,
    TESTER_SCAN_ERROR_BASELINE_STUCK_LOW,
    TESTER_SCAN_ERROR_SOURCE_DRIVE_FAILED,
    TESTER_SCAN_ERROR_RELEASE_CONFIG_FAILED,
    TESTER_SCAN_ERROR_DUT_SETTLE_TIMEOUT,
    TESTER_SCAN_ERROR_RELEASE_STUCK_LOW,
    TESTER_SCAN_ERROR_LED_OUTPUT,
    TESTER_SCAN_ERROR_ABORTED
} tester_scan_error_t;

typedef struct {
    uint8_t samples_per_source;
    uint8_t vote_threshold;
    uint16_t settle_time_ms;
    uint16_t power_settle_time_ms;
    uint16_t sample_interval_ms;
    uint16_t release_settle_time_ms;
    uint16_t release_retry_interval_ms;
    uint16_t release_timeout_ms;
    uint8_t contact_to_pcal_pin[TESTER_END_COUNT][TESTER_CONTACT_COUNT];
} tester_scan_config_t;

typedef struct {
    pcal6524_t end[TESTER_END_COUNT];
    tester_scan_config_t config;
    tester_scan_state_t state;
    tester_scan_error_t error;
    tester_observation_t observation;
    uint8_t source_endpoint;
    uint8_t completed_source_count;
    uint8_t sample_index;
    uint8_t source_low_votes;
    uint8_t release_check_count;
    uint8_t release_stable_count;
    uint8_t vote_count[TESTER_ENDPOINT_COUNT];
    uint8_t baseline_inputs[TESTER_END_COUNT][PCAL6524_PORT_COUNT];
    uint8_t release_inputs[TESTER_END_COUNT][PCAL6524_PORT_COUNT];
    uint8_t release_new_low[TESTER_END_COUNT][PCAL6524_PORT_COUNT];
    uint8_t release_new_high[TESTER_END_COUNT][PCAL6524_PORT_COUNT];
    uint8_t release_configuration[TESTER_END_COUNT][PCAL6524_PORT_COUNT];
    uint8_t pin_to_contact[TESTER_END_COUNT][TESTER_CONTACT_COUNT];
    uint32_t deadline_ms;
    uint32_t release_started_at_ms;
    uint16_t release_recovery_ms;
    uint32_t started_at_ms;
    uint32_t finished_at_ms;
} tester_scan_t;

tester_scan_config_t tester_scan_default_config(void);
bool tester_scan_init(
    tester_scan_t *scan,
    const tester_platform_t *platform,
    uint8_t end_j1_address,
    uint8_t end_j2_address,
    const tester_scan_config_t *config);
bool tester_scan_start(tester_scan_t *scan, uint32_t now_ms);
void tester_scan_tick(tester_scan_t *scan, uint32_t now_ms);
void tester_scan_abort(tester_scan_t *scan, uint32_t now_ms);
void tester_scan_fail_external(
    tester_scan_t *scan,
    tester_scan_error_t error,
    uint32_t now_ms);
bool tester_scan_busy(const tester_scan_t *scan);
bool tester_scan_finished(const tester_scan_t *scan);
uint32_t tester_scan_duration_ms(const tester_scan_t *scan);
const char *tester_scan_error_name(tester_scan_error_t error);

#ifdef __cplusplus
}
#endif

#endif
