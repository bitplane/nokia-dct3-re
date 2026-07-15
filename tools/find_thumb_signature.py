#!/usr/bin/env python3
"""Find a relocated Thumb-1 function in another DCT3 image.

Direct BL/BLX encodings depend on the link address, so those four-byte pairs
are ignored. All remaining bytes, including local branches and literals, must
match exactly. This intentionally favors quantified absence over fuzzy hits.
"""

import argparse
from pathlib import Path


FLASH_BASE = 0x200000


def parse_range(value: str) -> tuple[int, int]:
	address, length = value.split(":", 1)
	return int(address, 0), int(length, 0)


def relocation_mask(data: bytes) -> bytearray:
	mask = bytearray([0xff]) * len(data)
	for offset in range(0, len(data) - 3, 2):
		first = int.from_bytes(data[offset:offset + 2], "little")
		second = int.from_bytes(data[offset + 2:offset + 4], "little")
		if first & 0xf800 == 0xf000 and second & 0xe800 == 0xe800:
			mask[offset:offset + 4] = bytes(4)
	return mask


def main() -> None:
	parser = argparse.ArgumentParser()
	parser.add_argument("reference", type=Path)
	parser.add_argument("target", type=Path)
	parser.add_argument("region", type=parse_range, help="reference address:length")
	args = parser.parse_args()

	reference = args.reference.read_bytes()
	target = args.target.read_bytes()
	address, length = args.region
	offset = address - FLASH_BASE
	if offset < 0 or offset + length > len(reference):
		raise SystemExit("reference region is outside the image")

	signature = reference[offset:offset + length]
	mask = relocation_mask(signature)
	compared = sum(byte != 0 for byte in mask)
	matches = []
	for candidate in range(0, len(target) - length + 1, 2):
		if all(not mask[index] or target[candidate + index] == signature[index]
				for index in range(length)):
			matches.append(FLASH_BASE + candidate)

	print(f"reference=0x{address:08x} length={length} compared={compared}/{length}")
	print(f"matches={len(matches)}")
	for match in matches:
		print(f"0x{match:08x}")


if __name__ == "__main__":
	main()
