#ifndef TESTER_REPORT_H
#define TESTER_REPORT_H

#include "cable_analysis.h"
#include "tester_scan.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*tester_report_sink_fn)(void *context, const uint8_t *data, size_t data_length);

typedef struct {
    const char *firmware_version;
    uint32_t duration_ms;
    tester_scan_error_t scan_error;
    cable_kind_t requested_kind;
    const tester_scan_t *scan;
    const tester_observation_t *observation;
    const cable_analysis_result_t *analysis;
} tester_report_t;

bool tester_report_write(const tester_report_t *report, tester_report_sink_fn sink, void *sink_context);

#ifdef __cplusplus
}
#endif

#endif
