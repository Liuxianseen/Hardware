#ifndef TESTER_APP_H
#define TESTER_APP_H

#include "tester_report.h"
#include "tester_scan.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(TYPEC_TESTER_OFFICE_SILENT) && TYPEC_TESTER_OFFICE_SILENT
#define TESTER_FIRMWARE_VERSION "0.3.6-OFFICE-SILENT"
#else
#define TESTER_FIRMWARE_VERSION "0.3.6"
#endif
#define TESTER_I2C_ADDRESS_LED 0x20u
#define TESTER_I2C_ADDRESS_END_J1 0x22u
#define TESTER_I2C_ADDRESS_END_J2 0x23u
#define TESTER_BUZZER_SEGMENT_CAPACITY 16u

typedef enum {
    TESTER_APP_UNINITIALIZED = 0,
    TESTER_APP_IDLE,
    TESTER_APP_SCANNING,
    TESTER_APP_RESULT,
    TESTER_APP_HARDWARE_ERROR
} tester_app_state_t;

typedef struct {
    uint16_t duration_ms;
    bool enabled;
} tester_buzzer_segment_t;

typedef struct {
    const tester_platform_t *platform;
    tester_app_state_t state;
    tester_scan_t scan;
    pcal6524_t led_driver;
    cable_kind_t selected_kind;
    cable_kind_t active_requested_kind;
    cable_kind_t last_requested_kind;
    uint8_t displayed_completed_source_count;
    bool led_output_fault;
    cable_analysis_result_t last_result;
    bool has_result;
    bool key_raw_pressed;
    bool key_debounced_pressed;
    uint32_t key_changed_at_ms;
    tester_buzzer_segment_t buzzer_segments[TESTER_BUZZER_SEGMENT_CAPACITY];
    uint8_t buzzer_segment_count;
    uint8_t buzzer_segment_index;
    uint32_t buzzer_deadline_ms;
} tester_app_t;

bool tester_app_init(tester_app_t *app, const tester_platform_t *platform, uint32_t now_ms);
bool tester_app_init_with_scan_config(
    tester_app_t *app,
    const tester_platform_t *platform,
    uint32_t now_ms,
    const tester_scan_config_t *scan_config);
void tester_app_tick(tester_app_t *app, uint32_t now_ms, bool start_key_pressed);
bool tester_app_request_start(tester_app_t *app, uint32_t now_ms);
void tester_app_build_scan_progress_bitmap(
    uint8_t completed_source_count,
    uint8_t bitmap[TESTER_CONTACT_BITMAP_BYTES]);
void tester_app_handle_command(tester_app_t *app, const char *command_line, uint32_t now_ms);
const char *tester_app_state_name(tester_app_state_t state);

#ifdef __cplusplus
}
#endif

#endif
