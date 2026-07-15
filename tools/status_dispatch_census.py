#!/usr/bin/env python3
"""Enumerate 16-bit inputs that make a Thumb status dispatcher reach a target."""

import argparse
from pathlib import Path

from unicorn import Uc, UcError, UC_ARCH_ARM, UC_HOOK_CODE, UC_MODE_BIG_ENDIAN, UC_MODE_THUMB
from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_SP


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_ROM = ROOT / "roms/3210f600a.fls"


def number(value):
	return int(value, 0)


class StatusDispatcher:
	def __init__(self, rom, entry, lower, upper, target):
		self.entry = entry
		self.lower = lower
		self.upper = upper
		self.target = target
		self.hit = False
		self.uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_BIG_ENDIAN)
		self.uc.mem_map(0x00200000, 0x00100000)
		self.uc.mem_write(0x00200000, rom[:0x00100000])
		self.uc.mem_map(0x00100000, 0x00040000)
		self.uc.hook_add(UC_HOOK_CODE, self._hook)

	def _hook(self, uc, address, _size, _user_data):
		pc = address & ~1
		if pc == self.target:
			self.hit = True
			uc.emu_stop()
		elif not self.lower <= pc < self.upper:
			uc.emu_stop()

	def reaches_target(self, status):
		self.hit = False
		self.uc.reg_write(UC_ARM_REG_SP, 0x0013f000)
		self.uc.reg_write(UC_ARM_REG_R0, status)
		self.uc.reg_write(UC_ARM_REG_R1, 0)
		self.uc.reg_write(UC_ARM_REG_LR, 0)
		try:
			self.uc.emu_start(self.entry | 1, 0, count=4000)
		except UcError:
			# Non-matching leaves may require live firmware objects. They are outside
			# the bounded dispatcher question once the requested target was not hit.
			pass
		return self.hit


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--rom", type=Path, default=DEFAULT_ROM)
	parser.add_argument("--entry", type=number, required=True)
	parser.add_argument("--target", type=number, required=True)
	parser.add_argument("--lower", type=number)
	parser.add_argument("--upper", type=number)
	args = parser.parse_args()
	lower = args.lower if args.lower is not None else args.entry
	upper = args.upper if args.upper is not None else (args.entry & ~0xffff) + 0x10000
	dispatcher = StatusDispatcher(args.rom.read_bytes(), args.entry, lower, upper, args.target)
	hits = [status for status in range(0x10000) if dispatcher.reaches_target(status)]
	for status in hits:
		print(f"status=0x{status:04x} target=0x{args.target:08x}")
	print(f"coverage=65536 hits={len(hits)}")


if __name__ == "__main__":
	raise SystemExit(main())
