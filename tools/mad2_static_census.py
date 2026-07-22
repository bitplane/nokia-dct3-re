#!/usr/bin/env python3
"""Conservatively census direct MAD2 MMIO accesses in a swap16 Thumb image.

The analysis begins only at PC-relative literals whose effective value lies in
the MAD2 byte window.  It follows register copies and constant additions within
the surrounding routine, but never guesses values across a call.  The output
therefore describes resolved direct accesses, not every possible dynamic or
table-driven access.
"""

import argparse
import json
from collections import Counter
from pathlib import Path

import capstone

try:
	from tools.message_census import decode_image, effective_u32, literal_pool_address
except ModuleNotFoundError:  # Direct execution from tools/.
	from message_census import decode_image, effective_u32, literal_pool_address


ROOT = Path(__file__).resolve().parent.parent
FLASH_BASE = 0x200000
MAD2_BASE = 0x20000
MAD2_SIZE = 0x100
DEFAULT_ROMS = (
	("v6.00", ROOT / "roms/3210f600a_swap16.bin"),
	("v5.01", ROOT / "roms/nokia_3210_nse-8_v05_01_full_hu_swap16.bin"),
)


def parse_int(value):
	return int(value, 0)


def register_name(insn, operand):
	return insn.reg_name(operand.reg) if operand.type == capstone.arm.ARM_OP_REG else None


def destination_register(insn):
	if not insn.operands or insn.mnemonic.startswith(("b", "cmp", "str", "tst", "push")):
		return None
	return register_name(insn, insn.operands[0])


def literal_effective_value(insn, data, image_base):
	pool = literal_pool_address(insn)
	if pool is None:
		return None
	offset = pool - image_base
	if offset < 0 or offset + 4 > len(data):
		return None
	return effective_u32(data, offset)


def is_return(insn):
	if insn.mnemonic == "pop" and "pc" in insn.op_str:
		return True
	return insn.mnemonic in ("bx", "mov") and insn.op_str.startswith("pc,")


def apply_pointer_update(insn, pointers, data, image_base):
	"""Update known pointer values after one instruction."""
	dst = destination_register(insn)
	if dst is None:
		return
	value = None
	ops = insn.operands
	if insn.mnemonic == "ldr":
		value = literal_effective_value(insn, data, image_base)
	elif insn.mnemonic in ("mov", "movs") and len(ops) == 2:
		src = register_name(insn, ops[1])
		if src is not None:
			value = pointers.get(src)
	elif insn.mnemonic in ("add", "adds", "sub", "subs") and len(ops) in (2, 3):
		left_op = ops[0] if len(ops) == 2 else ops[1]
		right_op = ops[1] if len(ops) == 2 else ops[2]
		left_reg = register_name(insn, left_op)
		left = pointers.get(left_reg) if left_reg is not None else (
			left_op.imm if left_op.type == capstone.arm.ARM_OP_IMM else None)
		right_reg = register_name(insn, right_op)
		right = pointers.get(right_reg) if right_reg is not None else (
			right_op.imm if right_op.type == capstone.arm.ARM_OP_IMM else None)
		if left is not None and right is not None:
			value = left + right if insn.mnemonic.startswith("add") else left - right
	pointers[dst] = None if value is None else value & 0xffffffff


def memory_access(insn, pointers):
	if not insn.mnemonic.startswith(("ldr", "str")) or len(insn.operands) < 2:
		return None
	mem_op = insn.operands[-1]
	if mem_op.type != capstone.arm.ARM_OP_MEM or mem_op.mem.base == capstone.arm.ARM_REG_PC:
		return None
	base_reg = insn.reg_name(mem_op.mem.base)
	base = pointers.get(base_reg)
	if base is None or mem_op.mem.index:
		return None
	address = (base + mem_op.mem.disp) & 0xffffffff
	if not MAD2_BASE <= address < MAD2_BASE + MAD2_SIZE:
		return None
	width = {"ldrb": 1, "strb": 1, "ldrh": 2, "strh": 2, "ldr": 4, "str": 4}.get(insn.mnemonic)
	return {
		"pc": insn.address,
		"kind": "read" if insn.mnemonic.startswith("ldr") else "write",
		"width": width,
		"address": address,
		"offset": address - MAD2_BASE,
		"instruction": f"{insn.mnemonic} {insn.op_str}",
		"base_register": base_reg,
	}


def analyze_image(data, image_base=FLASH_BASE, max_seed_span=192):
	instructions = decode_image(data, image_base)
	accesses = {}
	seed_count = 0
	seed_terminations = Counter()
	for seed_index, seed in enumerate(instructions):
		value = literal_effective_value(seed, data, image_base) if seed else None
		if value is None or not MAD2_BASE <= value < MAD2_BASE + MAD2_SIZE:
			continue
		seed_reg = destination_register(seed)
		if seed_reg is None:
			continue
		seed_count += 1
		pointers = {seed_reg: value}
		seed_hits = 0
		for insn in instructions[seed_index + 1:seed_index + 1 + max_seed_span]:
			if insn is None:
				seed_terminations["decode_gap"] += 1
				break
			access = memory_access(insn, pointers)
			if access:
				access["seed_pc"] = seed.address
				access["seed_value"] = value
				accesses[(access["pc"], access["kind"], access["address"])] = access
				seed_hits += 1
			if insn.mnemonic in ("bl", "blx"):
				for reg in ("r0", "r1", "r2", "r3"):
					pointers[reg] = None
			else:
				apply_pointer_update(insn, pointers, data, image_base)
			if is_return(insn):
				seed_terminations["return"] += 1
				break
		else:
			seed_terminations["span_limit"] += 1
		if not seed_hits:
			seed_terminations["without_access"] += 1
	result = sorted(accesses.values(), key=lambda item: (item["pc"], item["address"], item["kind"]))
	return result, {
		"literal_seeds": seed_count,
		"resolved_accesses": len(result),
		"seed_terminations": dict(sorted(seed_terminations.items())),
	}


def summarize(label, path, accesses, coverage):
	by_offset = Counter()
	for access in accesses:
		by_offset[(access["offset"], access["kind"], access["width"])] += 1
	return {
		"label": label,
		"rom": str(path),
		"coverage": coverage,
		"accesses": accesses,
		"offset_summary": [
			{"offset": offset, "kind": kind, "width": width, "sites": count}
			for (offset, kind, width), count in sorted(by_offset.items())
		],
	}


def markdown(reports):
	lines = ["# MAD2 static access census", "",
		"Direct accesses resolved from swap16 Thumb literal seeds. Dynamic pointers and",
		"indirect/table-driven accesses are outside this conservative census.", "",
		"## Reviewed clock/reset anchors", "",
		"Both supported ROMs expose the same direct-access shape: four reset-cause reads",
		"and three reset-control writes; four Timer-1 current reads and eight destination",
		"reads; and ten read-modify-write sites for the peripheral clock register. The",
		"paired Timer-1 routines use FIQ5 as their destination race signal and compare",
		"`round((destination - current) / 8)` with Timer 0's remaining post-divider",
		"interval. The detailed tables below are reproducibility data, not additional",
		"semantic claims.", ""]
	for report in reports:
		coverage = report["coverage"]
		lines += [f"## {report['label']}", "",
			f"Seeds: {coverage['literal_seeds']}; resolved sites: {coverage['resolved_accesses']}; "
			f"terminations: `{coverage['seed_terminations']}`.", "",
			"| offset | direction | width | sites | PCs |", "|---:|---|---:|---:|---|"]
		for summary in report["offset_summary"]:
			pcs = [f"`0x{item['pc']:08x}`" for item in report["accesses"]
				if item["offset"] == summary["offset"] and item["kind"] == summary["kind"]
				and item["width"] == summary["width"]]
			lines.append(f"| `0x{summary['offset']:02x}` | {summary['kind']} | "
				f"{summary['width']} | {summary['sites']} | {', '.join(pcs)} |")
		lines.append("")
	return "\n".join(lines)


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("--rom", action="append", nargs=2, metavar=("LABEL", "PATH"),
		help="ROM label and swap16 image; repeat for paired analysis")
	parser.add_argument("--json", type=Path)
	parser.add_argument("--markdown", type=Path)
	parser.add_argument("--check", action="store_true",
		help="require both supported ROMs and their known Timer-1 read anchors")
	args = parser.parse_args()
	roms = [(label, Path(path)) for label, path in args.rom] if args.rom else list(DEFAULT_ROMS)
	reports = []
	for label, path in roms:
		accesses, coverage = analyze_image(path.read_bytes())
		reports.append(summarize(label, path, accesses, coverage))
	if args.check:
		if {report["label"] for report in reports} != {"v6.00", "v5.01"}:
			raise SystemExit("--check requires the supported v6.00 and v5.01 pair")
		expected = {"v6.00": {0x2aaaa8, 0x2aaab0, 0x2aab72, 0x2aab82},
			"v5.01": {0x2a7738, 0x2a7740, 0x2a7802, 0x2a7812}}
		for report in reports:
			pcs = {item["pc"] for item in report["accesses"] if item["offset"] in (4, 6)}
			missing = expected[report["label"]] - pcs
			if missing:
				raise SystemExit(f"{report['label']} missing Timer-1 anchors: " +
					", ".join(f"0x{pc:08x}" for pc in sorted(missing)))
			summary = {(item["offset"], item["kind"], item["width"]): item["sites"]
				for item in report["offset_summary"]}
			expected_shape = {
				(0x01, "read", 1): 4, (0x01, "write", 1): 3,
				(0x04, "read", 2): 4, (0x06, "read", 2): 8,
				(0x0d, "read", 1): 10, (0x0d, "write", 1): 10,
			}
			for key, count in expected_shape.items():
				if summary.get(key) != count:
					raise SystemExit(
						f"{report['label']} MAD2 shape changed at {key}: "
						f"{summary.get(key)} != {count}")
	payload = {"schema": 1, "mad2_base": MAD2_BASE, "window_size": MAD2_SIZE, "roms": reports}
	if args.json:
		args.json.write_text(json.dumps(payload, indent=2) + "\n")
	if args.markdown:
		args.markdown.write_text(markdown(reports) + "\n")
	if not args.json and not args.markdown:
		print(markdown(reports))


if __name__ == "__main__":
	main()
