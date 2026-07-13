#!/usr/bin/env python3
"""Find Thumb-1 PC-relative literal loads by loaded value or pool address."""

import argparse
from pathlib import Path

import capstone


FLASH_BASE = 0x200000


def parse_int(value: str) -> int:
	return int(value, 0)


def main() -> None:
	parser = argparse.ArgumentParser()
	parser.add_argument("value", type=parse_int, help="effective 32-bit firmware value")
	parser.add_argument("--raw", action="store_true",
		help="match the literal bytes without undoing the image's 16-bit halfword swap")
	parser.add_argument("--rom", type=Path,
		default=Path(__file__).resolve().parent.parent / "roms/3210f600a_swap16.bin")
	parser.add_argument("--start", type=parse_int, default=FLASH_BASE)
	parser.add_argument("--end", type=parse_int)
	args = parser.parse_args()

	data = args.rom.read_bytes()
	end = args.end if args.end is not None else FLASH_BASE + len(data)
	start = max(args.start, FLASH_BASE)
	end = min(end, FLASH_BASE + len(data))
	md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)

	raw_target = args.value if args.raw else ((args.value << 16) | (args.value >> 16)) & 0xffffffff
	for address in range(start & ~1, end, 2):
		offset = address - FLASH_BASE
		insns = list(md.disasm(data[offset:offset + 2], address, count=1))
		if not insns:
			continue
		insn = insns[0]
		# Thumb-1 literal loads encode an unsigned word offset from Align(PC + 4, 4).
		if insn.mnemonic != "ldr" or ", [pc, #" not in insn.op_str:
			continue
		immediate = int(insn.op_str.rsplit("#", 1)[1].rstrip("]"), 0)
		pool = ((address + 4) & ~3) + immediate
		pool_offset = pool - FLASH_BASE
		if pool_offset < 0 or pool_offset + 4 > len(data):
			continue
		loaded = int.from_bytes(data[pool_offset:pool_offset + 4], "little")
		if loaded == raw_target:
			print(f"{address:#010x}: {insn.mnemonic} {insn.op_str} ; "
				f"[{pool:#010x}] = {args.value:#010x} (raw {loaded:#010x})")


if __name__ == "__main__":
	main()
