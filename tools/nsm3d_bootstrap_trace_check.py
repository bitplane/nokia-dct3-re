"""Check observed 8250 upload bytes and fail-closed boundary, not phone boot."""

import argparse
from pathlib import Path
import re

try:
    from tools.extract_nsm3_verifier import extract
except ModuleNotFoundError:
    from extract_nsm3_verifier import extract


def check(text, program):
    captures = re.findall(r"nsm3d_verifier_program: words=([0-9a-f]+)", text)
    if len(captures) != 1 or bytes.fromhex(captures[0]) != program:
        raise ValueError("runtime staged program differs from pinned 8250 flash")
    inputs = dict((int(a, 16), int(v, 16)) for a, v in re.findall(
        r"nsm3d_verifier_input: address=([0-9a-f]+) value=([0-9a-f]+)", text))
    expected = dict(zip(range(0x110f6, 0x11104, 2),
                        (0x100, 0x300, 0, 0xe800, 1, 1, 0x200)))
    if inputs != expected:
        raise ValueError("unexpected MCU-supplied verifier geometry")
    boundary = re.findall(
        r"nsm3d_verifier_boundary: pc=([0-9a-f]+) result0=([0-9a-f]+) "
        r"result1=([0-9a-f]+) pairs0=(\d+) pairs1=(\d+) order_errors=(\d+)", text)
    if len(boundary) != 1:
        raise ValueError("missing unique final boundary")
    pc, result0, result1, first, second, errors = boundary[0]
    if int(pc, 16) not in (0x2cb30e, 0x2cb310, 0x2cb312, 0x2cb314):
        raise ValueError("not at final verification wait")
    if (int(result0, 16), int(result1, 16), int(first), int(second), int(errors)) != (0, 0xffff, 58, 58, 0):
        raise ValueError("unexpected publication or ownership sequence")
    if "[LUA ERROR]" in text:
        raise ValueError("observer error")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("flash", type=Path)
    args = parser.parse_args()
    try:
        check(args.log.read_text(), extract(args.flash.read_bytes(), "8250"))
    except (OSError, ValueError) as error:
        parser.exit(1, f"8250 bootstrap observation failed: {error}\n")
    print("8250 staged program, supplied geometry and 58 ordered pairs verified; final result remains unpublished")


if __name__ == "__main__":
    main()
