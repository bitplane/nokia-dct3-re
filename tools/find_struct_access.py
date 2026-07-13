#!/usr/bin/env python3
"""Find Thumb-1 memory accesses to selected immediate structure offsets."""

import argparse
import os
import re
from pathlib import Path

import capstone


FLASH_BASE = 0x200000
DEFAULT_IMAGE = Path(__file__).resolve().parent.parent / "roms/3210f600a_swap16.bin"


def main() -> None:
	parser = argparse.ArgumentParser()
	parser.add_argument("offset", nargs="+", type=lambda value: int(value, 0))
	parser.add_argument("--context", type=int, default=8, help="instructions on each side")
	parser.add_argument("--clusters", type=lambda value: int(value, 0),
			help="print only windows containing every requested offset")
	parser.add_argument("--end", type=lambda value: int(value, 0), default=0x2d0000)
	args = parser.parse_args()

	image = Path(os.environ.get("NOKI_BIN", DEFAULT_IMAGE)).read_bytes()
	decoder = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
	instructions = []
	address = FLASH_BASE
	while address < min(args.end, FLASH_BASE + len(image)):
		offset = address - FLASH_BASE
		decoded = list(decoder.disasm(image[offset:offset + 4], address, count=1))
		if decoded and not (decoded[0].size == 4 and decoded[0].mnemonic not in ("bl", "blx")):
			instruction = decoded[0]
		else:
			decoded = list(decoder.disasm(image[offset:offset + 2], address, count=1))
			instruction = decoded[0] if decoded and decoded[0].size == 2 else None
		instructions.append(instruction)
		address += instruction.size if instruction else 2

	patterns = [re.compile(rf"\[[^\]]+, #0x{offset:x}\]") for offset in args.offset]
	if args.clusters:
		hits = []
		for instruction in instructions:
			if not instruction or not instruction.mnemonic.startswith(("ldr", "str")):
				continue
			for offset, pattern in zip(args.offset, patterns):
				if pattern.search(instruction.op_str):
					hits.append((instruction.address, offset, instruction))
		for start, _, _ in hits:
			window = [hit for hit in hits if start <= hit[0] < start + args.clusters]
			if set(hit[1] for hit in window) == set(args.offset):
				print(f"\n==== cluster {start:08x}..{start + args.clusters:08x} ====")
				for address, offset, instruction in window:
					print(f"  {address:08x} +{offset:02x} {instruction.mnemonic:7} {instruction.op_str}")
			hits = [hit for hit in hits if hit[0] >= start + args.clusters]
		exit()
	for index, instruction in enumerate(instructions):
		if not instruction or not instruction.mnemonic.startswith(("ldr", "str")):
			continue
		if not any(pattern.search(instruction.op_str) for pattern in patterns):
			continue
		print(f"\n==== hit {instruction.address:08x}: {instruction.mnemonic} {instruction.op_str} ====")
		for nearby in instructions[max(0, index - args.context):index + args.context + 1]:
			if nearby:
				mark = ">" if nearby.address == instruction.address else " "
				print(f"{mark} {nearby.address:08x} {nearby.mnemonic:7} {nearby.op_str}")


if __name__ == "__main__":
	main()
