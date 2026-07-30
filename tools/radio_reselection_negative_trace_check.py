#!/usr/bin/env python3
"""Verify an unsuitable neighbour cannot become the serving GSM cell."""

import argparse
import pathlib
import re


LOCATION_UPDATE = re.compile(r"TX packet type=1b .*data=0080013f490508")
LOCI_WRITE = re.compile(r"sim_device: update-binary fid=6f7e")


def verify(
        text: str, profile: str, serving_arfcn: int = 1,
        neighbour_arfcn: int = 2) -> None:
    candidate = f"{neighbour_arfcn:04x}"
    candidate_published = re.search(
        rf"neighbour measurement list .*first={candidate}", text)
    if profile != "unsupported-band" and not candidate_published:
        raise ValueError("firmware did not publish cell B as a neighbour")
    if profile != "unsupported-band" and not (
            re.search(
                rf"neighbour measurement instruction arfcn={neighbour_arfcn} "
                r".*accepted=1", text) or
            re.search(
                rf"receiver tuned old_arfcn={serving_arfcn} "
                rf"new_arfcn={neighbour_arfcn}", text)):
        raise ValueError("firmware did not organically inspect cell B")
    if profile == "unsupported-band" and (
            candidate_published or
            re.search(
                rf"receiver tuned .*new_arfcn={neighbour_arfcn}", text)):
        raise ValueError(
            "unsupported-band neighbour entered the handset candidate set")

    if profile == "barred":
        si = re.search(
            r"RX enqueue type=80 payload=34 .*data=5062[0-9a-f]{8}0002"
            r"000049061b000200f110000140000000000002", text)
    elif profile == "rxlev":
        si = re.search(
            r"RX enqueue type=80 payload=34 .*data=5062[0-9a-f]{8}0002"
            r"000049061b000200f110000140000000003f", text)
    elif profile == "malformed":
        si = re.search(
            rf"RX enqueue type=80 payload=34 .*"
            rf"data=50[0-9a-f]{{2}}01[0-9a-f]{{6}}{candidate}", text)
    elif profile == "forbidden":
        si = re.search(
            rf"RX enqueue type=80 payload=34 .*"
            rf"data=50[0-9a-f]{{10}}{candidate}"
            r"000049061b00021300620001", text)
    elif profile == "access-class":
        si = re.search(
            rf"RX enqueue type=80 payload=34 .*"
            rf"data=50[0-9a-f]{{10}}{candidate}"
            r"000049061b000200f11000024000000000000003ff", text)
    else:
        # BSIC instability, stale measurements and an unsupported neighbour
        # band are evidenced by organic candidate inspection plus the absence
        # of a serving commit. Their detailed radio evidence is checked by
        # profile-specific targets.
        si = True
    if not si:
        raise ValueError(
            f"missing standards-shaped cell-B SI3 for {profile} profile")

    selected = (
        f"serving cell selected old_arfcn={serving_arfcn} "
        f"new_arfcn={neighbour_arfcn}") in text
    if selected and profile not in ("access-class", "rxlev"):
        raise ValueError("firmware falsely selected an unsuitable neighbour")
    if profile == "rxlev":
        if not selected:
            raise ValueError(
                "firmware did not reach the RXLEV candidate boundary")
        # A receiver/channel commit is needed to obtain the candidate's BCCH
        # and is not itself stable camp.  The decisive negative invariant is
        # that the handset never resumes PCH on that receiver after decoding
        # the unattainable RXLEV_ACCESS_MIN.
        selection = text.index(
            f"serving cell selected old_arfcn={serving_arfcn} "
            f"new_arfcn={neighbour_arfcn}")
        if "PCH no-identity fill" in text[selection:]:
            raise ValueError(
                "firmware falsely reached stable PCH on RXLEV neighbour")
    if profile == "access-class" and not selected:
        raise ValueError(
            "firmware did not reach the barred-access cell boundary")
    if len(LOCATION_UPDATE.findall(text)) != 1:
        raise ValueError(
            "unsuitable neighbour initiated a spurious Location Updating")
    if len(LOCI_WRITE.findall(text)) != 2:
        raise ValueError("unsuitable neighbour mutated EF_LOCI")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("trace", type=pathlib.Path)
    parser.add_argument(
        "--profile",
        choices=(
            "barred", "rxlev", "malformed", "bsic", "forbidden",
            "access-class", "stale", "unsupported-band"),
        required=True)
    parser.add_argument("--serving-arfcn", type=int, default=1)
    parser.add_argument("--neighbour-arfcn", type=int, default=2)
    args = parser.parse_args()
    try:
        verify(
            args.trace.read_text(errors="replace"), args.profile,
            args.serving_arfcn, args.neighbour_arfcn)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    outcome = (
        "selected the cell but blocked barred-class MM access"
        if args.profile == "access-class" else
        "inspected RXLEV candidate but did not establish stable camp"
        if args.profile == "rxlev" else
        "excluded the unsupported-band neighbour"
        if args.profile == "unsupported-band" else
        f"rejected {args.profile} neighbour")
    print(f"OK - firmware {outcome} without subscriber-state mutation")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
