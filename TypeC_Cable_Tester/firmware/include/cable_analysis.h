#ifndef CABLE_ANALYSIS_H
#define CABLE_ANALYSIS_H

#include "cable_profile.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CABLE_RESULT_NOT_TESTED = 0,
    CABLE_RESULT_NO_CONNECTION,
    CABLE_RESULT_ONE_END_ONLY,
    CABLE_RESULT_DISCOVERY,
    CABLE_RESULT_PASS,
    CABLE_RESULT_OPEN,
    CABLE_RESULT_SHORT_OR_MISWIRE,
    CABLE_RESULT_OPEN_AND_SHORT,
    CABLE_RESULT_UNSTABLE,
    CABLE_RESULT_POWER_CROSS_FAULT,
    CABLE_RESULT_POWER_CROSS_SUSPECT,
    CABLE_RESULT_HARDWARE_ERROR,
    CABLE_RESULT_CONDUCTORS_PASS_EMARKER_UNVERIFIED
} cable_result_code_t;

typedef struct {
    cable_result_code_t code;
    cable_kind_t kind;
    bool j1_flipped;
    bool j2_flipped;
    bool orientation_ambiguous;
    uint16_t missing_pair_count;
    uint16_t unexpected_pair_count;
    uint16_t short_pair_count;
    uint16_t miswire_pair_count;
    uint16_t unstable_pair_count;
    uint16_t asymmetric_pair_count;
    uint16_t temporal_unstable_pair_count;
    uint16_t emarker_electronic_path_pair_count;
    uint16_t power_cross_pair_count;
    /* Compatibility alias for the v0.3.3 bidirectional-only metric. */
    uint16_t confirmed_power_cross_pair_count;
    uint16_t power_cross_bidir_pair_count;
    uint16_t power_cross_gnd_source_to_vbus_pair_count;
    uint16_t power_cross_vbus_source_to_gnd_pair_count;
    uint16_t power_cross_temporal_pair_count;
    uint16_t confirmed_cross_pair_count;
    uint16_t confirmed_local_pair_count[TESTER_END_COUNT];
    uint8_t detected_end_mask;
    uint32_t score;
    uint8_t conductive_contact_bitmap[TESTER_CONTACT_BITMAP_BYTES];
    uint8_t fault_contact_bitmap[TESTER_CONTACT_BITMAP_BYTES];
    uint8_t open_endpoint_bitmap[TESTER_ENDPOINT_BITMAP_BYTES];
    uint8_t unexpected_endpoint_bitmap[TESTER_ENDPOINT_BITMAP_BYTES];
    uint8_t unstable_endpoint_bitmap[TESTER_ENDPOINT_BITMAP_BYTES];
    uint8_t power_cross_endpoint_bitmap[TESTER_ENDPOINT_BITMAP_BYTES];
} cable_analysis_result_t;

bool cable_analyze(
    const tester_observation_t *observation,
    const cable_profile_t *profile,
    cable_analysis_result_t *result);
bool cable_analyze_best_orientation(
    const tester_observation_t *observation,
    cable_kind_t kind,
    cable_analysis_result_t *result);
bool cable_analyze_auto(
    const tester_observation_t *observation,
    cable_analysis_result_t *result);
const char *cable_result_name(cable_result_code_t code);

#ifdef __cplusplus
}
#endif

#endif
