#!/usr/bin/env python3
"""Verify organic idle-mode reselection and restored PCH."""

import argparse
import pathlib
import re


LOCATION_UPDATE = re.compile(
    r"TX packet type=1b .*data=0080013f[0-9a-f]*"
    r"490508[0-9a-f]{12}3[03]080910101032547698"
)


def require_after(text: str, pattern: str, cursor: int, label: str) -> int:
    match = re.search(pattern, text[cursor:])
    if not match:
        raise ValueError(f"missing or out-of-order reselection checkpoint: {label}")
    return cursor + match.end()


def verify_same_lac(
        text: str, serving_arfcn: int = 1, neighbour_arfcn: int = 2,
        radio_profile: str = "nse8",
        allow_post_reselection_paging_access: bool = False) -> None:
    cursor = 0
    candidate = f"{neighbour_arfcn:04x}"
    if radio_profile in ("nhm5", "nhm6"):
        cursor = require_after(
            text,
            rf"neighbour measurement instruction arfcn={neighbour_arfcn} "
            r".*accepted=1",
            cursor, "firmware-published accepted neighbour instruction",
        )
    else:
        cursor = require_after(
            text, rf"neighbour measurement list .*first={candidate}", cursor,
            "firmware-published neighbour list",
        )
    cursor = require_after(
        text,
        rf"receiver tuned old_arfcn={serving_arfcn} "
        rf"new_arfcn={neighbour_arfcn}",
        cursor,
        "candidate receiver tune",
    )
    cursor = require_after(
        text,
        rf"serving cell selected old_arfcn={serving_arfcn} "
        rf"new_arfcn={neighbour_arfcn}",
        cursor,
        "serving-cell commit",
    )
    require_after(
        text,
        rf"RX enqueue type=80 payload=34 .*"
        rf"data=60[0-9a-f]{{2}}[0-9a-f]{{8}}{candidate}",
        cursor,
        f"ARFCN-{neighbour_arfcn} PCH after reselection",
    )

    updates = LOCATION_UPDATE.findall(text)
    if len(updates) != 1:
        raise ValueError(
            "same-LAC reselection must retain the initial registration without "
            f"a second Location Updating Request; observed {len(updates)}"
        )

    selection = text.find(
        f"serving cell selected old_arfcn={serving_arfcn} "
        f"new_arfcn={neighbour_arfcn}")
    if (not allow_post_reselection_paging_access and
            re.search(
                r"TX packet type=0c .*data=000[0-9a-f]{3}",
                text[selection:])):
        raise ValueError(
            "same-LAC reselection emitted a spurious CHANNEL REQUEST/MM access"
        )


def verify_different_lac(
        text: str, radio_profile: str = "nse8",
        serving_arfcn: int = 1, neighbour_arfcn: int = 2) -> None:
    initial_status = re.search(
        r"sim_device: update-binary fid=6f7e offset=10 length=1", text)
    if not initial_status:
        raise ValueError(
            "initial registration did not establish EF_LOCI status")

    cursor = 0
    candidate = f"{neighbour_arfcn:04x}"
    if radio_profile in ("nhm5", "nhm6"):
        cursor = require_after(
            text,
            rf"neighbour measurement instruction arfcn={neighbour_arfcn} "
            r".*accepted=1",
            cursor, "firmware-published accepted neighbour instruction",
        )
    else:
        cursor = require_after(
            text, rf"neighbour measurement list .*first={candidate}", cursor,
            "firmware-published neighbour list",
        )
    cursor = require_after(
        text,
        rf"receiver tuned old_arfcn={serving_arfcn} "
        rf"new_arfcn={neighbour_arfcn}",
        cursor,
        "candidate receiver tune",
    )
    si3_pattern = (
        r"radio_bcch_parse:.*49.*06.*1b.*00.*02.*00.*f1.*10.*00.*02"
        if radio_profile == "nse8"
        else rf"RX enqueue type=80 payload=34 .*"
        rf"data=50[0-9a-f]{{10}}{candidate}"
        r"000049061b000200f1100002"
    )
    cursor = require_after(
        text, si3_pattern, cursor, "cell-B SI3 with different LAC")
    cursor = require_after(
        text,
        rf"serving cell selected old_arfcn={serving_arfcn} "
        rf"new_arfcn={neighbour_arfcn}",
        cursor,
        "serving-cell commit",
    )
    if initial_status.end() >= cursor:
        raise ValueError(
            "EF_LOCI status was not established before reselection")
    cursor = require_after(
        text, r"TX packet type=1b .*data=0080013f490508", cursor,
        "firmware-owned post-reselection Location Updating Request",
    )
    cursor = require_after(
        text, r"LAPDm Location Updating Accept acknowledged", cursor,
        "Location Updating Accept acknowledgement",
    )
    cursor = require_after(
        text, r"sim_device: update-binary fid=6f7e offset=4 length=5", cursor,
        "EF_LOCI location update",
    )
    require_after(
        text,
        rf"RX enqueue type=80 payload=34 .*"
        rf"data=60[0-9a-f]{{2}}[0-9a-f]{{8}}{candidate}",
        cursor,
        "ARFCN-2 PCH after Location Updating",
    )

    requests = re.findall(r"TX packet type=1b .*data=0080013f490508", text)
    if len(requests) != 2:
        raise ValueError(
            "different-LAC lifecycle must contain initial registration and one "
            f"post-reselection Location Updating Request; observed {len(requests)}"
        )


def verify_loss_recovery(text: str, radio_profile: str = "nse8") -> None:
    cursor = 0
    cursor = require_after(
        text, r"neighbour measurement list .*first=0002", cursor,
        "firmware-published neighbour list before loss",
    )
    cursor = require_after(
        text, r"DOWNLINK_SIGNALLING_FAIL arfcn=1", cursor,
        "standards-counter serving-cell loss",
    )
    loss = cursor
    if radio_profile == "nhm2":
        cursor = require_after(
            text,
            r"TX packet type=02 payload=20 .*"
            r"data=0412020900000010600000011000000007297000",
            cursor, "firmware-owned serving-channel reconfiguration",
        )
        cursor = require_after(
            text,
            r"TX packet type=57 payload=2 .*data=0305",
            cursor, "NHM-2 serving-cell SCH request",
        )
    else:
        cursor = require_after(
            text,
            r"TX packet type=1a .*radio_phase=selected_search",
            cursor, "NSE-8 firmware-owned bitmap search",
        )
    cursor = require_after(
        text,
        r"RX enqueue type=80 payload=14 .*"
        r"data=401200[0-9a-f]{6}00010000[0-9a-f]{8}",
        cursor, "valid standards-encoded SCH after carrier recovery",
    )
    require_after(
        text,
        r"RX enqueue type=80 payload=34 .*data=6012[0-9a-f]{8}0001",
        cursor, "PCH monitoring after recovery",
    )

    if text.count("DOWNLINK_SIGNALLING_FAIL arfcn=1") != 1:
        raise ValueError("recovered serving carrier produced another loss report")
    if LOCATION_UPDATE.search(text[loss:]):
        raise ValueError("same-cell recovery emitted a spurious Location Updating Request")
    if "sim_device: update-binary fid=6f7e" in text[loss:]:
        raise ValueError("same-cell recovery mutated EF_LOCI")


def verify_all_cell_loss(text: str, radio_profile: str = "nse8") -> None:
    cursor = 0
    cursor = require_after(
        text, r"neighbour measurement list .*first=0002", cursor,
        "firmware-published neighbour list before loss",
    )
    cursor = require_after(
        text, r"DOWNLINK_SIGNALLING_FAIL arfcn=1", cursor,
        "standards-counter serving-cell loss",
    )
    loss = cursor
    if radio_profile == "nhm2":
        cursor = require_after(
            text, r"TX packet type=57 payload=2 .*data=0305", cursor,
            "NHM-2 serving-cell SCH request",
        )
        cursor = require_after(
            text, r"RX enqueue type=8a payload=8", cursor,
            "no power-synchronization word found",
        )
        cursor = require_after(
            text, r"RX enqueue type=8f payload=8", cursor,
            "finite synchronization search exhausted",
        )
    else:
        cursor = require_after(
            text, r"TX packet type=1a .*radio_phase=selected_search", cursor,
            "NSE-8 firmware-owned bitmap search",
        )
        cursor = require_after(
            text,
            r"RX enqueue type=8b payload=166 .*"
            r"data=001000010081ffff0081ffff",
            cursor, "measurement result containing no receivable carrier",
        )

    if re.search(r"RX enqueue type=80 payload=34 .*data=60", text[cursor:]):
        raise ValueError("decoded PCH continued after all usable cells were lost")
    if "serving cell selected " in text[loss:]:
        raise ValueError("all-cell loss falsely selected a serving cell")
    if LOCATION_UPDATE.search(text[loss:]):
        raise ValueError("all-cell loss emitted a spurious Location Updating Request")
    if "sim_device: update-binary fid=6f7e" in text[loss:]:
        raise ValueError("all-cell loss mutated EF_LOCI")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("trace", type=pathlib.Path)
    parser.add_argument(
        "--profile",
        choices=("same-lac", "different-lac", "loss-recovery", "all-cell-loss"),
        default="same-lac",
    )
    parser.add_argument(
        "--radio-profile",
        choices=("nse8", "nhm5", "nhm6", "nhm2"), default="nse8")
    parser.add_argument("--serving-arfcn", type=int, default=1)
    parser.add_argument("--neighbour-arfcn", type=int, default=2)
    parser.add_argument(
        "--paging-after-reselection", action="store_true",
        help="allow the separately checked Paging Response access after cell B")
    args = parser.parse_args()
    text = args.trace.read_text(errors="replace")
    if args.profile == "same-lac":
        verify_same_lac(
            text, args.serving_arfcn, args.neighbour_arfcn,
            args.radio_profile, args.paging_after_reselection)
        print("OK - organically reselected to same-LAC cell B and resumed PCH")
    elif args.profile == "different-lac":
        verify_different_lac(
            text, args.radio_profile, args.serving_arfcn,
            args.neighbour_arfcn)
        print(
            "OK - organically reselected to different-LAC cell B, updated "
            "location and resumed PCH")
    elif args.profile == "loss-recovery":
        verify_loss_recovery(text, args.radio_profile)
        print(
            "OK - organically detected serving loss, synchronized after RF "
            "recovery and resumed PCH without subscriber mutation")
    else:
        verify_all_cell_loss(text, args.radio_profile)
        print(
            "OK - organically detected all-cell loss, exhausted the evidenced "
            "search path and stopped PCH without subscriber mutation")


if __name__ == "__main__":
    main()
