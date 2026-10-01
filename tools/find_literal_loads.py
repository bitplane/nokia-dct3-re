#!/usr/bin/env python3
"""Find Thumb-1 PC-relative literal loads by loaded value or pool address."""

import argparse
from pathlib import Path

FLASH_BASE = 0x200000


def parse_int(value: str) -> int:
	return int(value, 0)


def literal_loads(data, value, start=FLASH_BASE, end=None, encoding="swap16", raw=False):
	"""Return syntactic Thumb-1 candidates, not proven executable references."""
	if encoding not in ("swap16", "big"):
		raise ValueError("unsupported image encoding")
	byteorder = "little" if encoding == "swap16" else "big"
	target = value
	if encoding == "swap16" and not raw:
		target = ((value << 16) | (value >> 16)) & 0xffffffff
	start = max(start, FLASH_BASE)
	end = min(end if end is not None else FLASH_BASE + len(data), FLASH_BASE + len(data))
	for address in range((start + 1) & ~1, end - 1, 2):
		offset = address - FLASH_BASE
		word = int.from_bytes(data[offset:offset + 2], byteorder)
		if word & 0xf800 != 0x4800:
			continue
		immediate = (word & 0xff) * 4
		pool = ((address + 4) & ~3) + immediate
		pool_offset = pool - FLASH_BASE
		if pool_offset < 0 or pool_offset + 4 > len(data):
			continue
		loaded = int.from_bytes(data[pool_offset:pool_offset + 4], byteorder)
		if loaded == target:
			yield address, (word >> 8) & 7, immediate, pool, loaded


def main() -> None:
	parser = argparse.ArgumentParser()
	parser.add_argument("value", type=parse_int, help="effective 32-bit firmware value")
	parser.add_argument("--raw", action="store_true",
		help="match the literal bytes without undoing the image's 16-bit halfword swap")
	parser.add_argument("--rom", type=Path,
		default=Path(__file__).resolve().parent.parent / "roms/3210f600a_swap16.bin")
	parser.add_argument("--start", type=parse_int, default=FLASH_BASE)
	parser.add_argument("--end", type=parse_int)
	parser.add_argument("--encoding", choices=("swap16", "big"), default="swap16",
		help="image instruction/literal encoding; acquired 7110 FLS uses big")
	args = parser.parse_args()

	data = args.rom.read_bytes()
	for address, register, immediate, pool, loaded in literal_loads(
			data, args.value, args.start, args.end, args.encoding, args.raw):
		print(f"{address:#010x}: ldr r{register}, [pc, #{immediate:#x}] ; "
			f"[{pool:#010x}] = {args.value:#010x} (raw {loaded:#010x})")


if __name__ == "__main__":
	main()
