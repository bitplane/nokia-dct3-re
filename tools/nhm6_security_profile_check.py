#!/usr/bin/env python3
"""Check the NHM-6 first-boot and Settings phone-code comparisons."""

import argparse
import re
from pathlib import Path


COMPARE_RE = re.compile(
    r"nhm6_security_verify: stage=compare .*transformed=([0-9a-f]{8}) "
    r"stored=([0-9a-f]{8})")
SETTLED_VERIFIER = "d83b30d8"


def validate(text: str) -> None:
    comparisons = COMPARE_RE.findall(text)
    if len(comparisons) != 2:
        raise ValueError(
            f"expected exactly two NHM-6 security comparisons, got {len(comparisons)}")
    for index, (transformed, stored) in enumerate(comparisons, 1):
        if transformed != SETTLED_VERIFIER or stored != SETTLED_VERIFIER:
            raise ValueError(
                f"comparison {index} did not use settled verifier {SETTLED_VERIFIER}: "
                f"transformed={transformed} stored={stored}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        validate(args.log.read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(f"NHM-6 security profile: FAIL - {error}") from error
    print("NHM-6 security profile: PASS")


if __name__ == "__main__":
    main()
