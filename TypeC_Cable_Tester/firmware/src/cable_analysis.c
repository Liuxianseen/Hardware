#include "cable_analysis.h"

#include <limits.h>
#include <string.h>

static void mark_endpoint_and_contact(uint8_t *endpoint_bitmap, uint8_t *contact_bitmap, uint8_t endpoint)
{
    tester_bitmap_set(endpoint_bitmap, endpoint, true);
    tester_bitmap_set(contact_bitmap, (uint8_t)tester_endpoint_contact(endpoint), true);
}

static tester_contact_t logical_contact(tester_contact_t physical_contact, bool end_flipped)
{
    uint8_t contact = (uint8_t)physical_contact;

    if (!end_flipped) {
        return physical_contact;
    }
    return (tester_contact_t)((contact < 12u) ? (contact + 12u) : (contact - 12u));
}

static bool contact_is_ground(tester_contact_t contact)
{
    return (contact == TESTER_CONTACT_A1) || (contact == TESTER_CONTACT_A12) ||
           (contact == TESTER_CONTACT_B1) || (contact == TESTER_CONTACT_B12);
}

static bool contact_is_vbus(tester_contact_t contact)
{
    return (contact == TESTER_CONTACT_A4) || (contact == TESTER_CONTACT_A9) ||
           (contact == TESTER_CONTACT_B4) || (contact == TESTER_CONTACT_B9);
}

static bool contacts_cross_power_rails(tester_contact_t first, tester_contact_t second)
{
    return (contact_is_ground(first) && contact_is_vbus(second)) ||
           (contact_is_vbus(first) && contact_is_ground(second));
}

static bool kind_has_emarker_electronics(cable_kind_t kind)
{
    return (kind == CABLE_KIND_USB2_EMARKED) || (kind == CABLE_KIND_FULL_UNMARKED) ||
           (kind == CABLE_KIND_FULL_EMARKED);
}

static bool contact_is_cc_or_vconn(tester_contact_t contact)
{
    return (contact == TESTER_CONTACT_A5) || (contact == TESTER_CONTACT_B5);
}

static bool contacts_form_emarker_electronic_path(tester_contact_t first, tester_contact_t second)
{
    return (contact_is_cc_or_vconn(first) && contact_is_ground(second)) ||
           (contact_is_ground(first) && contact_is_cc_or_vconn(second)) ||
           (contact_is_cc_or_vconn(first) && contact_is_cc_or_vconn(second));
}

static cable_result_code_t select_result_code(
    cable_kind_t kind,
    uint16_t missing,
    uint16_t unexpected,
    uint16_t unstable,
    uint16_t power_cross_fault,
    uint16_t power_cross_suspect,
    uint16_t confirmed_cross,
    uint8_t detected_end_mask)
{
    /* Power-rail isolation is profile-independent and always outranks topology. */
    if (power_cross_fault != 0u) {
        return CABLE_RESULT_POWER_CROSS_FAULT;
    }
    if (power_cross_suspect != 0u) {
        return CABLE_RESULT_POWER_CROSS_SUSPECT;
    }
    if ((missing != 0u) && (unexpected != 0u)) {
        return CABLE_RESULT_OPEN_AND_SHORT;
    }
    if (unexpected != 0u) {
        return CABLE_RESULT_SHORT_OR_MISWIRE;
    }
    if (unstable != 0u) {
        return CABLE_RESULT_UNSTABLE;
    }
    if (kind == CABLE_KIND_DISCOVERY) {
        return CABLE_RESULT_DISCOVERY;
    }
    if (confirmed_cross == 0u) {
        if (detected_end_mask == 0u) {
            return CABLE_RESULT_NO_CONNECTION;
        }
        if ((detected_end_mask == (1u << TESTER_END_J1)) ||
            (detected_end_mask == (1u << TESTER_END_J2))) {
            return CABLE_RESULT_ONE_END_ONLY;
        }
    }
    if (missing != 0u) {
        return CABLE_RESULT_OPEN;
    }
    if (confirmed_cross == 0u) {
        /* Never manufacture OPEN or PASS without either a missing pair or a conductor. */
        return CABLE_RESULT_NOT_TESTED;
    }
    if ((kind == CABLE_KIND_USB2_EMARKED) || (kind == CABLE_KIND_FULL_UNMARKED) ||
        (kind == CABLE_KIND_FULL_EMARKED)) {
        return CABLE_RESULT_CONDUCTORS_PASS_EMARKER_UNVERIFIED;
    }
    return CABLE_RESULT_PASS;
}

bool cable_analyze(
    const tester_observation_t *observation,
    const cable_profile_t *profile,
    cable_analysis_result_t *result)
{
    uint8_t first;
    uint8_t second;
    bool forward;
    bool reverse;
    bool forward_temporal;
    bool reverse_temporal;
    bool confirmed;
    bool unstable;
    bool asymmetric;
    bool temporal_unstable;
    bool required;
    bool allowed;
    bool optional_electronic_path;
    bool emarker_electronic_topology;
    bool emarker_electronic_path;
    bool is_power_cross;
    bool gnd_source_to_vbus;
    bool vbus_source_to_gnd;
    tester_end_t first_end;
    tester_end_t second_end;
    tester_contact_t first_contact;
    tester_contact_t second_contact;
    tester_contact_t first_physical_contact;
    tester_contact_t second_physical_contact;
    uint8_t confirmed_degree[TESTER_ENDPOINT_COUNT] = {0u};
    uint8_t required_degree[TESTER_ENDPOINT_COUNT] = {0u};

    if ((observation == NULL) || (profile == NULL) || (result == NULL)) {
        return false;
    }

    memset(result, 0, sizeof(*result));
    result->kind = profile->kind;
    result->j1_flipped = profile->j1_flipped;
    result->j2_flipped = profile->j2_flipped;

    for (first = 0u; first < TESTER_ENDPOINT_COUNT; ++first) {
        for (second = (uint8_t)(first + 1u); second < TESTER_ENDPOINT_COUNT; ++second) {
            forward = tester_matrix_get(&observation->low, first, second);
            reverse = tester_matrix_get(&observation->low, second, first);
            required = tester_matrix_get(&profile->required, first, second) ||
                       tester_matrix_get(&profile->required, second, first);
            if (forward && reverse) {
                ++confirmed_degree[first];
                ++confirmed_degree[second];
            }
            if (required) {
                ++required_degree[first];
                ++required_degree[second];
            }
        }
    }

    for (first = 0u; first < TESTER_ENDPOINT_COUNT; ++first) {
        for (second = (uint8_t)(first + 1u); second < TESTER_ENDPOINT_COUNT; ++second) {
            forward = tester_matrix_get(&observation->low, first, second);
            reverse = tester_matrix_get(&observation->low, second, first);
            forward_temporal = tester_matrix_get(&observation->unstable, first, second);
            reverse_temporal = tester_matrix_get(&observation->unstable, second, first);
            confirmed = forward && reverse;
            required = tester_matrix_get(&profile->required, first, second) ||
                       tester_matrix_get(&profile->required, second, first);
            allowed = tester_matrix_get(&profile->allowed, first, second) ||
                      tester_matrix_get(&profile->allowed, second, first);
            asymmetric = forward != reverse;
            temporal_unstable = forward_temporal || reverse_temporal;

            first_end = tester_endpoint_end(first);
            second_end = tester_endpoint_end(second);
            first_physical_contact = tester_endpoint_contact(first);
            second_physical_contact = tester_endpoint_contact(second);
            first_contact = logical_contact(
                first_physical_contact,
                (first_end == TESTER_END_J1) ? profile->j1_flipped : profile->j2_flipped);
            second_contact = logical_contact(
                second_physical_contact,
                (second_end == TESTER_END_J1) ? profile->j1_flipped : profile->j2_flipped);
            emarker_electronic_topology = kind_has_emarker_electronics(profile->kind) && !required &&
                                         contacts_form_emarker_electronic_path(
                                             first_contact,
                                             second_contact);
            emarker_electronic_path = emarker_electronic_topology && !temporal_unstable &&
                                       (asymmetric || (confirmed && allowed));
            optional_electronic_path = emarker_electronic_topology && asymmetric &&
                                       !temporal_unstable;
            unstable = (asymmetric && !optional_electronic_path) || temporal_unstable;

            if (asymmetric) {
                ++result->asymmetric_pair_count;
            }
            if (temporal_unstable) {
                ++result->temporal_unstable_pair_count;
            }
            if (emarker_electronic_path) {
                ++result->emarker_electronic_path_pair_count;
            }

            is_power_cross = contacts_cross_power_rails(first_physical_contact, second_physical_contact);
            gnd_source_to_vbus = false;
            vbus_source_to_gnd = false;
            if (is_power_cross) {
                if (contact_is_ground(first_physical_contact)) {
                    gnd_source_to_vbus = forward;
                    vbus_source_to_gnd = reverse;
                } else {
                    gnd_source_to_vbus = reverse;
                    vbus_source_to_gnd = forward;
                }
            }
            if (is_power_cross &&
                (gnd_source_to_vbus || vbus_source_to_gnd || temporal_unstable)) {
                ++result->power_cross_pair_count;
                if (gnd_source_to_vbus && vbus_source_to_gnd) {
                    ++result->power_cross_bidir_pair_count;
                    ++result->confirmed_power_cross_pair_count;
                } else if (gnd_source_to_vbus) {
                    ++result->power_cross_gnd_source_to_vbus_pair_count;
                } else if (vbus_source_to_gnd) {
                    ++result->power_cross_vbus_source_to_gnd_pair_count;
                }
                if (temporal_unstable) {
                    ++result->power_cross_temporal_pair_count;
                }
                tester_bitmap_set(result->power_cross_endpoint_bitmap, first, true);
                tester_bitmap_set(result->power_cross_endpoint_bitmap, second, true);
            }

            if (confirmed && !unstable) {
                if (first_end == second_end) {
                    ++result->confirmed_local_pair_count[first_end];
                    result->detected_end_mask |= (uint8_t)(1u << first_end);
                } else {
                    ++result->confirmed_cross_pair_count;
                    tester_bitmap_set(result->conductive_contact_bitmap, (uint8_t)first_contact, true);
                    tester_bitmap_set(result->conductive_contact_bitmap, (uint8_t)second_contact, true);
                }
            }

            if (unstable) {
                ++result->unstable_pair_count;
                mark_endpoint_and_contact(result->unstable_endpoint_bitmap, result->fault_contact_bitmap, first);
                mark_endpoint_and_contact(result->unstable_endpoint_bitmap, result->fault_contact_bitmap, second);
            }
            if (required && !confirmed) {
                ++result->missing_pair_count;
                mark_endpoint_and_contact(result->open_endpoint_bitmap, result->fault_contact_bitmap, first);
                mark_endpoint_and_contact(result->open_endpoint_bitmap, result->fault_contact_bitmap, second);
            }
            if (confirmed && !temporal_unstable && !allowed) {
                ++result->unexpected_pair_count;
                if ((required_degree[first] != 0u) && (required_degree[second] != 0u) &&
                    (confirmed_degree[first] == 1u) && (confirmed_degree[second] == 1u)) {
                    ++result->miswire_pair_count;
                } else {
                    ++result->short_pair_count;
                }
                mark_endpoint_and_contact(result->unexpected_endpoint_bitmap, result->fault_contact_bitmap, first);
                mark_endpoint_and_contact(result->unexpected_endpoint_bitmap, result->fault_contact_bitmap, second);
            }
        }
    }

    result->score = ((uint32_t)result->missing_pair_count * 100u) +
                    ((uint32_t)result->unexpected_pair_count * 200u) +
                    ((uint32_t)result->unstable_pair_count * 50u) +
                    ((uint32_t)result->power_cross_pair_count * 1000u);
    result->code = select_result_code(
        result->kind,
        result->missing_pair_count,
        result->unexpected_pair_count,
        result->unstable_pair_count,
        (uint16_t)(result->power_cross_bidir_pair_count +
                   result->power_cross_gnd_source_to_vbus_pair_count),
        (uint16_t)(result->power_cross_vbus_source_to_gnd_pair_count +
                   result->power_cross_temporal_pair_count),
        result->confirmed_cross_pair_count,
        result->detected_end_mask);
    return true;
}

static bool analysis_fit_is_better(
    const cable_analysis_result_t *candidate,
    const cable_analysis_result_t *reference)
{
    if (candidate->score != reference->score) {
        return candidate->score < reference->score;
    }
    if (candidate->unexpected_pair_count != reference->unexpected_pair_count) {
        return candidate->unexpected_pair_count < reference->unexpected_pair_count;
    }
    if (candidate->missing_pair_count != reference->missing_pair_count) {
        return candidate->missing_pair_count < reference->missing_pair_count;
    }
    return candidate->unstable_pair_count < reference->unstable_pair_count;
}

static bool analysis_fit_is_equal(
    const cable_analysis_result_t *left,
    const cable_analysis_result_t *right)
{
    return (left->score == right->score) &&
           (left->unexpected_pair_count == right->unexpected_pair_count) &&
           (left->missing_pair_count == right->missing_pair_count) &&
           (left->unstable_pair_count == right->unstable_pair_count);
}

bool cable_analyze_best_orientation(
    const tester_observation_t *observation,
    cable_kind_t kind,
    cable_analysis_result_t *result)
{
    cable_profile_t profile;
    cable_analysis_result_t candidate;
    cable_analysis_result_t best;
    uint8_t flip_mask;
    uint8_t tie_count = 0u;

    if ((observation == NULL) || (result == NULL) || ((uint8_t)kind >= (uint8_t)CABLE_KIND_COUNT)) {
        return false;
    }

    memset(&best, 0, sizeof(best));
    best.score = UINT32_MAX;

    for (flip_mask = 0u; flip_mask < 4u; ++flip_mask) {
        if (!cable_profile_build(
                &profile,
                kind,
                (flip_mask & 1u) != 0u,
                (flip_mask & 2u) != 0u) ||
            !cable_analyze(observation, &profile, &candidate)) {
            return false;
        }
        if (analysis_fit_is_better(&candidate, &best)) {
            best = candidate;
            tie_count = 1u;
        } else if (analysis_fit_is_equal(&candidate, &best)) {
            ++tie_count;
        }
    }

    best.orientation_ambiguous = tie_count > 1u;
    *result = best;
    return true;
}

bool cable_analyze_auto(
    const tester_observation_t *observation,
    cable_analysis_result_t *result)
{
    static const cable_kind_t candidates[] = {
        CABLE_KIND_USB2_UNMARKED,
        CABLE_KIND_USB2_EMARKED,
        CABLE_KIND_FULL_EMARKED
    };
    cable_analysis_result_t candidate;
    cable_analysis_result_t best;
    uint8_t index;

    if ((observation == NULL) || (result == NULL)) {
        return false;
    }

    memset(&best, 0, sizeof(best));
    best.score = UINT32_MAX;
    for (index = 0u; index < (uint8_t)(sizeof(candidates) / sizeof(candidates[0])); ++index) {
        if (!cable_analyze_best_orientation(observation, candidates[index], &candidate)) {
            return false;
        }
        /* Candidate order is the final deterministic least-capability tie-break. */
        if (analysis_fit_is_better(&candidate, &best)) {
            best = candidate;
        }
    }
    if ((best.code == CABLE_RESULT_NO_CONNECTION) ||
        (best.code == CABLE_RESULT_ONE_END_ONLY)) {
        /* AUTO cannot claim a cable family before an end-to-end conductor exists. */
        best.kind = CABLE_KIND_AUTO;
    }
    *result = best;
    return true;
}

const char *cable_result_name(cable_result_code_t code)
{
    switch (code) {
    case CABLE_RESULT_NOT_TESTED:
        return "NOT_TESTED";
    case CABLE_RESULT_NO_CONNECTION:
        return "NO_CONNECTION";
    case CABLE_RESULT_ONE_END_ONLY:
        return "ONE_END_ONLY";
    case CABLE_RESULT_DISCOVERY:
        return "DISCOVERY";
    case CABLE_RESULT_PASS:
        return "PASS";
    case CABLE_RESULT_OPEN:
        return "OPEN";
    case CABLE_RESULT_SHORT_OR_MISWIRE:
        return "SHORT_OR_MISWIRE";
    case CABLE_RESULT_OPEN_AND_SHORT:
        return "OPEN_AND_SHORT";
    case CABLE_RESULT_UNSTABLE:
        return "UNSTABLE";
    case CABLE_RESULT_POWER_CROSS_FAULT:
        return "POWER_CROSS_FAULT";
    case CABLE_RESULT_POWER_CROSS_SUSPECT:
        return "POWER_CROSS_SUSPECT";
    case CABLE_RESULT_HARDWARE_ERROR:
        return "HARDWARE_ERROR";
    case CABLE_RESULT_CONDUCTORS_PASS_EMARKER_UNVERIFIED:
        return "CONDUCTORS_PASS_EMARKER_UNVERIFIED";
    default:
        return "UNKNOWN";
    }
}
