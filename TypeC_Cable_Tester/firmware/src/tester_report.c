#include "tester_report.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static bool sink_bytes(tester_report_sink_fn sink, void *context, const char *text, size_t length)
{
    return (sink != NULL) && sink(context, (const uint8_t *)text, length);
}

static bool sink_text(tester_report_sink_fn sink, void *context, const char *text)
{
    return (text != NULL) && sink_bytes(sink, context, text, strlen(text));
}

static bool sink_format(tester_report_sink_fn sink, void *context, const char *format, ...)
{
    char buffer[192];
    int length;
    va_list arguments;

    va_start(arguments, format);
    length = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    if ((length < 0) || ((size_t)length >= sizeof(buffer))) {
        return false;
    }
    return sink_bytes(sink, context, buffer, (size_t)length);
}

static bool write_endpoint_bitmap(
    tester_report_sink_fn sink,
    void *context,
    const char *label,
    const uint8_t bitmap[TESTER_ENDPOINT_BITMAP_BYTES])
{
    uint8_t endpoint;
    char name[12];
    bool first = true;

    if (!sink_format(sink, context, "%s=", label)) {
        return false;
    }
    for (endpoint = 0u; endpoint < TESTER_ENDPOINT_COUNT; ++endpoint) {
        if (!tester_bitmap_get(bitmap, endpoint)) {
            continue;
        }
        if (!sink_format(
                sink,
                context,
                "%s%s",
                first ? "" : ",",
                tester_endpoint_name(endpoint, name, sizeof(name)))) {
            return false;
        }
        first = false;
    }
    return sink_text(sink, context, first ? "NONE\r\n" : "\r\n");
}

static bool write_contact_bitmap(
    tester_report_sink_fn sink,
    void *context,
    const char *label,
    const uint8_t bitmap[TESTER_CONTACT_BITMAP_BYTES])
{
    uint8_t contact;
    bool first = true;

    if (!sink_format(sink, context, "%s=", label)) {
        return false;
    }
    for (contact = 0u; contact < TESTER_CONTACT_COUNT; ++contact) {
        if (!tester_bitmap_get(bitmap, contact)) {
            continue;
        }
        if (!sink_format(
                sink,
                context,
                "%s%s",
                first ? "" : ",",
                tester_contact_name((tester_contact_t)contact))) {
            return false;
        }
        first = false;
    }
    return sink_text(sink, context, first ? "NONE\r\n" : "\r\n");
}

static bool write_ports_hex(
    tester_report_sink_fn sink,
    void *context,
    const char *label,
    const uint8_t ports[PCAL6524_PORT_COUNT])
{
    return sink_format(
        sink,
        context,
        "%s=%02X%02X%02X\r\n",
        label,
        (unsigned int)ports[0],
        (unsigned int)ports[1],
        (unsigned int)ports[2]);
}

static bool write_matrix(
    tester_report_sink_fn sink,
    void *context,
    const char *label,
    const tester_matrix_t *matrix)
{
    uint8_t source;
    uint8_t target;
    char source_name[12];
    char target_name[12];
    bool first;

    if (!sink_format(sink, context, "%s_BEGIN\r\n", label)) {
        return false;
    }
    for (source = 0u; source < TESTER_ENDPOINT_COUNT; ++source) {
        if (!sink_format(
                sink,
                context,
                "%s=",
                tester_endpoint_name(source, source_name, sizeof(source_name)))) {
            return false;
        }
        first = true;
        for (target = 0u; target < TESTER_ENDPOINT_COUNT; ++target) {
            if (!tester_matrix_get(matrix, source, target)) {
                continue;
            }
            if (!sink_format(
                    sink,
                    context,
                    "%s%s",
                    first ? "" : ",",
                    tester_endpoint_name(target, target_name, sizeof(target_name)))) {
                return false;
            }
            first = false;
        }
        if (!sink_text(sink, context, first ? "NONE\r\n" : "\r\n")) {
            return false;
        }
    }
    return sink_format(sink, context, "%s_END\r\n", label);
}

bool tester_report_write(const tester_report_t *report, tester_report_sink_fn sink, void *sink_context)
{
    const cable_analysis_result_t *analysis;
    const tester_scan_t *scan;
    char source_name[12];

    if ((report == NULL) || (report->analysis == NULL) || (report->observation == NULL) ||
        (report->scan == NULL) || (sink == NULL)) {
        return false;
    }
    analysis = report->analysis;
    scan = report->scan;

    if (!sink_text(sink, sink_context, "TYPEC_TEST_REPORT_BEGIN\r\n") ||
        !sink_format(
            sink,
            sink_context,
            "FW=%s\r\nREQUESTED_PROFILE=%s\r\nDETECTED_PROFILE=%s\r\n",
            (report->firmware_version != NULL) ? report->firmware_version : "UNKNOWN",
            cable_kind_name(report->requested_kind),
            cable_kind_name(analysis->kind)) ||
        !sink_format(
            sink,
            sink_context,
            "PROFILE=%s\r\nRESULT=%s\r\nDURATION_MS=%lu\r\n",
            cable_kind_name(analysis->kind),
            cable_result_name(analysis->code),
            (unsigned long)report->duration_ms) ||
        !sink_format(
            sink,
            sink_context,
            "SCAN_ERROR=%s\r\n",
            tester_scan_error_name(report->scan_error)) ||
        !sink_format(
            sink,
            sink_context,
            "SCAN_COMPLETE=%u\r\nSCAN_PROGRESS=%u/%u\r\nSCAN_SOURCE=%s\r\nRELEASE_CHECKS=%u\r\nRELEASE_RECOVERY_MS=%u\r\n",
            scan->state == TESTER_SCAN_DONE ? 1u : 0u,
            (unsigned int)scan->completed_source_count,
            (unsigned int)TESTER_ENDPOINT_COUNT,
            tester_endpoint_name(scan->source_endpoint, source_name, sizeof(source_name)),
            (unsigned int)scan->release_check_count,
            (unsigned int)scan->release_recovery_ms) ||
        !write_ports_hex(
            sink,
            sink_context,
            "BASELINE_J1_RAW",
            scan->baseline_inputs[TESTER_END_J1]) ||
        !write_ports_hex(
            sink,
            sink_context,
            "BASELINE_J2_RAW",
            scan->baseline_inputs[TESTER_END_J2]) ||
        !write_ports_hex(
            sink,
            sink_context,
            "RELEASE_J1_RAW",
            scan->release_inputs[TESTER_END_J1]) ||
        !write_ports_hex(
            sink,
            sink_context,
            "RELEASE_J2_RAW",
            scan->release_inputs[TESTER_END_J2]) ||
        !write_ports_hex(
            sink,
            sink_context,
            "RELEASE_NEW_LOW_J1",
            scan->release_new_low[TESTER_END_J1]) ||
        !write_ports_hex(
            sink,
            sink_context,
            "RELEASE_NEW_LOW_J2",
            scan->release_new_low[TESTER_END_J2]) ||
        !write_ports_hex(
            sink,
            sink_context,
            "RELEASE_NEW_HIGH_J1",
            scan->release_new_high[TESTER_END_J1]) ||
        !write_ports_hex(
            sink,
            sink_context,
            "RELEASE_NEW_HIGH_J2",
            scan->release_new_high[TESTER_END_J2]) ||
        !write_ports_hex(
            sink,
            sink_context,
            "RELEASE_CONFIG_J1",
            scan->release_configuration[TESTER_END_J1]) ||
        !write_ports_hex(
            sink,
            sink_context,
            "RELEASE_CONFIG_J2",
            scan->release_configuration[TESTER_END_J2]) ||
        !sink_format(
            sink,
            sink_context,
            "J1_FLIPPED=%u\r\nJ2_FLIPPED=%u\r\nORIENTATION_AMBIGUOUS=%u\r\n",
            analysis->j1_flipped ? 1u : 0u,
            analysis->j2_flipped ? 1u : 0u,
            analysis->orientation_ambiguous ? 1u : 0u) ||
        !sink_format(
            sink,
            sink_context,
            "MISSING_PAIRS=%u\r\nUNEXPECTED_PAIRS=%u\r\nSHORT_PAIRS=%u\r\nMISWIRE_PAIRS=%u\r\nUNSTABLE_PAIRS=%u\r\nASYMMETRIC_PAIRS=%u\r\nTEMPORAL_UNSTABLE_PAIRS=%u\r\n",
            (unsigned int)analysis->missing_pair_count,
            (unsigned int)analysis->unexpected_pair_count,
            (unsigned int)analysis->short_pair_count,
            (unsigned int)analysis->miswire_pair_count,
            (unsigned int)analysis->unstable_pair_count,
            (unsigned int)analysis->asymmetric_pair_count,
            (unsigned int)analysis->temporal_unstable_pair_count) ||
        !sink_format(
            sink,
            sink_context,
            "EMARKER_ELECTRONIC_PATH_PAIRS=%u\r\n",
            (unsigned int)analysis->emarker_electronic_path_pair_count) ||
        !sink_format(
            sink,
            sink_context,
            "POWER_CROSS_PAIRS=%u\r\nPOWER_CROSS_BIDIR_PAIRS=%u\r\nPOWER_CROSS_GND_SOURCE_TO_VBUS_PAIRS=%u\r\n",
            (unsigned int)analysis->power_cross_pair_count,
            (unsigned int)analysis->power_cross_bidir_pair_count,
            (unsigned int)analysis->power_cross_gnd_source_to_vbus_pair_count) ||
        !sink_format(
            sink,
            sink_context,
            "POWER_CROSS_VBUS_SOURCE_TO_GND_PAIRS=%u\r\nPOWER_CROSS_TEMPORAL_PAIRS=%u\r\nSCORE=%lu\r\n",
            (unsigned int)analysis->power_cross_vbus_source_to_gnd_pair_count,
            (unsigned int)analysis->power_cross_temporal_pair_count,
            (unsigned long)analysis->score) ||
        !sink_format(
            sink,
            sink_context,
            "DETECTED_END_MASK=%u\r\nCONFIRMED_LOCAL_PAIRS_J1=%u\r\nCONFIRMED_LOCAL_PAIRS_J2=%u\r\nCONFIRMED_CROSS_PAIRS=%u\r\n",
            (unsigned int)analysis->detected_end_mask,
            (unsigned int)analysis->confirmed_local_pair_count[TESTER_END_J1],
            (unsigned int)analysis->confirmed_local_pair_count[TESTER_END_J2],
            (unsigned int)analysis->confirmed_cross_pair_count) ||
        !write_contact_bitmap(
            sink,
            sink_context,
            "CONDUCTIVE_CONTACTS",
            analysis->conductive_contact_bitmap) ||
        !write_contact_bitmap(sink, sink_context, "FAULT_CONTACTS", analysis->fault_contact_bitmap) ||
        !write_endpoint_bitmap(sink, sink_context, "OPEN_ENDPOINTS", analysis->open_endpoint_bitmap) ||
        !write_endpoint_bitmap(sink, sink_context, "UNEXPECTED_ENDPOINTS", analysis->unexpected_endpoint_bitmap) ||
        !write_endpoint_bitmap(sink, sink_context, "UNSTABLE_ENDPOINTS", analysis->unstable_endpoint_bitmap) ||
        !write_endpoint_bitmap(
            sink,
            sink_context,
            "POWER_CROSS_ENDPOINTS",
            analysis->power_cross_endpoint_bitmap) ||
        !write_matrix(sink, sink_context, "OBSERVED_LOW_MATRIX", &report->observation->low) ||
        !write_matrix(sink, sink_context, "UNSTABLE_SAMPLE_MATRIX", &report->observation->unstable) ||
        !sink_text(sink, sink_context, "TYPEC_TEST_REPORT_END\r\n")) {
        return false;
    }
    return true;
}
