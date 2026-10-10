#!/usr/bin/env python3
"""Check measured NSE-6 control decoding, not peripheral reset completion."""

import argparse
from pathlib import Path
import re
import sys


def check(text):
    if "unimplemented C54x opcode" in text:
        raise ValueError("unsupported native opcode")
    posts = re.findall(
        r"nse6_native_control_post: object=[0-9a-f]+ length=04 "
        r"payload=0080,0104 lr=0029ba3b t=([0-9.]+)", text)
    parameters = re.findall(
        r"nse6_native_serial_parameters: address=([0-9a-f]{4}) "
        r"value=([0-9a-f]{4}) mask=ffff pc=([0-9a-f]{4}) t=([0-9.]+)", text)
    for posted in posts:
        previous = float(posted)
        for address, value, pc in (
                ("06f4", "0880", "3bb3"),
                ("06f6", "0015", "3bbd"),
                ("06f5", "0362", "3bca")):
            times = [float(at) for addr, data, site, at in parameters
                     if (addr, data, site) == (address, value, pc)
                     and float(at) >= previous]
            if not times:
                break
            previous = min(times)
        else:
            return {"posted": float(posted), "decoded": previous}
    raise ValueError("missing ordered control packet/parameter decoding")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        result = check(args.log.read_text(errors="replace"))
    except (OSError, ValueError) as error:
        print(f"NSE-6 serial control: FAIL {error}", file=sys.stderr)
        return 1
    print(f"NSE-6 serial control: PASS decoded={result['decoded']:.9f} "
          "reset_completion_claim=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
