#!/usr/bin/env python3
"""Exhaustively classify 3210 controller-dispatch inputs with concrete execution."""

import argparse
import json
import sys
from pathlib import Path

from unicorn import Uc, UC_ARCH_ARM, UC_HOOK_CODE, UC_MODE_BIG_ENDIAN, UC_MODE_THUMB
from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_SP


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_PROFILE = ROOT / "tools/profiles/noki3210_v600.json"
DEFAULT_ROM = ROOT / "roms/3210f600a.fls"


def number(value):
	return int(value, 0) if isinstance(value, str) else value


class Dispatcher:
	def __init__(self, rom, contract):
		self.entry = number(contract["entry"])
		self.end = number(contract["code_end"])
		self.mode_store = number(contract["mode_store"])
		self.result = None
		self.uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_BIG_ENDIAN)
		self.uc.mem_map(0x00200000, 0x00100000)
		self.uc.mem_write(0x00200000, rom[:0x00100000])
		self.uc.mem_map(0x00100000, 0x00040000)
		self.uc.hook_add(UC_HOOK_CODE, self._hook)

	def _hook(self, uc, address, _size, _user_data):
		pc = address & ~1
		if pc == self.mode_store:
			self.result = {
				"mode": uc.reg_read(UC_ARM_REG_R0) & 0xff,
				"object": uc.reg_read(UC_ARM_REG_R1),
			}
			uc.emu_stop()
		elif not self.entry <= pc < self.end:
			uc.emu_stop()

	def classify(self, status):
		self.result = None
		self.uc.reg_write(UC_ARM_REG_SP, 0x0013f000)
		self.uc.reg_write(UC_ARM_REG_R0, status)
		self.uc.reg_write(UC_ARM_REG_R1, 0)
		self.uc.reg_write(UC_ARM_REG_LR, 0)
		self.uc.emu_start(self.entry | 1, self.end | 1, count=4000)
		return self.result


def exhaustive_map(rom, contract):
	dispatcher = Dispatcher(rom, contract)
	result = {}
	for status in range(0x10000):
		classification = dispatcher.classify(status)
		if classification is not None:
			result[status] = classification["mode"]
	return result


def expected_map(contract):
	return {number(item["status"]): number(item["mode"]) for item in contract["expected"]}


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--profile", type=Path, default=DEFAULT_PROFILE)
	parser.add_argument("--rom", type=Path, default=DEFAULT_ROM)
	parser.add_argument("--check", action="store_true")
	args = parser.parse_args()

	profile = json.loads(args.profile.read_text())
	contract = profile["controller_dispatch_contract"]
	actual = exhaustive_map(args.rom.read_bytes(), contract)
	for status, mode in sorted(actual.items()):
		print(f"status=0x{status:04x} mode=0x{mode:02x}")

	if args.check and actual != expected_map(contract):
		print("controller dispatch map differs from reviewed profile", file=sys.stderr)
		print(f"expected={expected_map(contract)!r}", file=sys.stderr)
		print(f"actual={actual!r}", file=sys.stderr)
		return 1
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
