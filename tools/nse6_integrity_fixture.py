"""Create an erased NSE-6 integrity diagnostic, not valid handset provisioning."""

import argparse
from pathlib import Path

from tools.nse6_v602_static_check import integrity_arithmetic


def fixture(config_integrity=False, initialized_retry=False):
    image = bytearray(b"\xff" * 0x8000)
    # The excluded word is also the firmware-owned verifier-result record.
    checksum = integrity_arithmetic(image[0x40:0x11E],
                                    int.from_bytes(image[0x74:0x76], "big"))
    image[0x11E:0x120] = checksum.to_bytes(2, "big")
    if config_integrity:
        image[0x3C:0x40] = sum(image[:0x3C]).to_bytes(4, "big")
    if initialized_retry:
        # Own-ROM record 0x0701 loads eight bytes at 0x17f4 into 0x1215b0.
        # Its +4 retry count describes a fresh synthetic phone state,
        # not erased 0xff. The code and identity remain firmware-owned/erased.
        image[0x17F8] = 0
    return bytes(image)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--config-integrity", action="store_true",
                        help="also correct the separately decoded config-block sum")
    parser.add_argument("--initialized-retry", action="store_true",
                        help="initialize the synthetic phone-code retry count to zero")
    args = parser.parse_args()
    if args.output.exists():
        parser.error("refusing to overwrite existing storage")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(fixture(args.config_integrity, args.initialized_retry))
    print("created synthetic diagnostic; identity remains erased; retry initialized="
          + str(args.initialized_retry))


if __name__ == "__main__":
    main()
