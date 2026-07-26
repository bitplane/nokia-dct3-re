#!/usr/bin/env python3
"""Validate a provenance-bearing real-hardware NSE-3 bootstrap capture."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


ROM3_V548_SHA1 = "5768841c9eb39c744f4fa04f0485e4f9ad4553b3"
ROM4_V548_SHA1 = "3bcc5c93ec247c63490e134196aab98a4e60c184"

FIRMWARE_VARIANTS = {
    "v5.48-rom3-ppmb": {
        "sha1": ROM3_V548_SHA1,
        "identified_pair": 3,
    },
    "v5.48-rom4-ppmb": {
        "sha1": ROM4_V548_SHA1,
        "identified_pair": None,
    },
}


def verify(document: dict) -> dict:
    if document.get("schema_version") != 1:
        raise ValueError("expected NSE-3 bootstrap capture schema version 1")
    if document.get("product") != "Nokia 6110 NSE-3":
        raise ValueError("capture is not identified as Nokia 6110 NSE-3")

    firmware = document.get("firmware", {})
    variant = firmware.get("variant")
    if variant not in FIRMWARE_VARIANTS:
        raise ValueError("capture firmware variant is not a pinned NSE-3 v5.48 image")
    variant_profile = FIRMWARE_VARIANTS[variant]
    if firmware.get("sha1") != variant_profile["sha1"]:
        raise ValueError("capture does not use the pinned NSE-3 firmware image")

    source = document.get("source", {})
    if source.get("kind") != "real_handset":
        raise ValueError("bootstrap evidence must come from a real handset")
    if source.get("dsp_backend") != "physical":
        raise ValueError("bootstrap evidence must use the physical handset DSP")
    if source.get("hle_completion") is not False:
        raise ValueError("capture must explicitly exclude HLE bootstrap completion")
    if source.get("firmware_or_ram_patching") is not False:
        raise ValueError("capture must explicitly exclude firmware or RAM patching")
    capture_sha256 = source.get("capture_sha256", "")
    if (
        len(capture_sha256) != 64
        or any(character not in "0123456789abcdef" for character in capture_sha256)
    ):
        raise ValueError("capture requires a lowercase SHA-256 provenance digest")

    events = document.get("events")
    if not isinstance(events, list):
        raise ValueError("capture events must be an ordered list")

    expected_prefix = [
        {"owner": "mcu", "action": "write", "offset": 0x002, "value": 0xFFFF},
        {"owner": "mcu", "action": "write", "offset": 0x004, "value": 0xFFFF},
        {"owner": "mcu", "action": "write", "offset": 0x006, "value": 0xFFFF},
    ]
    normalized = [
        {key: event.get(key) for key in ("owner", "action", "offset", "value")}
        for event in events
    ]
    if normalized[: len(expected_prefix)] != expected_prefix:
        raise ValueError("capture does not prove the ordered pre-upload sentinels")
    pair_events = normalized[len(expected_prefix):len(expected_prefix) + 2]
    if (
        len(pair_events) != 2
        or pair_events[0].get("owner") != "dsp"
        or pair_events[0].get("action") != "write"
        or pair_events[0].get("offset") != 0x004
        or pair_events[1].get("owner") != "dsp"
        or pair_events[1].get("action") != "write"
        or pair_events[1].get("offset") != 0x006
    ):
        raise ValueError("capture does not prove the ordered DSP pre-upload pair")
    preupload_pair = [event.get("value") for event in pair_events]
    if (
        not all(isinstance(value, int) and 0 <= value < 10 for value in preupload_pair)
        or preupload_pair[0] != preupload_pair[1]
    ):
        raise ValueError("DSP pre-upload pair must be equal single-digit values")
    identified_pair = variant_profile["identified_pair"]
    if identified_pair is not None and preupload_pair != [identified_pair] * 2:
        raise ValueError(
            f"{variant} requires the independently identified "
            f"{identified_pair}/{identified_pair} pair"
        )

    acknowledgements = [
        event for event in events
        if event.get("owner") == "dsp"
        and event.get("action") == "exchange_ack"
    ]
    if [event.get("index") for event in acknowledgements] != list(range(1, 65)):
        raise ValueError("capture must contain exactly 64 ordered DSP exchange acknowledgements")

    final_publications = [
        event for event in events
        if event.get("owner") == "dsp"
        and event.get("action") == "write"
        and event.get("offset") in (0x000, 0x002)
    ]
    if len(final_publications) != 2:
        raise ValueError("capture must contain exactly two final DSP publications")
    if final_publications[0].get("offset") != 0x000:
        raise ValueError("first final DSP publication must target shared offset 0x000")
    if final_publications[0].get("value") != 0x0B06:
        raise ValueError("first final DSP publication is not the firmware-required 0x0b06")
    if final_publications[1].get("offset") != 0x002:
        raise ValueError("second final DSP publication must target shared offset 0x002")
    verdict = final_publications[1].get("value")
    if not isinstance(verdict, int) or not 0 <= verdict <= 0xFFFF:
        raise ValueError("DSP verification verdict is not a halfword")
    if verdict == 0xFFFF:
        raise ValueError("DSP verification verdict remains the MCU sentinel")

    first_final_index = events.index(final_publications[0])
    last_ack_index = events.index(acknowledgements[-1])
    if first_final_index <= last_ack_index:
        raise ValueError("final DSP publications precede completion of the 64 exchanges")

    return {
        "firmware_variant": variant,
        "firmware_sha1": variant_profile["sha1"],
        "preupload_pair": preupload_pair,
        "exchange_count": 64,
        "first_result": 0x0B06,
        "verification_verdict": verdict,
        "capture_sha256": capture_sha256,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("--raw-trace", type=Path)
    args = parser.parse_args()
    document = json.loads(args.capture.read_text())
    if args.raw_trace is not None:
        digest = hashlib.sha256(args.raw_trace.read_bytes()).hexdigest()
        expected = document.get("source", {}).get("capture_sha256")
        if digest != expected:
            raise SystemExit(
                f"raw trace SHA-256 mismatch: expected {expected}, got {digest}"
            )
    try:
        result = verify(document)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        "NSE-3 hardware bootstrap: "
        f"variant={result['firmware_variant']} "
        f"pair={result['preupload_pair'][0]}/{result['preupload_pair'][1]} "
        f"first={result['first_result']:04x} "
        f"verdict={result['verification_verdict']:04x}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
