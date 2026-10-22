#!/usr/bin/env python3
"""Verify an organic call-independent USSD request and network outcome."""

import argparse
import hashlib
import pathlib
import re


REQUEST = re.compile(
    r"GSM service uplink .*pd=0b message=3b length=27 "
    r"data=1b7b1c14a11202010102013b300a04010f0405aa986c36027f0100 ")
DECODED = re.compile(
    r"gsm_ss: request=ussd transaction=1b invoke=1 dcs=0f packed_length=5 "
    r"outcome=(?P<outcome>[0-4]) ")
RESPONSE = re.compile(
    r"GSM service downlink kind=27 sapi=0 pd=0b message=2a length=(?P<length>\d+) ")
RETURN_ERROR = re.compile(
    r"RX enqueue type=80 .*data="
    r"80120000[0-9a-f]{4}000100000342319b2a1c08a306020101020122")
REJECT = re.compile(
    r"RX enqueue type=80 .*data="
    r"80120000[0-9a-f]{4}000100000342319b2a1c08a406020101800100")
# Bytes 4/5 carry the changing report frame number, not an SS component field.
RR_RELEASE = re.compile(r"radio_phase=release_channel_change")
CONTINUED_FACILITY = re.compile(
    r"GSM service downlink kind=\d+ sapi=0 pd=0b message=3a length=28 ")
CONTINUED_CANCEL = re.compile(
    r"gsm_ss: continued handset_response message=2a length=2 data=1b2a ")
CONTINUED_REPLY = re.compile(
    r"gsm_ss: continued handset_response message=3a length=21 "
    r"data=1b3a12a210020102300b02013c300604010f040132 ")
RESULT_FRAME = "cf2e4a3461da27d5c49c2077810f57cc2caf6e295b089021c25157000b6324b7"


def verify(text: str, frame_dir: pathlib.Path, outcome: str = "success",
           require_frame: bool = True, require_state_roundtrip: bool = False,
           result_frame: str = RESULT_FRAME) -> dict:
    request = REQUEST.search(text)
    if not request:
        raise ValueError("missing exact processUnstructuredSS-Request for *123#")
    decoded = DECODED.search(text, request.end())
    if not decoded:
        raise ValueError("missing correlated USSD DCS/payload decode")
    expected_outcome = {
        "success": "0", "error": "1", "reject": "2", "silence": "3",
        "continued-cancelled": "4", "continued-reply": "4"
    }[outcome]
    if decoded.group("outcome") != expected_outcome:
        raise ValueError(f"wrong configured USSD outcome for {outcome}")

    if outcome == "silence":
        if RESPONSE.search(text, decoded.end()):
            raise ValueError("silent USSD outcome unexpectedly sent a response")
        if RR_RELEASE.search(text, decoded.end()):
            raise ValueError("silent USSD dialogue unexpectedly released RR")
        if require_state_roundtrip:
            summary = frame_dir / "boot_summary.txt"
            if not summary.is_file() or "state_roundtrip=pass" not in summary.read_text():
                raise ValueError("missing active USSD state save/load round trip")
        return {"frames": len(list(frame_dir.glob("*.pgm")))}

    if outcome in ("continued-cancelled", "continued-reply"):
        facility = CONTINUED_FACILITY.search(text, decoded.end())
        if not facility:
            raise ValueError("missing network continued-USSD FACILITY")
        handset = (CONTINUED_CANCEL if outcome == "continued-cancelled" else CONTINUED_REPLY).search(text, facility.end())
        if not handset:
            raise ValueError("missing exact handset continued-USSD cancellation/reply")
        end=handset.end()
        if outcome == "continued-reply":
            result=re.search(r'gsm_ss: continued_result transaction=1b invoke=2 dcs=0f packed_length=1 empty=0 ',text[end:])
            if not result:raise ValueError('continued reply was not parsed and correlated')
            end+=result.end()
            response=RESPONSE.search(text,end)
            if not response or response['length']!='37':raise ValueError('continued reply has no final result')
            end=response.end()
        if not RR_RELEASE.search(text, end):
            raise ValueError("missing RR release after continued-USSD cancellation/reply")
        return {"frames": len(list(frame_dir.glob("*.pgm")))}

    response = RESPONSE.search(text, decoded.end())
    if not response:
        raise ValueError(f"missing USSD {outcome} RELEASE COMPLETE")
    expected_length = "37" if outcome == "success" else "12"
    if response.group("length") != expected_length:
        raise ValueError(f"wrong USSD {outcome} response length")
    if outcome == "error" and not RETURN_ERROR.search(text, response.end()):
        raise ValueError("missing exact USSD ReturnError component")
    if outcome == "reject" and not REJECT.search(text, response.end()):
        raise ValueError("missing exact USSD Reject component")
    if not RR_RELEASE.search(text, response.end()):
        raise ValueError(f"missing RR release after USSD {outcome}")

    hashes = {
        hashlib.sha256(path.read_bytes()).hexdigest()
        for path in frame_dir.glob("*.pgm")
    }
    if outcome == "success" and require_frame and result_frame not in hashes:
        raise ValueError("missing exact firmware-rendered Nokia test network frame")
    return {"frames": len(hashes)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("frame_dir", type=pathlib.Path)
    parser.add_argument("--outcome", choices=(
        "success", "error", "reject", "silence", "continued-cancelled", "continued-reply"),
                        default="success")
    parser.add_argument("--require-state-roundtrip", action="store_true")
    parser.add_argument("--result-frame", default=RESULT_FRAME)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"), args.frame_dir,
               args.outcome, require_state_roundtrip=args.require_state_roundtrip,
               result_frame=args.result_frame)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(f"OK - organic *123# completed the {args.outcome} USSD contract")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
