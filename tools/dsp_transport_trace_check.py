#!/usr/bin/env python3
"""Check the normalized DSPIF transport lifecycle without depending on timings."""

import argparse
import pathlib
import re
import sys


def require(text: str, pattern: str, label: str) -> int:
    count = len(re.findall(pattern, text))
    if not count:
        raise ValueError(f"missing DSP transport event: {label}")
    return count


def check(text: str, full_session: bool, conformance: bool = False) -> dict[str, int]:
    if conformance:
        require(text, r"dspif_fixture: conformance=07", "wrap/full/partial conformance")
        return {"conformance": 0x07}
    counts = {
        "bootstrap_requests": require(
            text,
            r"RAM W off=(?:0fe|100) data=0000",
            "MCU bootstrap exchange request"),
        "bootstrap_publications": require(
            text,
            r"peer RAM W off=(?:000|002|004|0e0|0fe|100) data=000[01]",
            "DSP bootstrap shared-RAM publication"),
        "doorbells": require(text, r"dspif_transport: doorbell command=0004", "command-4 doorbell"),
        "service_pending": require(text, r"RAM W off=0e4 data=000[1-9a-f]", "service pending publication"),
        "service_complete": require(text, r"IRQ4 service-complete", "service completion IRQ4"),
    }
    for offset, value in (("000", "0001"), ("002", "0001"), ("004", "0001"),
                          ("0e0", "0000"), ("0fe", "0001"), ("100", "0001")):
        require(text, rf"peer RAM W off={offset} data={value}", f"bootstrap word {offset}")
    for offset in ("0fe", "100"):
        require(text, rf"RAM W off={offset} data=0000", f"bootstrap request {offset}")
    if full_session:
        counts.update({
            "tx_packets": require(text, r"dsp_hle: TX packet type=", "complete TX packet"),
            "tx_consumed": require(text, r"dspif_transport: TX consume type=", "peer consumer advance"),
            "rx_packets": require(text, r"dspif_transport: RX enqueue type=", "RX producer advance"),
            "fiq0": require(text, r"dspif_transport: FIQ0 notify", "RX FIQ0 notification"),
            "service_control": require(text, r"RX enqueue type=74 payload=2", "type-70 service completion"),
            "external_session": require(text, r"external_service: response command=64 result=01", "external registration"),
        })
        if counts["tx_packets"] != counts["tx_consumed"]:
            raise ValueError("complete TX packet and consumer-advance counts differ")
    return counts


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--bootstrap-only", action="store_true")
    parser.add_argument("--conformance", action="store_true")
    args = parser.parse_args()
    try:
        counts = check(args.log.read_text(errors="replace"), not args.bootstrap_only, args.conformance)
    except ValueError as exc:
        print(exc, file=sys.stderr)
        return 1
    print(f"DSP transport contract: {counts}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
