#include "cable_analysis.h"
#include "tester_app.h"

#define EXPECT(condition) do { if (!(condition)) { ++failures; } } while (0)

static int failures;

static void expect_scan_progress(uint8_t completed_source_count)
{
    uint8_t bitmap[TESTER_CONTACT_BITMAP_BYTES];
    uint8_t contact;
    uint8_t clamped = completed_source_count;
    bool expected;

    if (clamped > TESTER_ENDPOINT_COUNT) {
        clamped = TESTER_ENDPOINT_COUNT;
    }
    tester_app_build_scan_progress_bitmap(completed_source_count, bitmap);
    for (contact = 0u; contact < TESTER_CONTACT_COUNT; ++contact) {
        if (clamped <= TESTER_CONTACT_COUNT) {
            expected = contact < clamped;
        } else {
            expected = contact >= (uint8_t)(clamped - TESTER_CONTACT_COUNT);
        }
        EXPECT(tester_bitmap_get(bitmap, contact) == expected);
    }
}

static void test_scan_progress_contract(void)
{
    expect_scan_progress(0u);
    expect_scan_progress(1u);
    expect_scan_progress(23u);
    expect_scan_progress(24u);
    expect_scan_progress(25u);
    expect_scan_progress(47u);
    expect_scan_progress(48u);
    expect_scan_progress(255u);
}

static void observe_group(tester_observation_t *observation, const uint8_t *members, uint8_t count)
{
    uint8_t first;
    uint8_t second;

    for (first = 0u; first < count; ++first) {
        for (second = 0u; second < count; ++second) {
            if (first != second) {
                tester_matrix_set(&observation->low, members[first], members[second], true);
            }
        }
    }
}

static void observe_pair(tester_observation_t *observation, uint8_t first, uint8_t second)
{
    tester_matrix_set(&observation->low, first, second, true);
    tester_matrix_set(&observation->low, second, first, true);
}

static void observe_local_power_groups(tester_observation_t *observation, tester_end_t end)
{
    static const tester_contact_t ground_contacts[4] = {
        TESTER_CONTACT_A1, TESTER_CONTACT_B1, TESTER_CONTACT_A12, TESTER_CONTACT_B12
    };
    static const tester_contact_t vbus_contacts[4] = {
        TESTER_CONTACT_A4, TESTER_CONTACT_B4, TESTER_CONTACT_A9, TESTER_CONTACT_B9
    };
    uint8_t members[4];
    uint8_t index;

    for (index = 0u; index < 4u; ++index) {
        members[index] = tester_endpoint(end, ground_contacts[index]);
    }
    observe_group(observation, members, 4u);
    for (index = 0u; index < 4u; ++index) {
        members[index] = tester_endpoint(end, vbus_contacts[index]);
    }
    observe_group(observation, members, 4u);
}

static void observe_allowed_optional_pairs(
    tester_observation_t *observation,
    const cable_profile_t *profile)
{
    uint8_t first;
    uint8_t second;

    for (first = 0u; first < TESTER_ENDPOINT_COUNT; ++first) {
        for (second = (uint8_t)(first + 1u); second < TESTER_ENDPOINT_COUNT; ++second) {
            if ((tester_matrix_get(&profile->allowed, first, second) ||
                 tester_matrix_get(&profile->allowed, second, first)) &&
                !tester_matrix_get(&profile->required, first, second) &&
                !tester_matrix_get(&profile->required, second, first)) {
                observe_pair(observation, first, second);
            }
        }
    }
}

static void expect_conductive_contacts(
    const cable_analysis_result_t *result,
    const tester_contact_t *expected_contacts,
    uint8_t expected_count)
{
    uint8_t contact;
    uint8_t expected_index;
    bool expected;

    for (contact = 0u; contact < TESTER_CONTACT_COUNT; ++contact) {
        expected = false;
        for (expected_index = 0u; expected_index < expected_count; ++expected_index) {
            if (contact == (uint8_t)expected_contacts[expected_index]) {
                expected = true;
                break;
            }
        }
        EXPECT(tester_bitmap_get(result->conductive_contact_bitmap, contact) == expected);
    }
}

static void test_no_connection_has_no_signal_contacts(void)
{
    tester_observation_t observation = {0};
    cable_analysis_result_t result;

    EXPECT(cable_analyze_best_orientation(&observation, CABLE_KIND_FULL_EMARKED, &result));
    EXPECT(result.code == CABLE_RESULT_NO_CONNECTION);
    EXPECT(result.detected_end_mask == 0u);
    EXPECT(result.confirmed_cross_pair_count == 0u);
    EXPECT(result.confirmed_local_pair_count[TESTER_END_J1] == 0u);
    EXPECT(result.confirmed_local_pair_count[TESTER_END_J2] == 0u);
    expect_conductive_contacts(&result, NULL, 0u);
}

static void test_one_end_only_has_no_end_to_end_signal(void)
{
    tester_observation_t observation = {0};
    cable_analysis_result_t result;

    observe_local_power_groups(&observation, TESTER_END_J1);
    EXPECT(cable_analyze_best_orientation(&observation, CABLE_KIND_FULL_EMARKED, &result));
    EXPECT(result.code == CABLE_RESULT_ONE_END_ONLY);
    EXPECT(result.detected_end_mask == (uint8_t)(1u << TESTER_END_J1));
    EXPECT(result.confirmed_cross_pair_count == 0u);
    EXPECT(result.confirmed_local_pair_count[TESTER_END_J1] == 12u);
    EXPECT(result.confirmed_local_pair_count[TESTER_END_J2] == 0u);
    expect_conductive_contacts(&result, NULL, 0u);

    observation = (tester_observation_t){0};
    observe_local_power_groups(&observation, TESTER_END_J2);
    EXPECT(cable_analyze_best_orientation(&observation, CABLE_KIND_FULL_EMARKED, &result));
    EXPECT(result.code == CABLE_RESULT_ONE_END_ONLY);
    EXPECT(result.detected_end_mask == (uint8_t)(1u << TESTER_END_J2));
    EXPECT(result.confirmed_cross_pair_count == 0u);
    EXPECT(result.confirmed_local_pair_count[TESTER_END_J1] == 0u);
    EXPECT(result.confirmed_local_pair_count[TESTER_END_J2] == 12u);
    expect_conductive_contacts(&result, NULL, 0u);
}

static void test_two_inserted_ends_with_all_cross_wires_open(void)
{
    tester_observation_t observation = {0};
    cable_analysis_result_t result;

    observe_local_power_groups(&observation, TESTER_END_J1);
    observe_local_power_groups(&observation, TESTER_END_J2);
    EXPECT(cable_analyze_best_orientation(&observation, CABLE_KIND_FULL_EMARKED, &result));
    EXPECT(result.code == CABLE_RESULT_OPEN);
    EXPECT(result.detected_end_mask == 0x03u);
    EXPECT(result.confirmed_cross_pair_count == 0u);
    EXPECT(result.confirmed_local_pair_count[TESTER_END_J1] == 12u);
    EXPECT(result.confirmed_local_pair_count[TESTER_END_J2] == 12u);
    expect_conductive_contacts(&result, NULL, 0u);
}

static void test_cross_end_bitmap_is_normalized_for_all_orientations(void)
{
    static const tester_contact_t expected[1] = {TESTER_CONTACT_A6};
    tester_observation_t observation;
    cable_analysis_result_t result;
    uint8_t flip_mask;
    bool j1_flipped;
    bool j2_flipped;
    tester_contact_t j1_physical;
    tester_contact_t j2_physical;

    for (flip_mask = 0u; flip_mask < 4u; ++flip_mask) {
        observation = (tester_observation_t){0};
        j1_flipped = (flip_mask & 1u) != 0u;
        j2_flipped = (flip_mask & 2u) != 0u;
        j1_physical = j1_flipped ? TESTER_CONTACT_B6 : TESTER_CONTACT_A6;
        j2_physical = j2_flipped ? TESTER_CONTACT_B6 : TESTER_CONTACT_A6;
        observe_pair(
            &observation,
            tester_endpoint(TESTER_END_J1, j1_physical),
            tester_endpoint(TESTER_END_J2, j2_physical));

        EXPECT(cable_analyze_best_orientation(&observation, CABLE_KIND_USB2_UNMARKED, &result));
        EXPECT(result.j1_flipped == j1_flipped);
        EXPECT(result.j2_flipped == j2_flipped);
        EXPECT(!result.orientation_ambiguous);
        /* detected_end_mask intentionally describes plug-local presence signatures. */
        EXPECT(result.detected_end_mask == 0u);
        EXPECT(result.confirmed_cross_pair_count == 1u);
        EXPECT(result.confirmed_local_pair_count[TESTER_END_J1] == 0u);
        EXPECT(result.confirmed_local_pair_count[TESTER_END_J2] == 0u);
        expect_conductive_contacts(&result, expected, 1u);
    }
}

static void test_usb2_signal_bitmap_excludes_absent_plug_contacts(void)
{
    static const tester_contact_t expected[] = {
        TESTER_CONTACT_A1,
        TESTER_CONTACT_A4,
        TESTER_CONTACT_A5,
        TESTER_CONTACT_A6,
        TESTER_CONTACT_A7,
        TESTER_CONTACT_A9,
        TESTER_CONTACT_A12,
        TESTER_CONTACT_B1,
        TESTER_CONTACT_B4,
        TESTER_CONTACT_B9,
        TESTER_CONTACT_B12
    };
    cable_profile_t profile;
    tester_observation_t observation;
    cable_analysis_result_t result;
    uint8_t flip_mask;

    for (flip_mask = 0u; flip_mask < 4u; ++flip_mask) {
        EXPECT(cable_profile_build(
            &profile,
            CABLE_KIND_USB2_UNMARKED,
            (flip_mask & 1u) != 0u,
            (flip_mask & 2u) != 0u));
        observation = (tester_observation_t){0};
        observation.low = profile.required;

        EXPECT(cable_analyze_best_orientation(&observation, CABLE_KIND_USB2_UNMARKED, &result));
        EXPECT(result.code == CABLE_RESULT_PASS);
        EXPECT(result.detected_end_mask == 0x03u);
        EXPECT(result.confirmed_cross_pair_count == 35u);
        EXPECT(result.confirmed_local_pair_count[TESTER_END_J1] == 12u);
        EXPECT(result.confirmed_local_pair_count[TESTER_END_J2] == 12u);
        expect_conductive_contacts(&result, expected, (uint8_t)(sizeof(expected) / sizeof(expected[0])));
        EXPECT(!tester_bitmap_get(result.conductive_contact_bitmap, TESTER_CONTACT_B5));
        EXPECT(!tester_bitmap_get(result.conductive_contact_bitmap, TESTER_CONTACT_B6));
        EXPECT(!tester_bitmap_get(result.conductive_contact_bitmap, TESTER_CONTACT_B7));
    }
}

static void test_emarker_vconn_is_optional(void)
{
    cable_profile_t profile;
    tester_observation_t observation = {0};
    cable_analysis_result_t result;
    uint8_t j1_b5;
    uint8_t j2_b5;
    uint8_t j1_ground;

    EXPECT(cable_profile_build(&profile, CABLE_KIND_FULL_EMARKED, false, false));
    j1_b5 = tester_endpoint(TESTER_END_J1, TESTER_CONTACT_B5);
    j2_b5 = tester_endpoint(TESTER_END_J2, TESTER_CONTACT_B5);
    j1_ground = tester_endpoint(TESTER_END_J1, TESTER_CONTACT_A1);

    EXPECT(!tester_matrix_get(&profile.required, j1_b5, j2_b5));
    EXPECT(tester_matrix_get(&profile.allowed, j1_b5, j2_b5));
    EXPECT(!tester_matrix_get(&profile.required, j1_b5, j1_ground));
    EXPECT(tester_matrix_get(&profile.allowed, j1_b5, j1_ground));

    observation.low = profile.required;
    observe_allowed_optional_pairs(&observation, &profile);

    EXPECT(cable_analyze(&observation, &profile, &result));
    EXPECT(result.missing_pair_count == 0u);
    EXPECT(result.unexpected_pair_count == 0u);
    EXPECT(result.short_pair_count == 0u);
    EXPECT(result.unstable_pair_count == 0u);
    EXPECT(result.emarker_electronic_path_pair_count == 17u);
    EXPECT(result.code == CABLE_RESULT_CONDUCTORS_PASS_EMARKER_UNVERIFIED);
}

static void test_optional_vconn_direction_does_not_fail_conductors(void)
{
    cable_profile_t profile;
    tester_observation_t observation = {0};
    cable_analysis_result_t result;
    uint8_t j1_b5;
    uint8_t j1_ground;

    EXPECT(cable_profile_build(&profile, CABLE_KIND_FULL_EMARKED, false, false));
    observation.low = profile.required;
    j1_b5 = tester_endpoint(TESTER_END_J1, TESTER_CONTACT_B5);
    j1_ground = tester_endpoint(TESTER_END_J1, TESTER_CONTACT_A1);
    tester_matrix_set(&observation.low, j1_b5, j1_ground, true);

    EXPECT(cable_analyze(&observation, &profile, &result));
    EXPECT(result.unstable_pair_count == 0u);
    EXPECT(result.emarker_electronic_path_pair_count == 1u);
    EXPECT(result.code == CABLE_RESULT_CONDUCTORS_PASS_EMARKER_UNVERIFIED);
}

static void test_emarker_unallowed_bidirectional_and_temporal_paths_fail_safely(void)
{
    cable_profile_t profile;
    tester_observation_t observation = {0};
    cable_analysis_result_t result;
    uint8_t j1_a5 = tester_endpoint(TESTER_END_J1, TESTER_CONTACT_A5);
    uint8_t j1_ground = tester_endpoint(TESTER_END_J1, TESTER_CONTACT_A1);
    uint8_t j1_b5 = tester_endpoint(TESTER_END_J1, TESTER_CONTACT_B5);

    EXPECT(cable_profile_build(&profile, CABLE_KIND_FULL_EMARKED, false, false));
    observation.low = profile.required;
    observe_pair(&observation, j1_a5, j1_ground);
    EXPECT(cable_analyze(&observation, &profile, &result));
    EXPECT(result.unexpected_pair_count == 1u);
    EXPECT(result.short_pair_count == 1u);
    EXPECT(result.emarker_electronic_path_pair_count == 0u);
    EXPECT(result.code == CABLE_RESULT_SHORT_OR_MISWIRE);

    observation = (tester_observation_t){0};
    observation.low = profile.required;
    tester_matrix_set(&observation.low, j1_b5, j1_ground, true);
    tester_matrix_set(&observation.unstable, j1_b5, j1_ground, true);
    EXPECT(cable_analyze(&observation, &profile, &result));
    EXPECT(result.unexpected_pair_count == 0u);
    EXPECT(result.temporal_unstable_pair_count == 1u);
    EXPECT(result.unstable_pair_count == 1u);
    EXPECT(result.emarker_electronic_path_pair_count == 0u);
    EXPECT(result.code == CABLE_RESULT_UNSTABLE);
}

static void test_auto_profile_selection(void)
{
    cable_profile_t profile;
    tester_observation_t observation = {0};
    cable_analysis_result_t result;
    uint8_t j1_b5;
    uint8_t j1_ground;

    EXPECT(cable_profile_build(&profile, CABLE_KIND_FULL_EMARKED, false, true));
    observation.low = profile.required;
    observe_allowed_optional_pairs(&observation, &profile);
    EXPECT(cable_analyze_auto(&observation, &result));
    EXPECT(result.kind == CABLE_KIND_FULL_EMARKED);
    EXPECT(result.missing_pair_count == 0u);
    EXPECT(result.unexpected_pair_count == 0u);
    EXPECT(result.short_pair_count == 0u);
    EXPECT(result.unstable_pair_count == 0u);
    EXPECT(result.power_cross_pair_count == 0u);
    EXPECT(result.emarker_electronic_path_pair_count == 17u);
    EXPECT(result.code == CABLE_RESULT_CONDUCTORS_PASS_EMARKER_UNVERIFIED);

    EXPECT(cable_profile_build(&profile, CABLE_KIND_USB2_UNMARKED, false, false));
    observation = (tester_observation_t){0};
    observation.low = profile.required;
    EXPECT(cable_analyze_auto(&observation, &result));
    EXPECT(result.kind == CABLE_KIND_USB2_UNMARKED);
    EXPECT(result.code == CABLE_RESULT_PASS);

    EXPECT(cable_profile_build(&profile, CABLE_KIND_USB2_EMARKED, false, false));
    observation = (tester_observation_t){0};
    observation.low = profile.required;
    j1_b5 = tester_endpoint(TESTER_END_J1, TESTER_CONTACT_B5);
    j1_ground = tester_endpoint(TESTER_END_J1, TESTER_CONTACT_A1);
    tester_matrix_set(&observation.low, j1_b5, j1_ground, true);
    EXPECT(cable_analyze_auto(&observation, &result));
    EXPECT(result.kind == CABLE_KIND_USB2_EMARKED);
    EXPECT(result.emarker_electronic_path_pair_count == 1u);
    EXPECT(result.unstable_pair_count == 0u);
    EXPECT(result.code == CABLE_RESULT_CONDUCTORS_PASS_EMARKER_UNVERIFIED);
    EXPECT(result.code != CABLE_RESULT_PASS);

    observation = (tester_observation_t){0};
    EXPECT(cable_analyze_auto(&observation, &result));
    EXPECT(result.kind == CABLE_KIND_AUTO);
    EXPECT(result.code == CABLE_RESULT_NO_CONNECTION);
}

static void test_directed_power_cross_safety_classes(void)
{
    cable_profile_t profile;
    tester_observation_t observation = {0};
    cable_analysis_result_t result;
    uint8_t ground = tester_endpoint(TESTER_END_J1, TESTER_CONTACT_A1);
    uint8_t vbus = tester_endpoint(TESTER_END_J1, TESTER_CONTACT_A4);

    EXPECT(cable_profile_build(&profile, CABLE_KIND_DISCOVERY, false, false));

    tester_matrix_set(&observation.low, ground, vbus, true);
    tester_matrix_set(&observation.low, vbus, ground, true);
    EXPECT(cable_analyze(&observation, &profile, &result));
    EXPECT(result.code == CABLE_RESULT_POWER_CROSS_FAULT);
    EXPECT(result.power_cross_pair_count == 1u);
    EXPECT(result.power_cross_bidir_pair_count == 1u);
    EXPECT(result.confirmed_power_cross_pair_count == 1u);
    EXPECT(result.power_cross_gnd_source_to_vbus_pair_count == 0u);
    EXPECT(result.power_cross_vbus_source_to_gnd_pair_count == 0u);
    EXPECT(result.power_cross_temporal_pair_count == 0u);
    EXPECT(result.asymmetric_pair_count == 0u);
    EXPECT(result.temporal_unstable_pair_count == 0u);
    EXPECT(tester_bitmap_get(result.power_cross_endpoint_bitmap, ground));
    EXPECT(tester_bitmap_get(result.power_cross_endpoint_bitmap, vbus));

    observation = (tester_observation_t){0};
    tester_matrix_set(&observation.low, ground, vbus, true);
    EXPECT(cable_analyze(&observation, &profile, &result));
    EXPECT(result.code == CABLE_RESULT_POWER_CROSS_FAULT);
    EXPECT(result.power_cross_pair_count == 1u);
    EXPECT(result.power_cross_bidir_pair_count == 0u);
    EXPECT(result.power_cross_gnd_source_to_vbus_pair_count == 1u);
    EXPECT(result.power_cross_vbus_source_to_gnd_pair_count == 0u);
    EXPECT(result.asymmetric_pair_count == 1u);
    EXPECT(result.temporal_unstable_pair_count == 0u);
    EXPECT(result.unstable_pair_count == 1u);

    observation = (tester_observation_t){0};
    tester_matrix_set(&observation.low, vbus, ground, true);
    EXPECT(cable_analyze(&observation, &profile, &result));
    EXPECT(result.code == CABLE_RESULT_POWER_CROSS_SUSPECT);
    EXPECT(result.power_cross_pair_count == 1u);
    EXPECT(result.power_cross_bidir_pair_count == 0u);
    EXPECT(result.power_cross_gnd_source_to_vbus_pair_count == 0u);
    EXPECT(result.power_cross_vbus_source_to_gnd_pair_count == 1u);
    EXPECT(result.asymmetric_pair_count == 1u);
    EXPECT(result.temporal_unstable_pair_count == 0u);

    observation = (tester_observation_t){0};
    tester_matrix_set(&observation.unstable, vbus, ground, true);
    EXPECT(cable_analyze(&observation, &profile, &result));
    EXPECT(result.code == CABLE_RESULT_POWER_CROSS_SUSPECT);
    EXPECT(result.power_cross_pair_count == 1u);
    EXPECT(result.power_cross_bidir_pair_count == 0u);
    EXPECT(result.power_cross_vbus_source_to_gnd_pair_count == 0u);
    EXPECT(result.power_cross_temporal_pair_count == 1u);
    EXPECT(result.asymmetric_pair_count == 0u);
    EXPECT(result.temporal_unstable_pair_count == 1u);
    EXPECT(result.unstable_pair_count == 1u);
}

static void test_power_suspect_preserves_confirmed_cross_end_signal_bitmap(void)
{
    cable_profile_t profile;
    tester_observation_t observation = {0};
    cable_analysis_result_t result;
    uint8_t ground = tester_endpoint(TESTER_END_J1, TESTER_CONTACT_A1);
    uint8_t vbus = tester_endpoint(TESTER_END_J1, TESTER_CONTACT_A4);

    EXPECT(cable_profile_build(&profile, CABLE_KIND_USB2_UNMARKED, false, false));
    observation.low = profile.required;
    tester_matrix_set(&observation.low, vbus, ground, true);

    EXPECT(cable_analyze(&observation, &profile, &result));
    EXPECT(result.code == CABLE_RESULT_POWER_CROSS_SUSPECT);
    EXPECT(result.confirmed_cross_pair_count != 0u);
    EXPECT(tester_bitmap_get(result.conductive_contact_bitmap, TESTER_CONTACT_A6));
    EXPECT(tester_bitmap_get(result.conductive_contact_bitmap, TESTER_CONTACT_A7));
}

static void test_compliance_result_policy(void)
{
    cable_profile_t profile;
    tester_observation_t observation = {0};
    cable_analysis_result_t result;
    cable_kind_t parsed_kind = CABLE_KIND_DISCOVERY;

    EXPECT(cable_kind_parse("FULL", &parsed_kind));
    EXPECT(parsed_kind == CABLE_KIND_FULL_EMARKED);
    EXPECT(cable_kind_parse("AUTO", &parsed_kind));
    EXPECT(parsed_kind == CABLE_KIND_AUTO);
    EXPECT(!cable_profile_build(&profile, CABLE_KIND_AUTO, false, false));

    EXPECT(cable_profile_build(&profile, CABLE_KIND_FULL_UNMARKED, false, false));
    observation.low = profile.required;
    EXPECT(cable_analyze(&observation, &profile, &result));
    EXPECT(result.code == CABLE_RESULT_CONDUCTORS_PASS_EMARKER_UNVERIFIED);
    EXPECT(result.code != CABLE_RESULT_PASS);

    EXPECT(cable_profile_build(&profile, CABLE_KIND_USB2_UNMARKED, false, false));
    observation = (tester_observation_t){0};
    observation.low = profile.required;
    EXPECT(cable_analyze(&observation, &profile, &result));
    EXPECT(result.code == CABLE_RESULT_PASS);
}

static void test_discovery_and_straight_fixture_modes(void)
{
    cable_profile_t profile;
    tester_observation_t observation = {0};
    cable_analysis_result_t result;

    EXPECT(cable_profile_build(&profile, CABLE_KIND_DISCOVERY, false, false));
    EXPECT(cable_analyze(&observation, &profile, &result));
    EXPECT(result.code == CABLE_RESULT_DISCOVERY);

    EXPECT(cable_profile_build(&profile, CABLE_KIND_STRAIGHT_24_FIXTURE, false, false));
    observation.low = profile.required;
    EXPECT(cable_analyze(&observation, &profile, &result));
    EXPECT(result.code == CABLE_RESULT_PASS);
}

int main(void)
{
    test_scan_progress_contract();
    test_no_connection_has_no_signal_contacts();
    test_one_end_only_has_no_end_to_end_signal();
    test_two_inserted_ends_with_all_cross_wires_open();
    test_cross_end_bitmap_is_normalized_for_all_orientations();
    test_usb2_signal_bitmap_excludes_absent_plug_contacts();
    test_emarker_vconn_is_optional();
    test_optional_vconn_direction_does_not_fail_conductors();
    test_emarker_unallowed_bidirectional_and_temporal_paths_fail_safely();
    test_auto_profile_selection();
    test_directed_power_cross_safety_classes();
    test_power_suspect_preserves_confirmed_cross_end_signal_bitmap();
    test_compliance_result_policy();
    test_discovery_and_straight_fixture_modes();
    return (failures == 0) ? 0 : 1;
}
