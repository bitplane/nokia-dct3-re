#!/usr/bin/env python3
"""Enumerate exact Thumb-1 immediate and literal uses of scalar values."""

import argparse
import os
import re
from pathlib import Path

import capstone


FLASH_BASE = 0x200000
DEFAULT_IMAGE = Path(__file__).resolve().parent.parent / "roms/3210f600a_swap16.bin"


def decode_thumb1(image: bytes, start: int, end: int):
	decoder = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
	decoder.detail = True
	address = start
	decoded_count = 0
	undecoded_count = 0
	while address < min(end, FLASH_BASE + len(image) - 1):
		offset = address - FLASH_BASE
		decoded = list(decoder.disasm(image[offset:offset + 4], address, count=1))
		instruction = decoded[0] if decoded and not (
			decoded[0].size == 4 and decoded[0].mnemonic not in ("bl", "blx")) else None
		if instruction is None:
			decoded = list(decoder.disasm(image[offset:offset + 2], address, count=1))
			instruction = decoded[0] if decoded and decoded[0].size == 2 else None
		if instruction:
			decoded_count += 1
			yield instruction
			address += instruction.size
		else:
			undecoded_count += 1
			address += 2
	return decoded_count, undecoded_count


def literal_value(image: bytes, instruction):
	if instruction.mnemonic != "ldr" or "[pc" not in instruction.op_str:
		return None
	match = re.search(r"#(0x[0-9a-f]+)", instruction.op_str)
	if not match:
		return None
	address = ((instruction.address + 4) & ~3) + int(match.group(1), 16)
	offset = address - FLASH_BASE
	if not 0 <= offset <= len(image) - 4:
		return None
	return address, int.from_bytes(image[offset:offset + 4], "little")


def main() -> None:
	parser = argparse.ArgumentParser()
	parser.add_argument("value", nargs="+", type=lambda value: int(value, 0))
	parser.add_argument("--start", type=lambda value: int(value, 0), default=FLASH_BASE)
	parser.add_argument("--end", type=lambda value: int(value, 0), default=0x2d0000)
	parser.add_argument("--mnemonic", action="append", help="limit immediate hits to a mnemonic")
	args = parser.parse_args()
	values = set(args.value)
	image = Path(os.environ.get("NOKI_BIN", DEFAULT_IMAGE)).read_bytes()
	hits = {value: [] for value in values}
	decoded = 0
	undecoded = 0
	previous = None
	iterator = decode_thumb1(image, args.start, args.end)
	while True:
		try:
			instruction = next(iterator)
		except StopIteration as result:
			if result.value:
				decoded, undecoded = result.value
			break
		for operand in instruction.operands:
			if operand.type == capstone.arm.ARM_OP_IMM and operand.imm in values:
				if not args.mnemonic or instruction.mnemonic in args.mnemonic:
					hits[operand.imm].append((instruction.address, "immediate", instruction))
		literal = literal_value(image, instruction)
		if literal and literal[1] in values:
			hits[literal[1]].append((instruction.address, f"literal@{literal[0]:08x}", instruction))
		if previous and previous.mnemonic == "movs" and instruction.mnemonic in ("adds", "subs"):
			if len(previous.operands) == 2 and len(instruction.operands) == 2:
				previous_dst, previous_value = previous.operands
				current_dst, current_value = instruction.operands
				if (previous_dst.type == capstone.arm.ARM_OP_REG and
						previous_value.type == capstone.arm.ARM_OP_IMM and
						current_dst.type == capstone.arm.ARM_OP_REG and
						current_value.type == capstone.arm.ARM_OP_IMM and
						previous_dst.reg == current_dst.reg):
					value = previous_value.imm + current_value.imm * (1 if instruction.mnemonic == "adds" else -1)
					if value in values:
						hits[value].append((previous.address, "constructed/2-insn", previous))
		previous = instruction
	for value in sorted(values):
		print(f"\n=== scalar {value:#x} ({len(hits[value])} uses) ===")
		for address, kind, instruction in hits[value]:
			print(f"  {address:08x} {kind:18} {instruction.mnemonic:7} {instruction.op_str}")
	print(f"\ncoverage: range={args.start:#x}..{args.end:#x} decoded={decoded} "
			f"undecoded_halfwords={undecoded} image_bytes={len(image)}")


if __name__ == "__main__":
	main()
