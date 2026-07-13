#!/usr/bin/env python3
"""Conservative message-topology census for DCT3 swap16 MCU images.

Static extraction and reviewed profile facts remain separate in the JSON. An
unknown register value is emitted as null; the tool never guesses through a
branch, call, or RAM-built descriptor.
"""

import argparse
import json
import re
import sys
from pathlib import Path

import capstone


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_PROFILE = ROOT / "tools/profiles/noki3210_v600.json"
DEFAULT_ROM = ROOT / "roms/3210f600a_swap16.bin"


def number(value):
	return int(value, 0) if isinstance(value, str) else value


def effective_u32(data, offset):
	raw = int.from_bytes(data[offset:offset + 4], "little")
	return ((raw << 16) | (raw >> 16)) & 0xffffffff


def cpu_byte(data, base, address):
	"""Read an MCU byte from a swap16 image (byte lanes cross within each halfword)."""
	offset = (address - base) ^ 1
	if offset < 0 or offset >= len(data):
		return None
	return data[offset]


def u16(data, base, address):
	offset = address - base
	if offset < 0 or offset + 2 > len(data):
		return None
	return int.from_bytes(data[offset:offset + 2], "little")


def decode_image(data, base):
	md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
	md.detail = True
	result = []
	address = base
	while address < base + len(data):
		offset = address - base
		decoded = list(md.disasm(data[offset:offset + 4], address, count=1))
		if decoded and not (decoded[0].size == 4 and decoded[0].mnemonic not in ("bl", "blx")):
			insn = decoded[0]
		else:
			decoded = list(md.disasm(data[offset:offset + 2], address, count=1))
			insn = decoded[0] if decoded and decoded[0].size == 2 else None
		result.append(insn)
		address += insn.size if insn else 2
	return result


def immediate_target(insn):
	if not insn or not insn.operands or insn.operands[0].type != capstone.arm.ARM_OP_IMM:
		return None
	return insn.operands[0].imm & 0xffffffff


def literal_value(insn, data, base):
	if not insn or insn.mnemonic != "ldr" or len(insn.operands) != 2:
		return None
	mem = insn.operands[1]
	if mem.type != capstone.arm.ARM_OP_MEM or mem.mem.base != capstone.arm.ARM_REG_PC:
		return None
	pool = ((insn.address + 4) & ~3) + mem.mem.disp
	offset = pool - base
	if offset < 0 or offset + 4 > len(data):
		return None
	return effective_u32(data, offset)


def written_register(insn):
	if not insn or not insn.operands:
		return None
	if insn.mnemonic.startswith(("cmp", "str", "tst", "b", "push")):
		return None
	first = insn.operands[0]
	return insn.reg_name(first.reg) if first.type == capstone.arm.ARM_OP_REG else None


def apply_constant(insn, registers, data, base):
	dst = written_register(insn)
	if dst is None:
		return
	value = None
	ops = insn.operands
	if insn.mnemonic in ("mov", "movs") and len(ops) == 2:
		if ops[1].type == capstone.arm.ARM_OP_IMM:
			value = ops[1].imm
		elif ops[1].type == capstone.arm.ARM_OP_REG:
			value = registers.get(insn.reg_name(ops[1].reg))
	elif insn.mnemonic == "ldr":
		value = literal_value(insn, data, base)
	elif insn.mnemonic in ("adds", "subs") and len(ops) in (2, 3):
		left_op = ops[0] if len(ops) == 2 else ops[1]
		right_op = ops[1] if len(ops) == 2 else ops[2]
		left = registers.get(insn.reg_name(left_op.reg)) if left_op.type == capstone.arm.ARM_OP_REG else left_op.imm
		right = registers.get(insn.reg_name(right_op.reg)) if right_op.type == capstone.arm.ARM_OP_REG else right_op.imm
		if left is not None and right is not None:
			value = left + right if insn.mnemonic == "adds" else left - right
	elif insn.mnemonic in ("lsls", "lsrs") and len(ops) in (2, 3):
		left_op = ops[0] if len(ops) == 2 else ops[1]
		right_op = ops[1] if len(ops) == 2 else ops[2]
		left = registers.get(insn.reg_name(left_op.reg)) if left_op.type == capstone.arm.ARM_OP_REG else left_op.imm
		right = registers.get(insn.reg_name(right_op.reg)) if right_op.type == capstone.arm.ARM_OP_REG else right_op.imm
		if left is not None and right is not None:
			value = left << right if insn.mnemonic == "lsls" else left >> right
	registers[dst] = None if value is None else value & 0xffffffff


def call_registers(instructions, index, data, base, window=48):
	start = max(0, index - window)
	for cursor in range(index - 1, start - 1, -1):
		insn = instructions[cursor]
		if not insn:
			start = cursor + 1
			break
		if insn.mnemonic.startswith("b") or insn.mnemonic in ("pop", "bx"):
			start = cursor + 1
			break
	registers = {f"r{i}": None for i in range(8)}
	for insn in instructions[start:index]:
		if not insn:
			registers = {f"r{i}": None for i in range(8)}
			continue
		if insn.mnemonic in ("bl", "blx"):
			for reg in ("r0", "r1", "r2", "r3"):
				registers[reg] = None
			continue
		apply_constant(insn, registers, data, base)
	return registers


def extract_calls(profile, instructions, data, base):
	apis = {number(api["address"]): api for api in profile["apis"]}
	records = []
	for index, insn in enumerate(instructions):
		if not insn or insn.mnemonic not in ("bl", "blx"):
			continue
		target = immediate_target(insn)
		if target not in apis:
			continue
		api = apis[target]
		registers = call_registers(instructions, index, data, base)
		arguments = {name: registers.get(reg) for name, reg in api.get("arguments", {}).items()}
		if api["kind"] == "packed_event" and "packed_event" in arguments and arguments["packed_event"] is not None:
			packed = arguments["packed_event"]
			arguments["event"] = packed & 0x1fff
			arguments["argument_count"] = (packed >> 14) & 0xff
			arguments["argument_words"] = [registers.get(f"r{reg}") for reg in range(1, 1 + min(arguments["argument_count"], 3))]
		records.append({
			"callsite": insn.address, "api": api["name"], "api_address": target,
			"kind": api["kind"], "arguments": arguments, "classification": "direct",
			"provenance": "extracted_static"
		})
	return records


def extract_callbacks(profile, data, base):
	table = profile["callback_table"]
	address = number(table["address"])
	records = []
	for index in range(table["entries"]):
		offset = address - base + index * table["entry_size"]
		pointer = effective_u32(data, offset)
		flags = effective_u32(data, offset + 4)
		records.append({"index": index, "address": address + index * table["entry_size"],
			"pointer": pointer, "flags": flags, "provenance": "extracted_static"})
	return records


def extract_consumers(profile, instruction_by_address, instructions, data, base):
	result = []
	for definition in profile.get("consumers", []):
		record = dict(definition)
		address = number(record["address"])
		statuses = {number(status) for status in record.get("statuses", [])}
		refs = []
		for insn in instructions:
			if not insn or not (address <= insn.address < address + 0x800):
				continue
			value = literal_value(insn, data, base)
			if value in statuses:
				refs.append({"address": insn.address, "status": value})
		record["address"] = address
		record["statuses"] = sorted(statuses)
		record["entry_decodes"] = address in instruction_by_address
		record["local_literal_evidence"] = refs
		result.append(record)
	return result


def decode_transient_descriptor(data, base, address):
	if address is None or not (base <= address <= base + len(data) - 0x1c):
		return None
	offset = address - base
	return {
		"address": address,
		"word_00": effective_u32(data, offset),
		"word_0c": effective_u32(data, offset + 0x0c),
		"event": u16(data, base, address + 0x10),
		"callback": u16(data, base, address + 0x12),
		"flags": effective_u32(data, offset + 0x14),
		"provenance": "extracted_static"
	}


def decode_resident_descriptor(data, base, address):
	if address is None or not (base <= address <= base + len(data) - 12):
		return None
	offset = address - base
	table = effective_u32(data, offset)
	result = {
		"address": address, "table": table,
		"header_word_04": effective_u32(data, offset + 4),
		"header_word_08": effective_u32(data, offset + 8),
		"entries": [], "provenance": "extracted_static"
	}
	# Resident arrays immediately precede their registration header. Each 0x14-byte
	# entry has an effective pointer, a reserved word, two native halfwords, and
	# two effective flag words. Do not scan beyond the owning header.
	if not (base <= table < address) or (address - table) % 0x14:
		return result
	for entry_address in range(table, address, 0x14):
		entry_offset = entry_address - base
		result["entries"].append({
			"address": entry_address,
			"word_00": effective_u32(data, entry_offset),
			"word_04": effective_u32(data, entry_offset + 4),
			"event": u16(data, base, entry_address + 8),
			"callback": u16(data, base, entry_address + 10),
			"flags_0c": effective_u32(data, entry_offset + 0x0c),
			"flags_10": effective_u32(data, entry_offset + 0x10)
		})
	return result


def extract_descriptors(profile, calls, data, base):
	by_name = {api["name"]: api for api in profile["apis"]}
	result = []
	for call in calls:
		api = by_name[call["api"]]
		argument = api.get("descriptor_argument")
		if not argument:
			continue
		address = call["arguments"].get(argument)
		decoder = decode_resident_descriptor if call["api"] == "service_register_resident" else decode_transient_descriptor
		decoded = decoder(data, base, address)
		record = {"callsite": call["callsite"], "api": call["api"], "address": address,
			"resolution": "rom" if decoded else "unresolved_or_runtime",
			"classification": "descriptor_generated", "provenance": "extracted_static"}
		if decoded:
			record["fields"] = decoded
		result.append(record)
	return result


def verify_anchor(anchor, instruction_by_address, callbacks, data, base):
	kind = anchor["type"]
	if kind == "callback_entry":
		record = callbacks[number(anchor["index"])]
		return record["pointer"] == number(anchor["pointer"]) and record["flags"] == number(anchor["flags"])
	insn = instruction_by_address.get(number(anchor["address"]))
	if kind == "call":
		return insn is not None and insn.mnemonic in ("bl", "blx") and immediate_target(insn) == number(anchor["target"])
	if kind == "effective_literal_load":
		return literal_value(insn, data, base) == number(anchor["value"])
	return False


def load_runtime(profile, paths):
	records = []
	seen = set()
	patterns = [(item, re.compile(item["regex"])) for item in profile.get("runtime_patterns", [])]
	for path in paths:
		for line_number, line in enumerate(path.read_text(errors="replace").splitlines(), 1):
			for item, pattern in patterns:
				match = pattern.search(line)
				if not match:
					continue
				fields = match.groupdict()
				for key in ("source", "destination"):
					if key in fields:
						fields[key] = int(fields[key])
				for key in ("status", "caller"):
					if key in fields:
						fields[key] = int(fields[key], 16)
				key = (item["kind"], tuple(sorted(fields.items())))
				if key not in seen:
					seen.add(key)
					records.append({"kind": item["kind"], "fields": fields, "file": str(path),
						"line": line_number, "classification": "observed", "provenance": "observed_runtime"})
	return records


def status_inventory(instructions, calls, descriptors, data, base, status):
	literal_loads = [insn.address for insn in instructions if literal_value(insn, data, base) == status]
	call_arguments = []
	for call in calls:
		for name, value in call["arguments"].items():
			if name in ("event", "status") and value == status:
				call_arguments.append({"callsite": call["callsite"], "api": call["api"], "argument": name})
	descriptor_fields = []
	for descriptor in descriptors:
		fields = descriptor.get("fields", {})
		entries = fields.get("entries", [fields])
		for entry in entries:
			for name in ("event", "callback"):
				if entry.get(name) == status:
					descriptor_fields.append({"registration_callsite": descriptor["callsite"],
						"descriptor": descriptor["address"], "entry": entry.get("address", descriptor["address"]),
						"field": name})
	return {"status": status, "literal_loads": literal_loads, "call_arguments": call_arguments,
		"descriptor_fields": descriptor_fields}


def hexadecimal(value):
	return f"0x{value:04x}" if isinstance(value, int) else "?"


def render_report(result):
	lines = ["# 3210 v6.00 message-topology census", "",
		"This report separates extracted ROM facts, reviewed static semantics, and coherent runtime observations.", ""]
	summary = result["summary"]
	lines += ["## Coverage", "",
		f"- Known API callsites: {summary['calls']} ({summary['resolved_call_arguments']} / {summary['call_arguments']} arguments resolved, {summary['argument_coverage_percent']:.1f}%)",
		f"- Callback-table entries: {summary['callbacks']} (`0x2db720` through `0x2dbb0f`; index `0x28` is `0x2db860`)",
		f"- Known consumer entries: {summary['consumers']} ({summary['consumer_entries_decoded']} entry addresses decode)",
		f"- Descriptor registrations: {summary['descriptor_registrations']} ({summary['rom_descriptors']} ROM descriptors decoded, {summary['unresolved_descriptors']} RAM-built or unresolved)",
		f"- Runtime observations: {summary['runtime_observations']}", ""]
	if result["runtime"]:
		lines += [f"Runtime profile: {result['profile']['runtime_profile']}.",
			"Runtime source(s): " + ", ".join(f"`{path}`" for path in sorted({item["file"] for item in result["runtime"]})) + ".", ""]
	absence = result["status_inventory_05e8"]
	lines += ["## 0x05e8 inventory", "",
		f"- Effective literal loads: {len(absence['literal_loads'])}",
		f"- Recovered argumentless global-event generators: {len(absence['call_arguments'])}",
		f"- Decoded registration fields: {len(absence['descriptor_fields'])}", ""]
	lines += ["## Acceptance chains", ""]
	for edge in result["semantic_edges"]:
		mark = "PASS" if edge["anchors_valid"] else "FAIL"
		lines.append(f"- **{mark}** `{edge['id']}`: `{edge['input_status']}` -> `{edge['output_status']}` ({edge['classification']}, {edge['provenance']})")
	lines += ["", "## 0x05e8 boundary", "",
		"The census finds `0x05e8` as the registered input of callback-table entry `0x28` (`0x2618e9`), not as a direct immediate producer callsite. The callback's object-bearing completion would return `0x05ea`, and the provider then constructs task-15 `0x07dd`.", "",
		"The strongest evidenced missing predecessor is therefore **generic-service session/queue population before callback dispatch**: firmware must register or populate an object-bearing transaction that selects callback `0x28` and supplies `0x05e8`. Directly posting `0x05e8`, `0x05ea`, `0x07dd`, or `0x09d8` would skip this ownership boundary.", "",
		"The census does find argumentless in-ROM generators of the global `0x05e8` event (`0xbd << 3`). They are triggers, not object producers: the packed-event ABI encodes zero argument words, so none supplies the object the callback path later expects. The quantified absence is narrower and stronger: no literal load and no recovered `0x05e8` generator carries an argument word, while unresolved RAM-built descriptors remain outside static coverage.", ""]
	observed = [item for item in result["runtime"] if item["kind"] == "callback"]
	if observed:
		statuses = sorted({item["fields"]["status"] for item in observed})
		lines.append("Observed service-5 callback inputs in supplied coherent logs: " + ", ".join(f"`{hexadecimal(x)}`" for x in statuses) + ".")
	else:
		lines.append("No runtime log was supplied; the boundary statement is static-only until a coherent trace is correlated.")
	if result["runtime_status_inventory"]:
		lines += ["", "Target-chain statuses observed as task messages: " + ", ".join(
			f"`{int(status):#06x}`={count}" for status, count in result["runtime_status_inventory"].items()) + "."]
	lines += ["", "## Phase-two decision", "",
		"A broader census is justified, but should be a separate phase. Its acceptance question should be: **does any in-ROM path self-issue contact-service commands `0x64`, `0x65`, `0x70`, `0x71`, or the `0x74` producer family, or are those contracts external?** That phase needs consumer-cascade recovery and RAM-built descriptor data-flow; merely adding more direct callsites would not answer it.", ""]
	return "\n".join(lines)


def self_test_byte_lanes():
	fixture = bytes((0x11, 0x22, 0x33, 0x44))
	assert cpu_byte(fixture, 0x1000, 0x1000) == 0x22
	assert cpu_byte(fixture, 0x1000, 0x1001) == 0x11
	assert effective_u32(fixture, 0) == 0x22114433


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("--profile", type=Path, default=DEFAULT_PROFILE)
	parser.add_argument("--rom", type=Path, default=DEFAULT_ROM)
	parser.add_argument("--runtime-log", type=Path, action="append", default=[])
	parser.add_argument("--json", type=Path)
	parser.add_argument("--report", type=Path)
	parser.add_argument("--check", action="store_true", help="fail if a reviewed acceptance anchor is not present")
	args = parser.parse_args()
	self_test_byte_lanes()
	profile = json.loads(args.profile.read_text())
	data = args.rom.read_bytes()
	base = number(profile["flash_base"])
	instructions = decode_image(data, base)
	by_address = {insn.address: insn for insn in instructions if insn}
	calls = extract_calls(profile, instructions, data, base)
	callbacks = extract_callbacks(profile, data, base)
	callback_table = profile["callback_table"]
	next_callback_offset = number(callback_table["address"]) - base + callback_table["entries"] * callback_table["entry_size"]
	next_callback_pointer = effective_u32(data, next_callback_offset)
	callback_extent_valid = all((item["index"] == 0 and item["pointer"] == 0) or
		(base <= item["pointer"] < base + len(data) and item["pointer"] & 1) for item in callbacks) and not (
		base <= next_callback_pointer < base + len(data) and next_callback_pointer & 1)
	consumers = extract_consumers(profile, by_address, instructions, data, base)
	descriptors = extract_descriptors(profile, calls, data, base)
	runtime = load_runtime(profile, args.runtime_log)
	inventory_05e8 = status_inventory(instructions, calls, descriptors, data, base, 0x05e8)
	target_statuses = (0x05e8, 0x05ea, 0x07dd, 0x09d8, 0x0434)
	runtime_status_inventory = {str(status): sum(item["kind"] == "message" and item["fields"].get("status") == status for item in runtime)
		for status in target_statuses}
	edges = []
	for definition in profile["semantic_edges"]:
		edge = dict(definition)
		edge["anchors_valid"] = all(verify_anchor(anchor, by_address, callbacks, data, base) for anchor in edge.pop("anchors"))
		edges.append(edge)
	profile_arguments = {api["name"]: set(api.get("arguments", {})) for api in profile["apis"]}
	resolved = sum(call["arguments"].get(name) is not None for call in calls for name in profile_arguments[call["api"]])
	total = sum(len(profile_arguments[call["api"]]) for call in calls)
	result = {
		"schema_version": 1,
		"edge_taxonomy": {
			"direct": "mechanically recovered call to a known transport API",
			"descriptor_generated": "event or callback encoded in a recovered registration descriptor",
			"observed": "seen in a coherent runtime trace",
			"dormant": "reviewed static path not seen in the coherent trace",
			"external": "producer proven outside the MCU ROM; none is claimed in this phase",
			"disproven_alternative": "previous edge interpretation contradicted by current evidence",
			"unresolved": "boundary is known but its producer or population mechanism is not"
		},
		"profile": {"name": profile["name"], "path": str(args.profile), "rom": str(args.rom),
			"runtime_profile": profile["runtime_profile"]},
		"summary": {"calls": len(calls), "call_arguments": total, "resolved_call_arguments": resolved,
			"unresolved_call_arguments": total - resolved,
			"argument_coverage_percent": 100.0 * resolved / total if total else 100.0,
			"callbacks": len(callbacks),
			"callback_extent_valid": callback_extent_valid,
			"next_callback_word": next_callback_pointer,
			"consumers": len(consumers),
			"consumer_entries_decoded": sum(item["entry_decodes"] for item in consumers),
			"descriptor_registrations": len(descriptors),
			"rom_descriptors": sum(item["resolution"] == "rom" for item in descriptors),
			"unresolved_descriptors": sum(item["resolution"] != "rom" for item in descriptors),
			"runtime_observations": len(runtime)},
		"calls": calls, "callbacks": callbacks, "descriptor_registrations": descriptors,
		"consumers": consumers, "semantic_edges": edges,
		"status_inventory_05e8": inventory_05e8,
		"unresolved_contract": {"status": 0x05e8, "classification": "unresolved",
			"boundary": "generic-service session/queue population before callback-table index 0x28 dispatch",
			"external": "not_proven"},
		"runtime_status_inventory": runtime_status_inventory, "runtime": runtime
	}
	report = render_report(result)
	if args.json:
		args.json.parent.mkdir(parents=True, exist_ok=True)
		args.json.write_text(json.dumps(result, indent=2) + "\n")
	if args.report:
		args.report.parent.mkdir(parents=True, exist_ok=True)
		args.report.write_text(report)
	if not args.json and not args.report:
		print(report)
	if args.check and (not all(edge["anchors_valid"] for edge in edges) or not callback_extent_valid):
		failed = [edge["id"] for edge in edges if not edge["anchors_valid"]]
		if not callback_extent_valid:
			failed.append("callback_table_extent")
		print(f"message_census: failed checks: {', '.join(failed)}", file=sys.stderr)
		return 1
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
