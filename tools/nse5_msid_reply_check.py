#!/usr/bin/env python3
"""Read-only MSID reply/encoder agreement; never derives provisioning inputs."""

import argparse
from pathlib import Path
import re
import sys

try:
    from .make_5110_eeprom_profile import decode_msid
    from .nse5_transform_trace_check import CODEC
except ImportError:
    from make_5110_eeprom_profile import decode_msid
    from nse5_transform_trace_check import CODEC


REPLY = re.compile(r"nse5_compat_dsp_control: name=task2_message_received "
                   r"message=[0-9a-f]+ bytes=([0-9a-f]+) flags=[0-9a-f]+ t=([0-9.]+)")


def check(text):
    encoders = []
    for match in CODEC.finditer(text):
        source = bytes.fromhex(match[1].replace(":", ""))
        output = bytes.fromhex(match[4].replace(":", ""))
        if len(source) != 12 or len(output) != 12:
            raise ValueError("malformed codec words")
        encoders.append((source, output, float(match[5])))
    results = []
    for match in REPLY.finditer(text):
        packet = bytes.fromhex(match[1])
        if len(packet) < 9 or packet[8] != 0x34:
            continue
        if len(packet) != 24 or packet[9:12] != bytes.fromhex("0e0082"):
            raise ValueError("MSID reply length/header changed")
        msid = packet[11:24]
        groups = decode_msid(msid)
        plain = b"".join(groups)
        candidates = [entry for entry in encoders
                      if entry[1] == msid[1:] and entry[2] < float(match[2])]
        if not candidates:
            raise ValueError("MSID reply has no preceding matching encoder observation")
        if candidates[-1][0] != plain:
            raise ValueError("MSID decoder disagrees with observed encoder input")
        results.append({"msid": msid.hex(), "decoded_groups": [group.hex() for group in groups]})
    if not results:
        raise ValueError("no complete primitive-34 MSID replies")
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        results = check(args.log.read_text(errors="replace"))
    except (OSError, ValueError) as error:
        print(f"NSE-5 MSID observation: FAIL {error}", file=sys.stderr)
        return 1
    print(f"NSE-5 MSID observation: PASS replies={len(results)} groups={'/'.join(results[-1]['decoded_groups'])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
