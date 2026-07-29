#!/usr/bin/env python3
"""Check the observable, non-secret A5/1 activation and burst boundary."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


def check(text: str) -> list[str]:
    errors: list[str] = []
    required = {
        "A5/1 Cipher Mode Command": r"data=8012[0-9a-f]*063501",
        "redacted DSP key publication":
            r"TX packet type=14 payload=(?:10|12) .*data=<redacted>",
        "organic Cipher Mode Complete":
            r"GSM service uplink sapi=0 pd=06 message=32 .*data=0632",
        "session activation":
            r"gsm_cipher: event=activated algorithm=1",
        "uplink burst ciphering":
            r"radio_l1: kind=cipher direction=uplink algorithm=1 fn=\d+ count=\d+",
        "downlink burst ciphering":
            r"radio_l1: kind=cipher direction=downlink algorithm=1 fn=\d+ count=\d+",
        "uplink SDCCH xCCH ciphering":
            r"radio_l1: kind=xcch direction=uplink algorithm=1 "
            r"first_fn=\d+ last_fn=\d+",
        "downlink SDCCH xCCH ciphering":
            r"radio_l1: kind=xcch direction=downlink algorithm=1 "
            r"first_fn=\d+ last_fn=\d+",
    }
    for label, pattern in required.items():
        if not re.search(pattern, text):
            errors.append(f"missing {label}")

    if re.search(r"TX (?:packet|consume) type=14 .*data=02[0-9a-f]{18,}", text):
        errors.append("cipher-control trace exposes Kc")

    command = text.find("063501")
    complete = text.find("message=32")
    activation = text.find("gsm_cipher: event=activated")
    first_burst = text.find("radio_l1: kind=cipher")
    if min(command, complete, activation, first_burst) >= 0 and not (
            command < complete <= activation < first_burst):
        errors.append("cipher activation ordering is not command -> complete -> bursts")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    errors = check(args.log.read_text(errors="replace"))
    if errors:
        for error in errors:
            print(f"ERROR - {error}")
        return 1
    print("OK - organic A5/1 command/completion and directional burst ciphering")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
