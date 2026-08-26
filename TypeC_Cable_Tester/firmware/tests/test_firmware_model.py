from __future__ import annotations

import re
import unittest
from pathlib import Path


CONTACTS = [*(f"A{i}" for i in range(1, 13)), *(f"B{i}" for i in range(1, 13))]
CONTACT_INDEX = {name: index for index, name in enumerate(CONTACTS)}
GROUND_CONTACTS = {CONTACT_INDEX[name] for name in ("A1", "A12", "B1", "B12")}
VBUS_CONTACTS = {CONTACT_INDEX[name] for name in ("A4", "A9", "B4", "B9")}
CC_CONTACTS = {CONTACT_INDEX[name] for name in ("A5", "B5")}

FULL_CABLE_REAL_MATRIX = """
J1.A1=J1.A12,J1.B1,J1.B5,J1.B12,J2.A1,J2.A5,J2.A12,J2.B1,J2.B12
J1.A2=J2.A11
J1.A3=J2.A10
J1.A4=J1.A9,J1.B4,J1.B9,J2.A4,J2.A9,J2.B4,J2.B9
J1.A5=J1.A1,J1.A12,J1.B1,J1.B5,J1.B12,J2.A1,J2.A5,J2.A12,J2.B1,J2.B5,J2.B12
J1.A6=J2.B6
J1.A7=J2.B7
J1.A8=J2.A8
J1.A9=J1.A4,J1.B4,J1.B9,J2.A4,J2.A9,J2.B4,J2.B9
J1.A10=J2.A3
J1.A11=J2.A2
J1.A12=J1.A1,J1.B1,J1.B5,J1.B12,J2.A1,J2.A5,J2.A12,J2.B1,J2.B12
J1.B1=J1.A1,J1.A12,J1.B5,J1.B12,J2.A1,J2.A5,J2.A12,J2.B1,J2.B12
J1.B2=J2.B11
J1.B3=J2.B10
J1.B4=J1.A4,J1.A9,J1.B9,J2.A4,J2.A9,J2.B4,J2.B9
J1.B5=J1.A1,J1.A12,J1.B1,J1.B12,J2.A1,J2.A5,J2.A12,J2.B1,J2.B12
J1.B8=J2.B8
J1.B9=J1.A4,J1.A9,J1.B4,J2.A4,J2.A9,J2.B4,J2.B9
J1.B10=J2.B3
J1.B11=J2.B2
J1.B12=J1.A1,J1.A12,J1.B1,J1.B5,J2.A1,J2.A5,J2.A12,J2.B1,J2.B12
J2.A1=J1.A1,J1.A12,J1.B1,J1.B5,J1.B12,J2.A5,J2.A12,J2.B1,J2.B12
J2.A2=J1.A11
J2.A3=J1.A10
J2.A4=J1.A4,J1.A9,J1.B4,J1.B9,J2.A9,J2.B4,J2.B9
J2.A5=J1.A1,J1.A12,J1.B1,J1.B5,J1.B12,J2.A1,J2.A12,J2.B1,J2.B12
J2.A8=J1.A8
J2.A9=J1.A4,J1.A9,J1.B4,J1.B9,J2.A4,J2.B4,J2.B9
J2.A10=J1.A3
J2.A11=J1.A2
J2.A12=J1.A1,J1.A12,J1.B1,J1.B5,J1.B12,J2.A1,J2.A5,J2.B1,J2.B12
J2.B1=J1.A1,J1.A12,J1.B1,J1.B5,J1.B12,J2.A1,J2.A5,J2.A12,J2.B12
J2.B2=J1.B11
J2.B3=J1.B10
J2.B4=J1.A4,J1.A9,J1.B4,J1.B9,J2.A4,J2.A9,J2.B9
J2.B5=J1.A1,J1.A5,J1.A12,J1.B1,J1.B5,J1.B12,J2.A1,J2.A5,J2.A12,J2.B1,J2.B12
J2.B6=J1.A6
J2.B7=J1.A7
J2.B8=J1.B8
J2.B9=J1.A4,J1.A9,J1.B4,J1.B9,J2.A4,J2.A9,J2.B4
J2.B10=J1.B3
J2.B11=J1.B2
J2.B12=J1.A1,J1.A12,J1.B1,J1.B5,J1.B12,J2.A1,J2.A5,J2.A12,J2.B1
"""


def braced_block_after(text: str, marker: str) -> str:
    marker_index = text.index(marker)
    brace_index = text.index("{", marker_index)
    depth = 0
    for index in range(brace_index, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[brace_index + 1 : index]
    raise AssertionError(f"unterminated C block after {marker!r}")


def endpoint(end: int, contact: str, flipped: bool) -> int:
    index = CONTACT_INDEX[contact]
    if flipped:
        index = index + 12 if index < 12 else index - 12
    return end * 24 + index


def add_group(graph: set[tuple[int, int]], members: list[int]) -> None:
    for first in range(len(members)):
        for second in range(first + 1, len(members)):
            graph.add(tuple(sorted((members[first], members[second]))))


def build_profile_rules(
    kind: str, j1_flipped: bool, j2_flipped: bool
) -> tuple[set[tuple[int, int]], set[tuple[int, int]]]:
    required: set[tuple[int, int]] = set()
    allowed: set[tuple[int, int]] = set()

    def group(members: list[int], required_path: bool = True) -> None:
        add_group(allowed, members)
        if required_path:
            add_group(required, members)

    def wire(j1_contact: str, j2_contact: str) -> None:
        group(
            [endpoint(0, j1_contact, j1_flipped), endpoint(1, j2_contact, j2_flipped)],
        )

    if kind == "DISCOVERY":
        for first in range(48):
            for second in range(first + 1, 48):
                allowed.add((first, second))
        return required, allowed
    if kind == "STRAIGHT24":
        for contact in CONTACTS:
            wire(contact, contact)
        return required, allowed

    ground_members = [
        *(endpoint(0, contact, j1_flipped) for contact in ("A1", "B1", "A12", "B12")),
        *(endpoint(1, contact, j2_flipped) for contact in ("A1", "B1", "A12", "B12")),
    ]
    group(ground_members)
    group(
        [
            *(endpoint(0, contact, j1_flipped) for contact in ("A4", "B4", "A9", "B9")),
            *(endpoint(1, contact, j2_flipped) for contact in ("A4", "B4", "A9", "B9")),
        ],
    )
    wire("A5", "A5")
    wire("A6", "A6")
    wire("A7", "A7")

    if (kind == "USB2_EMARKED") or kind.startswith("FULL"):
        group(
            [
                *ground_members,
                endpoint(0, "B5", j1_flipped),
                endpoint(1, "B5", j2_flipped),
            ],
            required_path=False,
        )

    if kind.startswith("FULL"):
        for j1_contact, j2_contact in (
            ("A2", "B11"),
            ("A3", "B10"),
            ("B11", "A2"),
            ("B10", "A3"),
            ("B2", "A11"),
            ("B3", "A10"),
            ("A11", "B2"),
            ("A10", "B3"),
            ("A8", "B8"),
            ("B8", "A8"),
        ):
            wire(j1_contact, j2_contact)
    return required, allowed


def build_profile(kind: str, j1_flipped: bool, j2_flipped: bool) -> set[tuple[int, int]]:
    return build_profile_rules(kind, j1_flipped, j2_flipped)[0]


def analyze(
    observed: set[tuple[int, int]],
    required: set[tuple[int, int]],
    allowed: set[tuple[int, int]] | None = None,
) -> tuple[int, int, int]:
    if allowed is None:
        allowed = required
    missing = len(required - observed)
    unexpected = len(observed - allowed)
    score = missing * 100 + unexpected * 200
    return missing, unexpected, score


def result_code(
    kind: str,
    missing: int,
    unexpected: int,
    unstable: int = 0,
    power_fault: int = 0,
    power_suspect: int = 0,
) -> str:
    if power_fault:
        return "POWER_CROSS_FAULT"
    if power_suspect:
        return "POWER_CROSS_SUSPECT"
    if missing and unexpected:
        return "OPEN_AND_SHORT"
    if unexpected:
        return "SHORT_OR_MISWIRE"
    if unstable:
        return "UNSTABLE"
    if kind == "DISCOVERY":
        return "DISCOVERY"
    if missing:
        return "OPEN"
    if kind in {"USB2_EMARKED", "FULL_UNMARKED", "FULL_EMARKED"}:
        return "CONDUCTORS_PASS_EMARKER_UNVERIFIED"
    return "PASS"


def classify_unexpected(
    observed: set[tuple[int, int]], required: set[tuple[int, int]]
) -> tuple[int, int]:
    confirmed_degree = [0] * 48
    required_degree = [0] * 48
    for first, second in observed:
        confirmed_degree[first] += 1
        confirmed_degree[second] += 1
    for first, second in required:
        required_degree[first] += 1
        required_degree[second] += 1

    shorts = 0
    miswires = 0
    for first, second in observed - required:
        if (
            required_degree[first]
            and required_degree[second]
            and confirmed_degree[first] == 1
            and confirmed_degree[second] == 1
        ):
            miswires += 1
        else:
            shorts += 1
    return shorts, miswires


def confirmed_pair_summary(
    observed: set[tuple[int, int]], j1_flipped: bool, j2_flipped: bool
) -> tuple[int, tuple[int, int], int, set[str]]:
    cross_pairs = 0
    local_pairs = [0, 0]
    detected_end_mask = 0
    conductive_contacts: set[str] = set()
    flips = (j1_flipped, j2_flipped)

    for first, second in observed:
        first_end = first // 24
        second_end = second // 24
        if first_end == second_end:
            local_pairs[first_end] += 1
            detected_end_mask |= 1 << first_end
            continue

        cross_pairs += 1
        for physical_endpoint in (first, second):
            end = physical_endpoint // 24
            contact_index = physical_endpoint % 24
            if flips[end]:
                contact_index = contact_index + 12 if contact_index < 12 else contact_index - 12
            conductive_contacts.add(CONTACTS[contact_index])

    return cross_pairs, (local_pairs[0], local_pairs[1]), detected_end_mask, conductive_contacts


def directed_power_summary(
    stable: set[tuple[int, int]], temporal: set[tuple[int, int]] | None = None
) -> tuple[int, int, int, int, int]:
    temporal = temporal or set()
    total = bidir = ground_source = vbus_source = temporal_pairs = 0
    for first in range(48):
        for second in range(first + 1, 48):
            first_contact = first % 24
            second_contact = second % 24
            if first_contact in GROUND_CONTACTS and second_contact in VBUS_CONTACTS:
                ground, vbus = first, second
            elif first_contact in VBUS_CONTACTS and second_contact in GROUND_CONTACTS:
                ground, vbus = second, first
            else:
                continue
            gnd_low = (ground, vbus) in stable
            vbus_low = (vbus, ground) in stable
            has_temporal = (ground, vbus) in temporal or (vbus, ground) in temporal
            if not (gnd_low or vbus_low or has_temporal):
                continue
            total += 1
            if gnd_low and vbus_low:
                bidir += 1
            elif gnd_low:
                ground_source += 1
            elif vbus_low:
                vbus_source += 1
            if has_temporal:
                temporal_pairs += 1
    return total, bidir, ground_source, vbus_source, temporal_pairs


def directed_from_confirmed(pairs: set[tuple[int, int]]) -> set[tuple[int, int]]:
    directed: set[tuple[int, int]] = set()
    for first, second in pairs:
        directed.add((first, second))
        directed.add((second, first))
    return directed


def physical_endpoint(name: str) -> int:
    end_name, contact = name.strip().split(".", 1)
    return (0 if end_name == "J1" else 1) * 24 + CONTACT_INDEX[contact]


def parse_directed_matrix(text: str) -> set[tuple[int, int]]:
    edges: set[tuple[int, int]] = set()
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        source_name, targets_text = line.split("=", 1)
        source = physical_endpoint(source_name)
        for target_name in targets_text.split(","):
            edges.add((source, physical_endpoint(target_name)))
    return edges


def logical_contact_index(physical: int, j1_flipped: bool, j2_flipped: bool) -> int:
    contact = physical % 24
    flipped = j1_flipped if physical < 24 else j2_flipped
    if flipped:
        contact = contact + 12 if contact < 12 else contact - 12
    return contact


def directed_profile_analysis(
    stable: set[tuple[int, int]],
    temporal: set[tuple[int, int]],
    kind: str,
    j1_flipped: bool,
    j2_flipped: bool,
) -> dict[str, int | str | bool]:
    required, allowed = build_profile_rules(kind, j1_flipped, j2_flipped)
    marked = kind in {"USB2_EMARKED", "FULL_UNMARKED", "FULL_EMARKED"}
    missing = unexpected = unstable = asymmetric = marker_evidence = confirmed_cross = 0
    for first in range(48):
        for second in range(first + 1, 48):
            pair = (first, second)
            forward = (first, second) in stable
            reverse = (second, first) in stable
            confirmed = forward and reverse
            is_asymmetric = forward != reverse
            is_temporal = (first, second) in temporal or (second, first) in temporal
            is_required = pair in required
            is_allowed = pair in allowed
            first_contact = logical_contact_index(first, j1_flipped, j2_flipped)
            second_contact = logical_contact_index(second, j1_flipped, j2_flipped)
            marker_topology = marked and not is_required and (
                (first_contact in CC_CONTACTS and second_contact in GROUND_CONTACTS)
                or (first_contact in GROUND_CONTACTS and second_contact in CC_CONTACTS)
                or (first_contact in CC_CONTACTS and second_contact in CC_CONTACTS)
            )
            marker_path = marker_topology and not is_temporal and (
                is_asymmetric or (confirmed and is_allowed)
            )
            optional_asymmetry = marker_topology and is_asymmetric and not is_temporal

            asymmetric += int(is_asymmetric)
            marker_evidence += int(marker_path)
            unstable += int((is_asymmetric and not optional_asymmetry) or is_temporal)
            missing += int(is_required and not confirmed)
            unexpected += int(confirmed and not is_temporal and not is_allowed)
            confirmed_cross += int(confirmed and (first // 24 != second // 24))

    power_total, power_bidir, power_gnd, power_vbus, power_temporal = directed_power_summary(
        stable, temporal
    )
    score = missing * 100 + unexpected * 200 + unstable * 50 + power_total * 1000
    power_fault = power_bidir + power_gnd
    power_suspect = power_vbus + power_temporal
    return {
        "kind": kind,
        "j1_flipped": j1_flipped,
        "j2_flipped": j2_flipped,
        "missing": missing,
        "unexpected": unexpected,
        "short": unexpected,
        "unstable": unstable,
        "asymmetric": asymmetric,
        "marker": marker_evidence,
        "power": power_total,
        "confirmed_cross": confirmed_cross,
        "score": score,
        "result": result_code(kind, missing, unexpected, unstable, power_fault, power_suspect),
    }


def best_directed_profile(
    stable: set[tuple[int, int]], temporal: set[tuple[int, int]], kind: str
) -> dict[str, int | str | bool]:
    candidates = []
    for flip_mask in range(4):
        candidate = directed_profile_analysis(
            stable,
            temporal,
            kind,
            bool(flip_mask & 1),
            bool(flip_mask & 2),
        )
        candidates.append((candidate, flip_mask))
    return min(
        candidates,
        key=lambda item: (
            item[0]["score"],
            item[0]["unexpected"],
            item[0]["missing"],
            item[0]["unstable"],
            item[1],
        ),
    )[0]


def auto_detect(
    stable: set[tuple[int, int]], temporal: set[tuple[int, int]] | None = None
) -> dict[str, int | str | bool]:
    temporal = temporal or set()
    candidates = [
        best_directed_profile(stable, temporal, kind)
        for kind in ("USB2_UNMARKED", "USB2_EMARKED", "FULL_EMARKED")
    ]
    return min(
        enumerate(candidates),
        key=lambda item: (
            item[1]["score"],
            item[1]["unexpected"],
            item[1]["missing"],
            item[1]["unstable"],
            item[0],
        ),
    )[1]


def scan_progress_contacts(completed_source_count: int) -> set[int]:
    completed = min(max(completed_source_count, 0), 48)
    if completed <= 24:
        return set(range(completed))
    return set(range(completed - 24, 24))


def best_orientation(observed: set[tuple[int, int]], kind: str) -> tuple[int, bool, bool]:
    candidates = []
    for j1_flipped in (False, True):
        for j2_flipped in (False, True):
            required, allowed = build_profile_rules(kind, j1_flipped, j2_flipped)
            candidates.append((*analyze(observed, required, allowed), j1_flipped, j2_flipped))
    best = min(candidates, key=lambda item: item[2])
    return best[2], best[3], best[4]


class FirmwareTopologyTests(unittest.TestCase):
    def test_v03_empty_and_one_end_presence_contract(self) -> None:
        self.assertEqual(confirmed_pair_summary(set(), False, False), (0, (0, 0), 0, set()))

        one_end: set[tuple[int, int]] = set()
        for contacts in (("A1", "B1", "A12", "B12"), ("A4", "B4", "A9", "B9")):
            add_group(one_end, [endpoint(0, contact, False) for contact in contacts])
        self.assertEqual(
            confirmed_pair_summary(one_end, False, False),
            (0, (12, 0), 0x01, set()),
        )

        both_ends = set(one_end)
        for contacts in (("A1", "B1", "A12", "B12"), ("A4", "B4", "A9", "B9")):
            add_group(both_ends, [endpoint(1, contact, False) for contact in contacts])
        self.assertEqual(
            confirmed_pair_summary(both_ends, False, False),
            (0, (12, 12), 0x03, set()),
        )

    def test_v03_cross_end_bitmap_is_orientation_normalized(self) -> None:
        for j1_flipped in (False, True):
            for j2_flipped in (False, True):
                observed = {
                    tuple(
                        sorted(
                            (
                                endpoint(0, "A6", j1_flipped),
                                endpoint(1, "A6", j2_flipped),
                            )
                        )
                    )
                }
                self.assertEqual(
                    confirmed_pair_summary(observed, j1_flipped, j2_flipped),
                    (1, (0, 0), 0, {"A6"}),
                )

    def test_v03_usb2_bitmap_has_only_real_logical_plug_contacts(self) -> None:
        expected_contacts = {
            "A1",
            "A4",
            "A5",
            "A6",
            "A7",
            "A9",
            "A12",
            "B1",
            "B4",
            "B9",
            "B12",
        }
        for j1_flipped in (False, True):
            for j2_flipped in (False, True):
                observed = build_profile("USB2_UNMARKED", j1_flipped, j2_flipped)
                self.assertEqual(
                    confirmed_pair_summary(observed, j1_flipped, j2_flipped),
                    (35, (12, 12), 0x03, expected_contacts),
                )

    def test_full_feature_orientation_autodetect(self) -> None:
        for j1_flipped in (False, True):
            for j2_flipped in (False, True):
                observed = build_profile("FULL_EMARKED", j1_flipped, j2_flipped)
                score, detected_j1, detected_j2 = best_orientation(observed, "FULL_EMARKED")
                self.assertEqual(score, 0)
                self.assertEqual((detected_j1, detected_j2), (j1_flipped, j2_flipped))

    def test_usb2_does_not_require_superspeed_pairs(self) -> None:
        observed = build_profile("USB2_UNMARKED", False, False)
        missing, unexpected, score = analyze(observed, build_profile("USB2_UNMARKED", False, False))
        self.assertEqual((missing, unexpected, score), (0, 0, 0))
        full_missing, _, _ = analyze(observed, build_profile("FULL_UNMARKED", False, False))
        self.assertGreater(full_missing, 0)

    def test_emarker_vconn_is_optional_and_ra_paths_are_allowed(self) -> None:
        required, allowed = build_profile_rules("FULL_EMARKED", False, False)
        b5_pair = tuple(sorted((endpoint(0, "B5", False), endpoint(1, "B5", False))))
        local_ra = tuple(sorted((endpoint(0, "B5", False), endpoint(0, "A1", False))))

        self.assertNotIn(b5_pair, required)
        self.assertIn(b5_pair, allowed)
        self.assertNotIn(local_ra, required)
        self.assertIn(local_ra, allowed)

        observed = set(required)
        emarker_network = [
            *(endpoint(0, contact, False) for contact in ("A1", "B1", "A12", "B12")),
            *(endpoint(1, contact, False) for contact in ("A1", "B1", "A12", "B12")),
            endpoint(0, "B5", False),
            endpoint(1, "B5", False),
        ]
        add_group(observed, emarker_network)
        self.assertEqual(analyze(observed, required, allowed), (0, 0, 0))

    def test_full_feature_conductors_never_claim_emarker_compliance(self) -> None:
        for kind in ("FULL_UNMARKED", "FULL_EMARKED"):
            required, allowed = build_profile_rules(kind, False, False)
            missing, unexpected, _ = analyze(set(required), required, allowed)
            self.assertEqual(
                result_code(kind, missing, unexpected),
                "CONDUCTORS_PASS_EMARKER_UNVERIFIED",
            )

    def test_usb2_marked_and_unmarked_results_remain_distinct(self) -> None:
        self.assertEqual(result_code("USB2_UNMARKED", 0, 0), "PASS")
        self.assertEqual(
            result_code("USB2_EMARKED", 0, 0),
            "CONDUCTORS_PASS_EMARKER_UNVERIFIED",
        )

    def test_open_and_short_fault_injection(self) -> None:
        required = build_profile("FULL_EMARKED", False, False)
        observed = set(required)
        dp = tuple(sorted((endpoint(0, "A6", False), endpoint(1, "A6", False))))
        observed.remove(dp)
        observed.add(tuple(sorted((endpoint(0, "A6", False), endpoint(1, "A7", False)))))
        missing, unexpected, _ = analyze(observed, required)
        self.assertEqual(missing, 1)
        self.assertEqual(unexpected, 1)

    def test_short_and_miswire_are_logged_separately(self) -> None:
        required = build_profile("FULL_EMARKED", False, False)
        dp = tuple(sorted((endpoint(0, "A6", False), endpoint(1, "A6", False))))
        dn = tuple(sorted((endpoint(0, "A7", False), endpoint(1, "A7", False))))
        cross_dp = tuple(sorted((endpoint(0, "A6", False), endpoint(1, "A7", False))))
        cross_dn = tuple(sorted((endpoint(0, "A7", False), endpoint(1, "A6", False))))

        swapped = (required - {dp, dn}) | {cross_dp, cross_dn}
        self.assertEqual(classify_unexpected(swapped, required), (0, 2))

        shorted = required | {cross_dp}
        self.assertEqual(classify_unexpected(shorted, required), (1, 0))

    def test_power_groups_include_all_parallel_contacts(self) -> None:
        graph = build_profile("USB2_UNMARKED", False, False)
        ground_members = [
            *(endpoint(0, contact, False) for contact in ("A1", "B1", "A12", "B12")),
            *(endpoint(1, contact, False) for contact in ("A1", "B1", "A12", "B12")),
        ]
        ground_pairs = {
            tuple(sorted((ground_members[first], ground_members[second])))
            for first in range(8)
            for second in range(first + 1, 8)
        }
        self.assertTrue(ground_pairs.issubset(graph))
        self.assertEqual(len(ground_pairs), 28)

    def test_discovery_and_straight_fixture_modes_are_preserved(self) -> None:
        discovery_required, discovery_allowed = build_profile_rules("DISCOVERY", False, False)
        self.assertFalse(discovery_required)
        self.assertEqual(len(discovery_allowed), (48 * 47) // 2)
        self.assertEqual(result_code("DISCOVERY", 0, 0), "DISCOVERY")

        straight_required, straight_allowed = build_profile_rules("STRAIGHT24", False, False)
        self.assertEqual(len(straight_required), 24)
        self.assertEqual(straight_required, straight_allowed)
        self.assertEqual(result_code("STRAIGHT24", 0, 0), "PASS")

    def test_c_source_contains_usb_if_full_feature_pair_table(self) -> None:
        source = (Path(__file__).parents[1] / "src" / "cable_profile.c").read_text(encoding="utf-8")
        body = source.split("static void add_full_feature_wires", 1)[1].split("static bool text_equals", 1)[0]
        found = re.findall(
            r"connect_wire\(profile, TESTER_CONTACT_([AB]\d+), TESTER_CONTACT_([AB]\d+)\)", body
        )
        expected = [
            ("A2", "B11"),
            ("A3", "B10"),
            ("B11", "A2"),
            ("B10", "A3"),
            ("B2", "A11"),
            ("B3", "A10"),
            ("A11", "B2"),
            ("A10", "B3"),
            ("A8", "B8"),
            ("B8", "A8"),
        ]
        self.assertEqual(found, expected)

    def test_c_source_does_not_require_vconn_wire_and_full_alias_is_safe(self) -> None:
        source = (Path(__file__).parents[1] / "src" / "cable_profile.c").read_text(encoding="utf-8")
        self.assertNotIn(
            "connect_wire(profile, TESTER_CONTACT_B5, TESTER_CONTACT_B5)",
            source,
        )
        self.assertRegex(
            source,
            r'if \(text_equals\(text, "FULL"\)\) \{\s*\*kind = CABLE_KIND_FULL_EMARKED;',
        )

    def test_default_app_uses_auto_and_exposes_unverified_full_feature_result(self) -> None:
        root = Path(__file__).parents[1]
        app_source = (root / "src" / "tester_app.c").read_text(encoding="utf-8")
        analysis_source = (root / "src" / "cable_analysis.c").read_text(encoding="utf-8")
        main_source = (root / "target" / "stm32c071" / "src" / "main.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("app->selected_kind = CABLE_KIND_AUTO", app_source)
        self.assertIn("cable_analyze_auto", app_source)
        self.assertIn('PROFILE=AUTO\\r\\n"', main_source)
        self.assertIn("CABLE_RESULT_CONDUCTORS_PASS_EMARKER_UNVERIFIED", app_source)
        self.assertIn('return "CONDUCTORS_PASS_EMARKER_UNVERIFIED"', analysis_source)

    def test_v03_c_result_contract_and_names_are_exposed(self) -> None:
        root = Path(__file__).parents[1]
        header = (root / "include" / "cable_analysis.h").read_text(encoding="utf-8")
        source = (root / "src" / "cable_analysis.c").read_text(encoding="utf-8")

        self.assertIn("CABLE_RESULT_NO_CONNECTION", header)
        self.assertIn("CABLE_RESULT_ONE_END_ONLY", header)
        self.assertRegex(header, r"uint8_t\s+detected_end_mask\s*;")
        self.assertRegex(header, r"uint16_t\s+confirmed_cross_pair_count\s*;")
        self.assertRegex(
            header,
            r"uint16_t\s+confirmed_local_pair_count\s*\[\s*TESTER_END_COUNT\s*\]\s*;",
        )
        self.assertRegex(
            header,
            r"uint8_t\s+conductive_contact_bitmap\s*\[\s*TESTER_CONTACT_BITMAP_BYTES\s*\]\s*;",
        )
        self.assertIn('return "NO_CONNECTION"', source)
        self.assertIn('return "ONE_END_ONLY"', source)

    def test_v03_operator_channel_leds_use_conductive_bitmap(self) -> None:
        app_source = (Path(__file__).parents[1] / "src" / "tester_app.c").read_text(
            encoding="utf-8"
        )
        body = app_source.split("static bool apply_result_outputs", 1)[1].split(
            "static void write_last_report", 1
        )[0]
        self.assertIn("conductive_contact_bitmap", body)
        self.assertNotIn("fault_contact_bitmap", body)
        self.assertEqual(body.count("signal_bitmap = no_signals"), 1)
        fail_safe_body = body.split("Only an invalid/incomplete hardware scan", 1)[1]
        self.assertIn("CABLE_RESULT_HARDWARE_ERROR", fail_safe_body)
        self.assertNotIn("CABLE_RESULT_UNSTABLE", fail_safe_body)
        self.assertNotIn("CABLE_RESULT_POWER_CROSS_FAULT", fail_safe_body)
        self.assertNotIn("CABLE_RESULT_POWER_CROSS_SUSPECT", fail_safe_body)

        suspect_body = braced_block_after(body, "CABLE_RESULT_POWER_CROSS_SUSPECT")
        self.assertNotIn("short_on = true", suspect_body)
        self.assertNotIn("open_on = true", suspect_body)
        self.assertNotIn("pass_on = true", suspect_body)

        unverified_body = braced_block_after(
            body, "CABLE_RESULT_CONDUCTORS_PASS_EMARKER_UNVERIFIED"
        )
        self.assertIn("pass_on = true", unverified_body)
        self.assertNotIn("short_on = true", unverified_body)
        self.assertNotIn("open_on = true", unverified_body)

    def test_v03_no_connection_keeps_status_and_buzzer_quiet(self) -> None:
        app_source = (Path(__file__).parents[1] / "src" / "tester_app.c").read_text(
            encoding="utf-8"
        )
        output_body = app_source.split("static bool apply_result_outputs", 1)[1].split(
            "static void write_last_report", 1
        )[0]
        no_connection_output = braced_block_after(output_body, "CABLE_RESULT_NO_CONNECTION")
        self.assertNotIn("pass_on = true", no_connection_output)
        self.assertNotIn("short_on = true", no_connection_output)
        self.assertNotIn("open_on = true", no_connection_output)

        buzzer_body = app_source.split("static void start_buzzer_pattern", 1)[1].split(
            "static void tick_buzzer", 1
        )[0]
        self.assertIn("CABLE_RESULT_NO_CONNECTION", buzzer_body)
        no_connection_buzzer = braced_block_after(buzzer_body, "CABLE_RESULT_NO_CONNECTION")
        self.assertIn("set_buzzer(app, false)", no_connection_buzzer)
        self.assertIn("return", no_connection_buzzer)

    def test_pcal_drive_uses_break_before_make_order(self) -> None:
        source = (Path(__file__).parents[1] / "src" / "pcal6524.c").read_text(encoding="utf-8")
        body = source.split("bool pcal6524_drive_one_low", 1)[1].split("bool pcal6524_read_inputs", 1)[0]
        make_input = body.index("pcal6524_set_all_inputs")
        write_latch = body.index("PCAL6524_REG_OUTPUT_PORT_0")
        enable_output = body.index("PCAL6524_REG_CONFIGURATION_PORT_0")
        self.assertLess(make_input, write_latch)
        self.assertLess(write_latch, enable_output)

    def test_scan_error_path_releases_both_expanders(self) -> None:
        source = (Path(__file__).parents[1] / "src" / "tester_scan.c").read_text(encoding="utf-8")
        release_body = source.split("static void release_all_inputs", 1)[1].split("static void fail_scan", 1)[0]
        self.assertIn("TESTER_END_J1", release_body)
        self.assertIn("TESTER_END_J2", release_body)
        fail_body = source.split("static void fail_scan", 1)[1].split("static void collect_sample", 1)[0]
        self.assertIn("release_all_inputs(scan)", fail_body)

    def test_v032_release_distinguishes_gpio_direction_from_dut_recovery(self) -> None:
        source = (Path(__file__).parents[1] / "src" / "tester_scan.c").read_text(encoding="utf-8")
        release_body = source.split("case TESTER_SCAN_RELEASE:", 1)[1].split(
            "case TESTER_SCAN_RELEASE_SETTLE:", 1
        )[0]
        check_body = source.split("case TESTER_SCAN_RELEASE_CHECK:", 1)[1].split(
            "case TESTER_SCAN_UNINITIALIZED:", 1
        )[0]

        self.assertIn("pcal6524_read_configuration", release_body)
        self.assertIn("TESTER_SCAN_ERROR_RELEASE_CONFIG_FAILED", release_body)
        self.assertIn("release_has_new_low", check_body)
        self.assertIn("TESTER_SCAN_ERROR_DUT_SETTLE_TIMEOUT", check_body)
        self.assertIn("release_retry_interval_ms", check_body)
        self.assertNotIn("memcmp", check_body)

    def test_v032_commits_only_stable_votes_after_release_confirmation(self) -> None:
        source = (Path(__file__).parents[1] / "src" / "tester_scan.c").read_text(encoding="utf-8")
        vote_body = source.split("static void commit_source_votes", 1)[1].split(
            "tester_scan_config_t tester_scan_default_config", 1
        )[0]
        sample_body = source.split("case TESTER_SCAN_SAMPLE:", 1)[1].split(
            "case TESTER_SCAN_RELEASE:", 1
        )[0]
        check_body = source.split("case TESTER_SCAN_RELEASE_CHECK:", 1)[1].split(
            "case TESTER_SCAN_UNINITIALIZED:", 1
        )[0]

        self.assertIn("votes == scan->config.samples_per_source", vote_body)
        self.assertNotIn("commit_source_votes", sample_body)
        self.assertLess(check_body.index("release_stable_count"), check_body.index("commit_source_votes"))

    def test_v033_uses_extended_settle_only_for_power_and_configuration_contacts(self) -> None:
        source = (Path(__file__).parents[1] / "src" / "tester_scan.c").read_text(encoding="utf-8")
        helper = source.split("static bool contact_needs_power_settle", 1)[1].split(
            "static bool release_has_new_low", 1
        )[0]
        drive = source.split("case TESTER_SCAN_DRIVE:", 1)[1].split(
            "case TESTER_SCAN_SETTLE:", 1
        )[0]

        for contact in ("A1", "A4", "A5", "A9", "A12", "B1", "B4", "B5", "B9", "B12"):
            self.assertIn(f"TESTER_CONTACT_{contact}", helper)
        for contact in ("A2", "A6", "A7", "A8", "B2", "B6", "B7", "B8"):
            self.assertNotIn(f"TESTER_CONTACT_{contact}", helper)
        self.assertIn("power_settle_time_ms", drive)
        self.assertIn("settle_time_ms", drive)

    def test_v034_directed_power_cross_is_profile_independent_and_highest_priority(self) -> None:
        root = Path(__file__).parents[1]
        header = (root / "include" / "cable_analysis.h").read_text(encoding="utf-8")
        analysis = (root / "src" / "cable_analysis.c").read_text(encoding="utf-8")
        report = (root / "src" / "tester_report.c").read_text(encoding="utf-8")
        app = (root / "src" / "tester_app.c").read_text(encoding="utf-8")

        selector = analysis.split("static cable_result_code_t select_result_code", 1)[1].split(
            "bool cable_analyze", 1
        )[0]
        self.assertIn("CABLE_RESULT_POWER_CROSS_FAULT", header)
        self.assertIn("CABLE_RESULT_POWER_CROSS_SUSPECT", header)
        for field in (
            "asymmetric_pair_count",
            "temporal_unstable_pair_count",
            "power_cross_pair_count",
            "power_cross_bidir_pair_count",
            "power_cross_gnd_source_to_vbus_pair_count",
            "power_cross_vbus_source_to_gnd_pair_count",
            "power_cross_temporal_pair_count",
        ):
            self.assertIn(field, header)
        self.assertIn("confirmed_power_cross_pair_count", header)
        self.assertLess(
            selector.index("power_cross_fault"),
            selector.index("CABLE_KIND_DISCOVERY"),
        )
        self.assertLess(
            selector.index("power_cross_suspect"),
            selector.index("CABLE_KIND_DISCOVERY"),
        )
        self.assertIn("contacts_cross_power_rails", analysis)
        for label in (
            "ASYMMETRIC_PAIRS",
            "TEMPORAL_UNSTABLE_PAIRS",
            "POWER_CROSS_PAIRS",
            "POWER_CROSS_BIDIR_PAIRS",
            "POWER_CROSS_GND_SOURCE_TO_VBUS_PAIRS",
            "POWER_CROSS_VBUS_SOURCE_TO_GND_PAIRS",
            "POWER_CROSS_TEMPORAL_PAIRS",
            "POWER_CROSS_ENDPOINTS",
        ):
            self.assertIn(label, report)
        self.assertIn("CABLE_RESULT_POWER_CROSS_FAULT", app)
        self.assertIn("CABLE_RESULT_POWER_CROSS_SUSPECT", app)
        self.assertEqual(
            result_code("DISCOVERY", 0, 0, power_fault=1),
            "POWER_CROSS_FAULT",
        )
        self.assertEqual(
            result_code("DISCOVERY", 0, 0, power_suspect=1),
            "POWER_CROSS_SUSPECT",
        )
        self.assertEqual(result_code("FULL_EMARKED", 1, 1, 1), "OPEN_AND_SHORT")

    def test_v034_directed_power_pair_counts_are_unique_and_directional(self) -> None:
        ground = endpoint(1, "A1", False)
        vbus = endpoint(1, "A4", False)

        self.assertEqual(directed_power_summary({(ground, vbus), (vbus, ground)}), (1, 1, 0, 0, 0))
        self.assertEqual(directed_power_summary({(ground, vbus)}), (1, 0, 1, 0, 0))
        self.assertEqual(directed_power_summary({(vbus, ground)}), (1, 0, 0, 1, 0))
        self.assertEqual(directed_power_summary(set(), {(vbus, ground)}), (1, 0, 0, 0, 1))
        self.assertEqual(
            directed_power_summary({(vbus, ground)}, {(ground, vbus)}),
            (1, 0, 0, 1, 1),
        )

        stable = {
            (endpoint(1, vbus_name, False), endpoint(1, ground_name, False))
            for vbus_name in ("A4", "A9", "B4", "B9")
            for ground_name in ("A1", "A12", "B1", "B12")
        }
        self.assertEqual(directed_power_summary(stable), (16, 0, 0, 16, 0))

    def test_v035_exact_full_cable_fixture_auto_detects_full_without_faults(self) -> None:
        stable = parse_directed_matrix(FULL_CABLE_REAL_MATRIX)
        detected = auto_detect(stable)

        self.assertEqual(detected["kind"], "FULL_EMARKED")
        self.assertEqual((detected["j1_flipped"], detected["j2_flipped"]), (False, True))
        for metric in ("missing", "unexpected", "short", "unstable", "power"):
            self.assertEqual(detected[metric], 0, metric)
        self.assertEqual(detected["asymmetric"], 20)
        self.assertEqual(detected["confirmed_cross"], 54)
        self.assertGreater(detected["marker"], 0)
        self.assertEqual(detected["result"], "CONDUCTORS_PASS_EMARKER_UNVERIFIED")

    def test_v035_auto_selects_new_usb2_and_rejects_old_electronic_usb2_pass(self) -> None:
        required, _ = build_profile_rules("USB2_UNMARKED", False, False)
        new_usb2 = directed_from_confirmed(required)
        detected = auto_detect(new_usb2)
        self.assertEqual(detected["kind"], "USB2_UNMARKED")
        self.assertEqual(detected["result"], "PASS")

        old_usb2 = set(new_usb2)
        old_usb2.add((endpoint(0, "B5", False), endpoint(0, "A1", False)))
        detected = auto_detect(old_usb2)
        self.assertEqual(detected["kind"], "USB2_EMARKED")
        self.assertEqual(detected["marker"], 1)
        self.assertEqual(detected["unstable"], 0)
        self.assertEqual(detected["result"], "CONDUCTORS_PASS_EMARKER_UNVERIFIED")
        self.assertNotEqual(detected["result"], "PASS")

    def test_v035_emarker_electronic_paths_separate_stable_temporal_and_short(self) -> None:
        required, _ = build_profile_rules("FULL_EMARKED", False, False)
        stable = directed_from_confirmed(required)
        b5 = endpoint(0, "B5", False)
        a5 = endpoint(0, "A5", False)
        ground = endpoint(0, "A1", False)

        one_way = set(stable) | {(b5, ground)}
        summary = directed_profile_analysis(one_way, set(), "FULL_EMARKED", False, False)
        self.assertEqual((summary["marker"], summary["unstable"], summary["unexpected"]), (1, 0, 0))

        allowed_bidir = one_way | {(ground, b5)}
        summary = directed_profile_analysis(
            allowed_bidir, set(), "FULL_EMARKED", False, False
        )
        self.assertEqual((summary["marker"], summary["unstable"], summary["unexpected"]), (1, 0, 0))

        unallowed_bidir = set(stable) | {(a5, ground), (ground, a5)}
        summary = directed_profile_analysis(
            unallowed_bidir, set(), "FULL_EMARKED", False, False
        )
        self.assertEqual((summary["marker"], summary["unstable"], summary["unexpected"]), (0, 0, 1))
        self.assertEqual(summary["result"], "SHORT_OR_MISWIRE")

        summary = directed_profile_analysis(
            one_way, {(b5, ground)}, "FULL_EMARKED", False, False
        )
        self.assertEqual((summary["marker"], summary["unstable"], summary["unexpected"]), (0, 1, 0))
        self.assertEqual(summary["result"], "UNSTABLE")

    def test_v035_auto_command_report_and_physical_button_contract(self) -> None:
        root = Path(__file__).parents[1]
        profile_header = (root / "include" / "cable_profile.h").read_text(encoding="utf-8")
        analysis_header = (root / "include" / "cable_analysis.h").read_text(encoding="utf-8")
        analysis = (root / "src" / "cable_analysis.c").read_text(encoding="utf-8")
        app = (root / "src" / "tester_app.c").read_text(encoding="utf-8")
        report = (root / "src" / "tester_report.c").read_text(encoding="utf-8")

        self.assertIn("CABLE_KIND_AUTO", profile_header)
        self.assertIn("cable_analyze_auto", analysis_header)
        key_body = app.split("static void update_key", 1)[1].split("bool tester_app_init", 1)[0]
        self.assertIn("request_start_with_kind(app, CABLE_KIND_AUTO", key_body)
        self.assertNotIn("tester_app_request_start(app", key_body)
        self.assertIn('"AUTO DISCOVERY USB2_UNMARKED', app)
        self.assertIn("REQUESTED_PROFILE", report)
        self.assertIn("DETECTED_PROFILE", report)
        self.assertIn("EMARKER_ELECTRONIC_PATH_PAIRS", report)
        auto_body = analysis.split("bool cable_analyze_auto", 1)[1].split(
            "const char *cable_result_name", 1
        )[0]
        self.assertIn("CABLE_RESULT_NO_CONNECTION", auto_body)
        self.assertIn("CABLE_RESULT_ONE_END_ONLY", auto_body)
        self.assertIn("best.kind = CABLE_KIND_AUTO", auto_body)

    def test_v035_status_leds_only_show_confirmed_pass_open_and_short(self) -> None:
        root = Path(__file__).parents[1]
        app = (root / "src" / "tester_app.c").read_text(encoding="utf-8")
        body = app.split("static bool apply_result_outputs", 1)[1].split(
            "static void write_last_report", 1
        )[0]
        for code in (
            "CABLE_RESULT_UNSTABLE",
            "CABLE_RESULT_POWER_CROSS_SUSPECT",
            "CABLE_RESULT_HARDWARE_ERROR",
        ):
            block = braced_block_after(body, code)
            self.assertNotIn("pass_on = true", block)
            self.assertNotIn("short_on = true", block)
            self.assertNotIn("open_on = true", block)
        fault = braced_block_after(body, "CABLE_RESULT_POWER_CROSS_FAULT")
        self.assertIn("short_on = true", fault)
        self.assertIn("missing_pair_count != 0u", fault)

        analysis = (root / "src" / "cable_analysis.c").read_text(encoding="utf-8")
        selector = analysis.split("static cable_result_code_t select_result_code", 1)[1].split(
            "bool cable_analyze", 1
        )[0]
        open_index = selector.index("CABLE_RESULT_OPEN")
        self.assertIn("missing != 0u", selector[max(0, open_index - 100) : open_index])

    def test_v035_power_evidence_does_not_pollute_unexpected_endpoints(self) -> None:
        source = (Path(__file__).parents[1] / "src" / "cable_analysis.c").read_text(
            encoding="utf-8"
        )
        power_block = source.split("if (is_power_cross &&", 1)[1].split(
            "if (confirmed && !unstable)", 1
        )[0]
        self.assertIn("power_cross_endpoint_bitmap", power_block)
        self.assertNotIn("unexpected_endpoint_bitmap", power_block)
        unexpected_block = source.split("if (confirmed && !temporal_unstable && !allowed)", 1)[1]
        self.assertIn("unexpected_endpoint_bitmap", unexpected_block)

    def test_v036_scan_progress_is_commit_driven_and_two_phase(self) -> None:
        self.assertEqual(scan_progress_contacts(0), set())
        self.assertEqual(scan_progress_contacts(1), {0})
        self.assertEqual(scan_progress_contacts(24), set(range(24)))
        self.assertEqual(scan_progress_contacts(25), set(range(1, 24)))
        self.assertEqual(scan_progress_contacts(47), {23})
        self.assertEqual(scan_progress_contacts(48), set())

        root = Path(__file__).parents[1]
        app = (root / "src" / "tester_app.c").read_text(encoding="utf-8")
        progress = app.split("static bool update_scan_progress", 1)[1].split(
            "static void add_buzzer_segment", 1
        )[0]
        self.assertIn("completed_source_count <= app->displayed_completed_source_count", progress)
        self.assertEqual(progress.count("pcal6524_write_led_bitmap"), 1)
        self.assertLess(
            progress.index("displayed_completed_source_count = completed_source_count"),
            progress.index("pcal6524_write_led_bitmap"),
        )
        self.assertIn("tester_scan_fail_external", progress)
        self.assertIn("TESTER_SCAN_ERROR_LED_OUTPUT", progress)
        self.assertIn("return false", progress)

        tick = app.split("void tester_app_tick", 1)[1].split(
            "bool tester_app_request_start", 1
        )[0]
        self.assertLess(tick.index("tester_scan_tick"), tick.index("update_scan_progress"))
        self.assertLess(tick.index("update_scan_progress"), tick.index("tester_scan_finished"))
        self.assertGreaterEqual(tick.count("complete_scan"), 2)

        scan = (root / "src" / "tester_scan.c").read_text(encoding="utf-8")
        release = scan.split("case TESTER_SCAN_RELEASE_CHECK:", 1)[1].split(
            "case TESTER_SCAN_UNINITIALIZED:", 1
        )[0]
        self.assertLess(release.index("commit_source_votes"), release.index("advance_source"))

    def test_v036_status_report_and_led_failure_contract(self) -> None:
        root = Path(__file__).parents[1]
        app = (root / "src" / "tester_app.c").read_text(encoding="utf-8")
        scan_header = (root / "include" / "tester_scan.h").read_text(encoding="utf-8")
        scan_source = (root / "src" / "tester_scan.c").read_text(encoding="utf-8")

        command = app.split("void tester_app_handle_command", 1)[1].split(
            "const char *tester_app_state_name", 1
        )[0]
        for token in (
            "DISPLAY_MODE=PROGRESS",
            "SCAN_PROGRESS=%u/%u",
            "SCAN_SOURCE=%s",
            "DISPLAY_MODE=RESULT",
            "ERR SCANNING",
        ):
            self.assertIn(token, command)
        report_block = command.split('strcmp(command, "REPORT")', 1)[1].split(
            'strcmp(command, "ABORT")', 1
        )[0]
        self.assertLess(report_block.index("TESTER_APP_SCANNING"), report_block.index("write_last_report"))

        complete = app.split("static void complete_scan", 1)[1].split(
            "static bool request_start_with_kind", 1
        )[0]
        self.assertIn("if (!apply_result_outputs(app))", complete)
        self.assertIn("CABLE_RESULT_HARDWARE_ERROR", complete)
        self.assertIn("TESTER_SCAN_ERROR_LED_OUTPUT", complete)

        self.assertIn("TESTER_SCAN_ERROR_LED_OUTPUT", scan_header)
        self.assertIn("tester_scan_fail_external", scan_header)
        self.assertIn('return "LED_OUTPUT"', scan_source)

    def test_v036_version_and_artifact_names_are_consistent(self) -> None:
        root = Path(__file__).parents[1]
        app_header = (root / "include" / "tester_app.h").read_text(encoding="utf-8")
        build = (root / "target" / "stm32c071" / "build.ps1").read_text(encoding="utf-8")
        flash = (root / "target" / "stm32c071" / "flash-stlink.ps1").read_text(
            encoding="utf-8"
        )

        self.assertIn('"0.3.6"', app_header)
        self.assertIn('"0.3.6-OFFICE-SILENT"', app_header)
        self.assertIn("v0.3.6", build)
        self.assertNotIn("v0.3.5", build)
        self.assertIn("v0.3.6", flash)
        self.assertNotIn("v0.3.5", flash)

    def test_board_usb_pinmap_preserves_swdio(self) -> None:
        source = (
            Path(__file__).parents[1] / "target" / "stm32c071" / "src" / "main.cpp"
        ).read_text(encoding="utf-8")
        pinmap = source.split("PinMap_USB_DRD_FS[]", 1)[1].split("};", 1)[0]
        self.assertIn("PA_11", pinmap)
        self.assertIn("PA_12", pinmap)
        self.assertNotIn("PA_4", pinmap)
        self.assertNotIn("PA_13", pinmap)
        self.assertNotIn("PA_15", pinmap)

    def test_office_silent_target_clamps_buzzer_low(self) -> None:
        root = Path(__file__).parents[1]
        main_source = (root / "target" / "stm32c071" / "src" / "main.cpp").read_text(
            encoding="utf-8"
        )
        platformio = (root / "target" / "stm32c071" / "platformio.ini").read_text(
            encoding="utf-8"
        )
        buzzer_body = main_source.split("void set_buzzer", 1)[1].split(
            "const tester_platform_t", 1
        )[0]
        self.assertIn("TYPEC_TESTER_OFFICE_SILENT", main_source)
        self.assertIn("(!kBuzzerMuted && enabled) ? HIGH : LOW", buzzer_body)
        self.assertIn("[env:office_silent]", platformio)
        self.assertIn("-DTYPEC_TESTER_OFFICE_SILENT=1", platformio)


if __name__ == "__main__":
    unittest.main()
