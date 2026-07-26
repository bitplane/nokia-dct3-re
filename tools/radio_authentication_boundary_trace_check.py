#!/usr/bin/env python3
"""Verify the organic handset-to-SIM authentication frontier."""

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

    authentication_responses = [
        line for line in text.splitlines()
        if "GSM service uplink sapi=0 pd=05 message=14 length=6" in line
    ]
    return {
        "authentication_requests": 1,
        "run_gsm_algorithm_commands": 1,
        "result_bytes_fetched": 12,
        "mm_authentication_responses": len(authentication_responses),
        "registration_promotion": False,
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
        f"MM-responses={result['mm_authentication_responses']} "
        "promotion=no"
    )


if __name__ == "__main__":
    main()
