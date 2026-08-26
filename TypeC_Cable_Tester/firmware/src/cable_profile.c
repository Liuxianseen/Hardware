#include "cable_profile.h"

#include <ctype.h>
#include <string.h>

static tester_contact_t flip_contact(tester_contact_t contact, bool flipped)
{
    uint8_t value = (uint8_t)contact;

    if (!flipped) {
        return contact;
    }
    if (value < 12u) {
        value = (uint8_t)(value + 12u);
    } else {
        value = (uint8_t)(value - 12u);
    }
    return (tester_contact_t)value;
}

static uint8_t plug_endpoint(tester_end_t end, tester_contact_t contact, bool flipped)
{
    return tester_endpoint(end, flip_contact(contact, flipped));
}

static void connect_group(cable_profile_t *profile, const uint8_t *members, uint8_t member_count, bool required)
{
    uint8_t first;
    uint8_t second;

    for (first = 0u; first < member_count; ++first) {
        for (second = 0u; second < member_count; ++second) {
            if (first == second) {
                continue;
            }
            tester_matrix_set(&profile->allowed, members[first], members[second], true);
            if (required) {
                tester_matrix_set(&profile->required, members[first], members[second], true);
            }
        }
    }
}

static void connect_wire(
    cable_profile_t *profile,
    tester_contact_t j1_contact,
    tester_contact_t j2_contact)
{
    uint8_t members[2];

    members[0] = plug_endpoint(TESTER_END_J1, j1_contact, profile->j1_flipped);
    members[1] = plug_endpoint(TESTER_END_J2, j2_contact, profile->j2_flipped);
    connect_group(profile, members, 2u, true);
}

static void build_ground_members(const cable_profile_t *profile, uint8_t members[8])
{
    static const tester_contact_t ground_contacts[4] = {
        TESTER_CONTACT_A1, TESTER_CONTACT_B1, TESTER_CONTACT_A12, TESTER_CONTACT_B12
    };
    uint8_t index;

    for (index = 0u; index < 4u; ++index) {
        members[index] = plug_endpoint(TESTER_END_J1, ground_contacts[index], profile->j1_flipped);
        members[index + 4u] = plug_endpoint(TESTER_END_J2, ground_contacts[index], profile->j2_flipped);
    }
}

static void add_power_groups(cable_profile_t *profile)
{
    static const tester_contact_t vbus_contacts[4] = {
        TESTER_CONTACT_A4, TESTER_CONTACT_B4, TESTER_CONTACT_A9, TESTER_CONTACT_B9
    };
    uint8_t ground_members[8];
    uint8_t vbus_members[8];
    uint8_t index;

    build_ground_members(profile, ground_members);
    for (index = 0u; index < 4u; ++index) {
        vbus_members[index] = plug_endpoint(TESTER_END_J1, vbus_contacts[index], profile->j1_flipped);
        vbus_members[index + 4u] = plug_endpoint(TESTER_END_J2, vbus_contacts[index], profile->j2_flipped);
    }
    connect_group(profile, ground_members, 8u, true);
    connect_group(profile, vbus_members, 8u, true);
}

static void allow_emarker_vconn_paths(cable_profile_t *profile)
{
    uint8_t members[10];

    build_ground_members(profile, members);
    members[8] = plug_endpoint(TESTER_END_J1, TESTER_CONTACT_B5, profile->j1_flipped);
    members[9] = plug_endpoint(TESTER_END_J2, TESTER_CONTACT_B5, profile->j2_flipped);

    /*
     * An unpowered eMarker may expose Ra from its local VCONN contact to the
     * cable GND network.  Depending on the cable construction, VCONN may be
     * isolated end-to-end or implemented by two local eMarkers.  These paths
     * are therefore tolerated but never required as low-resistance wires.
     */
    connect_group(profile, members, 10u, false);
}

static void add_usb2_wires(cable_profile_t *profile, bool allow_emarker_paths)
{
    add_power_groups(profile);
    connect_wire(profile, TESTER_CONTACT_A5, TESTER_CONTACT_A5);
    connect_wire(profile, TESTER_CONTACT_A6, TESTER_CONTACT_A6);
    connect_wire(profile, TESTER_CONTACT_A7, TESTER_CONTACT_A7);
    if (allow_emarker_paths) {
        allow_emarker_vconn_paths(profile);
    }
}

static void add_full_feature_wires(cable_profile_t *profile)
{
    /* All compliant full-featured cables require an eMarker. */
    add_usb2_wires(profile, true);

    /* USB Type-C Cable and Connector Specification, standard cable wire table. */
    connect_wire(profile, TESTER_CONTACT_A2, TESTER_CONTACT_B11);
    connect_wire(profile, TESTER_CONTACT_A3, TESTER_CONTACT_B10);
    connect_wire(profile, TESTER_CONTACT_B11, TESTER_CONTACT_A2);
    connect_wire(profile, TESTER_CONTACT_B10, TESTER_CONTACT_A3);
    connect_wire(profile, TESTER_CONTACT_B2, TESTER_CONTACT_A11);
    connect_wire(profile, TESTER_CONTACT_B3, TESTER_CONTACT_A10);
    connect_wire(profile, TESTER_CONTACT_A11, TESTER_CONTACT_B2);
    connect_wire(profile, TESTER_CONTACT_A10, TESTER_CONTACT_B3);
    connect_wire(profile, TESTER_CONTACT_A8, TESTER_CONTACT_B8);
    connect_wire(profile, TESTER_CONTACT_B8, TESTER_CONTACT_A8);
}

static bool text_equals(const char *left, const char *right)
{
    unsigned char a;
    unsigned char b;

    if ((left == NULL) || (right == NULL)) {
        return false;
    }
    do {
        a = (unsigned char)*left++;
        b = (unsigned char)*right++;
        if ((a == '-') || (a == ' ')) {
            a = '_';
        }
        if ((b == '-') || (b == ' ')) {
            b = '_';
        }
        if (toupper(a) != toupper(b)) {
            return false;
        }
    } while ((a != '\0') && (b != '\0'));
    return a == b;
}

bool cable_profile_build(
    cable_profile_t *profile,
    cable_kind_t kind,
    bool j1_flipped,
    bool j2_flipped)
{
    uint8_t contact;

    if ((profile == NULL) || ((uint8_t)kind >= (uint8_t)CABLE_KIND_COUNT)) {
        return false;
    }

    memset(profile, 0, sizeof(*profile));
    profile->kind = kind;
    profile->j1_flipped = j1_flipped;
    profile->j2_flipped = j2_flipped;

    switch (kind) {
    case CABLE_KIND_DISCOVERY:
        tester_matrix_fill(&profile->allowed);
        for (contact = 0u; contact < TESTER_ENDPOINT_COUNT; ++contact) {
            tester_matrix_set(&profile->allowed, contact, contact, false);
        }
        break;

    case CABLE_KIND_USB2_UNMARKED:
        add_usb2_wires(profile, false);
        break;

    case CABLE_KIND_USB2_EMARKED:
        add_usb2_wires(profile, true);
        break;

    case CABLE_KIND_FULL_UNMARKED:
        /* Legacy profile name retained for command compatibility. */
        add_full_feature_wires(profile);
        break;

    case CABLE_KIND_FULL_EMARKED:
        add_full_feature_wires(profile);
        break;

    case CABLE_KIND_STRAIGHT_24_FIXTURE:
        for (contact = 0u; contact < TESTER_CONTACT_COUNT; ++contact) {
            connect_wire(profile, (tester_contact_t)contact, (tester_contact_t)contact);
        }
        break;

    case CABLE_KIND_AUTO:
    case CABLE_KIND_COUNT:
    default:
        return false;
    }
    return true;
}

const char *cable_kind_name(cable_kind_t kind)
{
    switch (kind) {
    case CABLE_KIND_DISCOVERY:
        return "DISCOVERY";
    case CABLE_KIND_USB2_UNMARKED:
        return "USB2_UNMARKED";
    case CABLE_KIND_USB2_EMARKED:
        return "USB2_EMARKED";
    case CABLE_KIND_FULL_UNMARKED:
        return "FULL_UNMARKED";
    case CABLE_KIND_FULL_EMARKED:
        return "FULL_EMARKED";
    case CABLE_KIND_STRAIGHT_24_FIXTURE:
        return "STRAIGHT24";
    case CABLE_KIND_AUTO:
        return "AUTO";
    case CABLE_KIND_COUNT:
    default:
        return "UNKNOWN";
    }
}

bool cable_kind_parse(const char *text, cable_kind_t *kind)
{
    cable_kind_t candidate;

    if ((text == NULL) || (kind == NULL)) {
        return false;
    }
    if (text_equals(text, "USB2")) {
        *kind = CABLE_KIND_USB2_UNMARKED;
        return true;
    }
    if (text_equals(text, "FULL")) {
        *kind = CABLE_KIND_FULL_EMARKED;
        return true;
    }
    for (candidate = CABLE_KIND_DISCOVERY; candidate < CABLE_KIND_COUNT;
         candidate = (cable_kind_t)((uint8_t)candidate + 1u)) {
        if (text_equals(text, cable_kind_name(candidate))) {
            *kind = candidate;
            return true;
        }
    }
    return false;
}
