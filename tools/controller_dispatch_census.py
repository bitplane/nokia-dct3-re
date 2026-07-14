#!/usr/bin/env python3
"""Exhaustively classify 3210 controller-dispatch inputs with concrete execution."""

import argparse
import json
import sys
from pathlib import Path

from unicorn import Uc, UcError, UC_ARCH_ARM, UC_HOOK_CODE, UC_MODE_BIG_ENDIAN, UC_MODE_THUMB
from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_SP


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_PROFILE = ROOT / "tools/profiles/noki3210_v600.json"
DEFAULT_ROM = ROOT / "roms/3210f600a.fls"


def number(value):
	return int(value, 0) if isinstance(value, str) else value


class Dispatcher:
	def __init__(self, rom, contract, target=None):
		self.entry = number(contract["entry"])
		self.end = number(contract["code_end"])
		self.mode_store = number(contract["mode_store"])
		self.target = target
		self.result = None
		self.uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_BIG_ENDIAN)
		self.uc.mem_map(0x00200000, 0x00100000)
		self.uc.mem_write(0x00200000, rom[:0x00100000])
		self.uc.mem_map(0x00100000, 0x00040000)
		self.uc.hook_add(UC_HOOK_CODE, self._hook)

	def _hook(self, uc, address, _size, _user_data):
		pc = address & ~1
		if self.target is not None and pc == self.target:
			self.result = {"target": pc}
			uc.emu_stop()
		elif self.target is None and pc == self.mode_store:
			self.result = {
				"mode": uc.reg_read(UC_ARM_REG_R0) & 0xff,
				"object": uc.reg_read(UC_ARM_REG_R1),
			}
			uc.emu_stop()
		elif self.target is not None and not self.entry <= pc < 0x00256000:
			uc.emu_stop()
		elif self.target is None and not self.entry <= pc < self.end:
			uc.emu_stop()

	def classify(self, status):
		self.result = None
		self.uc.reg_write(UC_ARM_REG_SP, 0x0013f000)
		self.uc.reg_write(UC_ARM_REG_R0, status)
		self.uc.reg_write(UC_ARM_REG_R1, 0)
		self.uc.reg_write(UC_ARM_REG_LR, 0)
		# The dispatcher branches to leaf handlers beyond code_end. The code hook
		# provides the actual execution boundary and must see a requested leaf.
		try:
			self.uc.emu_start(self.entry | 1, 0, count=4000)
		except UcError:
			# Leaf handlers may dereference runtime-owned objects. A target hit has
			# already stopped cleanly; other inputs are irrelevant to target mode.
			if self.target is None:
				raise
		return self.result


def exhaustive_map(rom, contract, target=None):
	dispatcher = Dispatcher(rom, contract, target)
	result = {}
	for status in range(0x10000):
		classification = dispatcher.classify(status)
		if classification is not None:
			result[status] = classification.get("mode", classification.get("target"))
	return result


def expected_map(contract):
	return {number(item["status"]): number(item["mode"]) for item in contract["expected"]}


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--profile", type=Path, default=DEFAULT_PROFILE)
	parser.add_argument("--rom", type=Path, default=DEFAULT_ROM)
	parser.add_argument("--check", action="store_true")
	parser.add_argument("--target", type=number,
		help="list dispatcher inputs that execute this firmware address")
	args = parser.parse_args()

	profile = json.loads(args.profile.read_text())
	contract = profile["controller_dispatch_contract"]
	actual = exhaustive_map(args.rom.read_bytes(), contract, args.target)
	for status, mode in sorted(actual.items()):
		if args.target is None:
			print(f"status=0x{status:04x} mode=0x{mode:02x}")
		else:
			print(f"status=0x{status:04x} target=0x{mode:08x}")

	if args.check and args.target is not None:
		parser.error("--check and --target are mutually exclusive")
	if args.check and actual != expected_map(contract):
		print("controller dispatch map differs from reviewed profile", file=sys.stderr)
		print(f"expected={expected_map(contract)!r}", file=sys.stderr)
		print(f"actual={actual!r}", file=sys.stderr)
		return 1
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
