"""Create an erased NSE-6 integrity diagnostic, not valid handset provisioning."""

import argparse
from pathlib import Path

from tools.nse6_v602_static_check import integrity_arithmetic


def fixture():
    image = bytearray(b"\xff" * 0x8000)
    # The excluded word is also the firmware-owned verifier-result record.
    checksum = integrity_arithmetic(image[0x40:0x11E],
                                    int.from_bytes(image[0x74:0x76], "big"))
    image[0x11E:0x120] = checksum.to_bytes(2, "big")
    return bytes(image)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("refusing to overwrite existing storage")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(fixture())
    print("created checksum-only diagnostic; identity and lock fields remain erased")


if __name__ == "__main__":
    main()
