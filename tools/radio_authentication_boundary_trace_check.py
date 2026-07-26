#!/usr/bin/env python3
"""Verify organic GSM authentication through registration and return to camp."""

from __future__ import annotations

import argparse
from pathlib import Path


REQUEST = "05120023553cbe9637a89d218ae64dae47bf35"


def verify(text: str) -> dict:
    request_lines = [
        line for line in text.splitlines()
        if "RX enqueue type=80" in line and REQUEST in line
    ]
    run_headers = [
        line for line in text.splitlines()
        if "sim_device: header cla=a0 ins=88 p1=00 p2=00 p3=10" in line
    ]
    run_bodies = [
        line for line in text.splitlines()
        if "sim_device: body ins=88 length=16 selected=7f20" in line
    ]
    get_responses = [
        line for line in text.splitlines()
        if "sim_device: header cla=a0 ins=c0 p1=00 p2=00 p3=0c" in line
    ]
    results = [
        line for line in text.splitlines()
        if "sim_device: pending response ins=c0 length=12 status=9000" in line
    ]
    if len(request_lines) != 1:
        raise ValueError(
            f"expected one MM Authentication Request, found {len(request_lines)}"
        )
    if len(run_headers) != 1 or len(run_bodies) != 1:
        raise ValueError("expected one complete RUN GSM ALGORITHM transaction")
    if len(get_responses) != 1 or len(results) != 1:
        raise ValueError("expected one complete twelve-byte GET RESPONSE")
    consumers = [
        line for line in text.splitlines()
        if "sim_authentication_consumer:" in line
        and "get_status=0066 accepted=01" in line
    ]
    if len(consumers) != 1:
        raise ValueError(
            f"expected one firmware-accepted authentication result, found {len(consumers)}"
        )
    queued_primitives = [
        line for line in text.splitlines()
        if "radio_pending_primitive:" in line
        and "old=0000 data=1000" in line
    ]
    if len(queued_primitives) != 1:
        raise ValueError(
            f"expected one queued SRES radio primitive, found {len(queued_primitives)}"
        )

    authentication_responses = [
        line for line in text.splitlines()
        if "GSM service uplink sapi=0 pd=05 message=14 length=6" in line
    ]
    if len(authentication_responses) != 1:
        raise ValueError(
            "expected one organic MM Authentication Response, found "
            f"{len(authentication_responses)}"
        )

    lifecycle = (
        ("Location Updating Accept", "032245050200f11000011708"),
        ("Accept acknowledgement", "data=0080034101"),
        ("Channel Release acknowledgement",
         "LAPDm service Channel Release acknowledged nr=3"),
        ("Release acknowledgement", "data=0080036101"),
        ("EF_LOCI location update",
         "sim_device: update-binary fid=6f7e offset=4 length=5"),
        ("EF_LOCI status update",
         "sim_device: update-binary fid=6f7e offset=10 length=1"),
        ("channel deconfiguration", "radio_phase=release_channel_change"),
        ("release confirmation", "RX enqueue type=89 payload=8"),
        ("return to idle PCH", "1506210001f0"),
    )
    cursor = text.find(authentication_responses[0])
    for label, needle in lifecycle:
        cursor = text.find(needle, cursor)
        if cursor < 0:
            raise ValueError(f"missing or out-of-order authentication checkpoint: {label}")
        cursor += len(needle)

    return {
        "authentication_requests": 1,
        "run_gsm_algorithm_commands": 1,
        "result_bytes_fetched": 12,
        "firmware_results_accepted": 1,
        "sres_primitives_queued": 1,
        "mm_authentication_responses": len(authentication_responses),
        "registration_promotion": True,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    result = verify(args.log.read_text(errors="replace"))
    print(
        "authentication boundary: "
        f"requests={result['authentication_requests']} "
        f"SIM-runs={result['run_gsm_algorithm_commands']} "
        f"fetched={result['result_bytes_fetched']} "
        f"accepted={result['firmware_results_accepted']} "
        f"queued={result['sres_primitives_queued']} "
        f"MM-responses={result['mm_authentication_responses']} "
        "promotion=yes"
    )


if __name__ == "__main__":
    main()
