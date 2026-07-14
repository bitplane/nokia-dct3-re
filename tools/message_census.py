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


def descriptor_storage(call, instruction_by_address, data, base):
	address = call["arguments"].get("descriptor")
	if address is not None:
		if base <= address < base + len(data):
			return "rom"
		if 0x00100000 <= address < 0x00180000:
			return "fixed_ram"
		return "address_outside_image"
	insn = instruction_by_address.get(call["callsite"] - 2)
	if insn and len(insn.operands) >= 2 and insn.operands[0].type == capstone.arm.ARM_OP_REG:
		if insn.reg_name(insn.operands[0].reg) == "r2" and insn.mnemonic in ("mov", "adds", "add"):
			source = insn.operands[1]
			if source.type == capstone.arm.ARM_OP_REG:
				return "stack" if insn.reg_name(source.reg) == "sp" else "dynamic_ram"
	return "unresolved_pointer"


def service5_candidacy(call):
	service = call["arguments"].get("service")
	if service is None:
		return "dynamic_service_unresolved"
	return "candidate" if service == 5 else "excluded_other_service"


def extract_descriptors(profile, calls, instruction_by_address, data, base):
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
			"storage": descriptor_storage(call, instruction_by_address, data, base),
			"service": call["arguments"].get("service"),
			"service5_candidacy": service5_candidacy(call),
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
	if kind == "decoded_address":
		return insn is not None
	return False


def load_runtime_records(profile, manifest_id, subsystems, paths):
	records = []
	seen = {}
	patterns = [(item, re.compile(item["regex"])) for item in profile.get("runtime_patterns", [])
		if item.get("subsystem", "unscoped") in subsystems]
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
				for key in ("status", "caller", "command", "payload", "task", "class", "source_node", "destination_node",
						"service", "ordinal", "descriptor", "event", "callback", "enable", "mask"):
					if key in fields:
						fields[key] = int(fields[key], 16)
				key = (manifest_id, str(path), item["kind"], tuple(sorted(fields.items())))
				if key in seen:
					seen[key]["count"] += 1
				else:
					record = {"kind": item["kind"], "subsystem": item.get("subsystem", "unscoped"),
						"manifest": manifest_id, "fields": fields, "file": str(path),
						"line": line_number, "count": 1, "classification": "observed",
						"provenance": "observed_runtime"}
					seen[key] = record
					records.append(record)
	return records


def load_runtime(profile, manifest_paths, legacy_paths):
	records = []
	manifests = []
	for manifest_path in manifest_paths:
		manifest = json.loads(manifest_path.read_text())
		paths = [(ROOT / item).resolve() if not Path(item).is_absolute() else Path(item)
			for item in manifest.get("logs", [])]
		missing = [str(path) for path in paths if not path.is_file()]
		available = [path for path in paths if path.is_file()]
		records.extend(load_runtime_records(profile, manifest["id"], set(manifest.get("subsystems", [])), available))
		manifests.append({
			"id": manifest["id"], "description": manifest["description"],
			"subsystems": manifest.get("subsystems", []), "path": str(manifest_path),
			"logs": [str(path) for path in paths], "missing_logs": missing,
			"available": bool(paths) and not missing
		})
	if legacy_paths:
		subsystems = {item.get("subsystem", "unscoped") for item in profile.get("runtime_patterns", [])}
		records.extend(load_runtime_records(profile, "ad_hoc", subsystems, legacy_paths))
		manifests.append({"id": "ad_hoc", "description": "Unscoped command-line runtime logs",
			"subsystems": sorted(subsystems), "path": None, "logs": [str(path) for path in legacy_paths],
			"missing_logs": [], "available": True})
	return records, manifests


def subsystem_runtime_available(manifests, subsystem):
	return any(item["available"] and subsystem in item["subsystems"] for item in manifests)


def contact_service_inventory(profile, calls, runtime):
	constructors = [call for call in calls if call["api"] == "contact_message_alloc"]
	commands = []
	for definition in profile.get("contact_service_commands", []):
		record = dict(definition)
		command = number(record["command"])
		record["command"] = command
		record["consumer"] = number(record["consumer"])
		record["constructors"] = [call for call in constructors if call["arguments"].get("command") == command]
		expected = record.pop("expected_constructor", None)
		if expected:
			expected_callsite = number(expected["callsite"])
			expected_length = number(expected["payload_length"])
			record["constructor_anchor_valid"] = any(call["callsite"] == expected_callsite and
				call["arguments"].get("payload_length") == expected_length for call in record["constructors"])
		else:
			record["constructor_anchor_valid"] = not record["constructors"]
		record["runtime"] = {}
		for kind in ("contact_construct", "contact_send", "contact_receive"):
			matches = [item for item in runtime if item["kind"] == kind and item["fields"].get("command") == command]
			record["runtime"][kind] = {
				"occurrences": sum(item["count"] for item in matches),
				"profiles": sorted({item["file"] for item in matches})
			}
		commands.append(record)
	return {
		"constructor_callsites_scanned": len(constructors),
		"commands": commands,
		"all_constructor_anchors_valid": all(item["constructor_anchor_valid"] for item in commands)
	}


def computed_r0_constructions(instructions, data, base, status):
	"""Find conservative straight-line constructions of a requested value in r0.

	This is supporting evidence, not a producer claim. State is discarded across
	calls, branches, returns, and function-entry pushes so values are never
	propagated through an unresolved control-flow join.
	"""
	registers = {f"r{i}": None for i in range(8)}
	result = []
	for insn in instructions:
		if not insn:
			registers = {f"r{i}": None for i in range(8)}
			continue
		if insn.mnemonic == "push" and "lr" in insn.op_str:
			registers = {f"r{i}": None for i in range(8)}
		apply_constant(insn, registers, data, base)
		if written_register(insn) == "r0" and registers.get("r0") == status:
			result.append({"address": insn.address, "instruction": f"{insn.mnemonic} {insn.op_str}"})
		if insn.mnemonic in ("bl", "blx"):
			for reg in ("r0", "r1", "r2", "r3"):
				registers[reg] = None
		elif insn.mnemonic.startswith("b") or insn.mnemonic == "pop":
			registers = {f"r{i}": None for i in range(8)}
	return result


def catalogue_predecessors(contract, data, base, status, instructions=None, calls=None):
	"""Return catalogue-mode packed inputs whose decoded sequence emits status."""
	if not contract:
		return []
	address = number(contract["address"])
	entries = number(contract["entries"])
	entry_size = number(contract.get("entry_size", 4))
	result = []
	for input_status in range(entries):
		offset = address + input_status * entry_size - base
		sequence_offset = None
		for ordinal in range(number(contract.get("max_sequence_events", 64))):
			if offset < 0 or offset + 4 > len(data):
				break
			packed = effective_u32(data, offset)
			offset += 4
			if packed == 0x00dc:
				break
			if (packed & 0x1fff) == status:
				sequence_offset = ordinal
				break
			argument_count = (packed >> 14) & 3
			offset += argument_count * 4
		if sequence_offset is not None:
			# 0x2aee20 enters this table only when (packed & 0xa000) == 0x2000.
			# Bit 14 is the argument-count bit and is outside that mode mask, so
			# both packed forms select the same catalogue sequence.
			packed_inputs = [0x2000 | input_status, 0x6000 | input_status]
			literal_loads = []
			computed_r0 = []
			direct_calls = []
			for packed in packed_inputs:
				if instructions is not None:
					literal_loads.extend(insn.address for insn in instructions
						if literal_value(insn, data, base) == packed)
					computed_r0.extend(item["address"] for item in
						computed_r0_constructions(instructions, data, base, packed))
				if calls is not None:
					direct_calls.extend(call["callsite"] for call in calls
						if call["arguments"].get("packed_event") == packed)
			result.append({"status_index": input_status,
				"packed_inputs": packed_inputs,
				"entry": address + input_status * entry_size,
				"sequence_offset": sequence_offset,
				"producer_evidence": {"literal_loads": sorted(set(literal_loads)),
					"computed_r0_constructions": sorted(set(computed_r0)),
					"direct_calls": sorted(set(direct_calls))}})
	return result


def status_inventory(instructions, calls, descriptors, data, base, status,
		catalogue_contract=None):
	literal_loads = [insn.address for insn in instructions if literal_value(insn, data, base) == status]
	computed_r0 = computed_r0_constructions(instructions, data, base, status)
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
	return {"status": status, "literal_loads": literal_loads,
		"computed_r0_constructions": computed_r0, "call_arguments": call_arguments,
		"descriptor_fields": descriptor_fields,
		"catalogue_predecessors": catalogue_predecessors(
			catalogue_contract, data, base, status, instructions, calls)}


def object_lifecycle_inventory(calls, assessments):
	"""Inventory direct 0x05e0 lifecycle constructors that carry an object word."""
	reviewed = {number(item["callsite"]): item for item in assessments}
	result = []
	for call in calls:
		arguments = call["arguments"]
		if call["api"] != "generic_event_generate" or arguments.get("event") != 0x05e0:
			continue
		if (arguments.get("argument_count") or 0) < 2:
			continue
		words = arguments.get("argument_words", [])
		record = {
			"callsite": call["callsite"],
			"packed_event": arguments["packed_event"],
			"argument_count": arguments["argument_count"],
			"selector": words[0] if words else None,
			"object": words[1] if len(words) > 1 else None,
			"extra_arguments": words[2:],
			"provenance": "extracted_static"
		}
		assessment = reviewed.get(call["callsite"])
		if assessment:
			record["assessment"] = {key: value for key, value in assessment.items() if key != "callsite"}
		result.append(record)
	extracted = {item["callsite"] for item in result}
	return {
		"constructors": result,
		"reviewed_callsites": sorted(reviewed),
		"missing_assessments": sorted(extracted - set(reviewed)),
		"stale_assessments": sorted(set(reviewed) - extracted),
		"coverage_complete": extracted == set(reviewed)
	}


def dynamic_packed_event_inventory(calls, assessments):
	"""Require reviewed bounds for packed-event calls whose r0 is runtime-built."""
	unresolved = [call for call in calls if call["api"] == "generic_event_generate" and
		call["arguments"].get("packed_event") is None]
	reviewed = {number(item["callsite"]): item for item in assessments}
	extracted = {call["callsite"] for call in unresolved}
	return {
		"calls": [{**call, "assessment": reviewed.get(call["callsite"])} for call in unresolved],
		"unresolved_calls": len(unresolved),
		"assessed_calls": sum(call["callsite"] in reviewed for call in unresolved),
		"missing_assessments": sorted(extracted - set(reviewed)),
		"stale_assessments": sorted(set(reviewed) - extracted),
		"coverage_complete": extracted == set(reviewed),
		"object_bearing_candidates": [item for item in assessments if item.get("argument_count", 0) >= 2],
		"can_publish_05e0": any(number(event) == 0x05e0 for item in assessments
			for event in item.get("possible_events", []))
	}


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
	lines += [
		"Descriptor storage: " + ", ".join(f"`{key}`={value}" for key, value in sorted(summary["descriptor_storage"].items())) + ".",
		"Service-5 candidacy among unresolved descriptors: " + ", ".join(f"`{key}`={value}" for key, value in sorted(summary["unresolved_descriptor_service5_candidacy"].items())) + ".", ""]
	lines += ["## Runtime manifests", ""]
	if result["runtime_manifests"]:
		for manifest in result["runtime_manifests"]:
			state = "available" if manifest["available"] else "missing"
			lines.append(f"- `{manifest['id']}` ({state}): {manifest['description']} [subsystems: {', '.join(manifest['subsystems']) or 'none'}]")
	else:
		lines.append("- No runtime manifest supplied; static extraction and reviewed runtime claims remain available.")
	lines.append("")
	lines += ["## Dynamic descriptor assessment", ""]
	for item in result.get("dynamic_descriptor_assessments", []):
		fields = [f"{key} `{item[key]}`" for key in ("event", "callback", "descriptor_source") if key in item]
		lines.append(f"- `{item['callsite']}` ({item['storage']}): {', '.join(fields)}; **{item['service5_population']}**.")
	lines.append("")
	absence = result["status_inventory_05e8"]
	publisher_callsites = ", ".join(
		f"`{hexadecimal(item['callsite'])}`" for item in absence["call_arguments"])
	lines += ["## 0x05e8 inventory", "",
		f"- Effective literal loads: {len(absence['literal_loads'])}",
		f"- Recovered argumentless global-event generators: {len(absence['call_arguments'])}",
		f"- Generator callsites: {publisher_callsites or 'none'}",
		f"- Decoded registration fields: {len(absence['descriptor_fields'])}", ""]
	publishers = result.get("status_05e8_publishers", [])
	if publishers:
		lines += ["### Publisher owners", ""]
		for item in publishers:
			triggers = ", ".join(f"`{value}`" for value in item["trigger_statuses"])
			callsites = ", ".join(f"`{value}`" for value in item["callsites"])
			lines.append(f"- callback `{item['callback_index']}` / `{item['entry']}`: {callsites}; triggers {triggers}; {item['role']}.")
		lines.append("")
	lifecycle = result["object_lifecycle_05e0"]
	lines += ["## Object-bearing 0x05dc lifecycle constructors", "",
		f"The ROM scan recovered {len(lifecycle['constructors'])} direct packed `0x05e0` constructors with at least two argument words. "
		"Argumentless and selector-only lifecycle events are excluded.", ""]
	for item in lifecycle["constructors"]:
		assessment = item.get("assessment", {})
		lines.append(f"- `{item['callsite']:#08x}`: selector `{hexadecimal(item['selector'])}`, "
			f"object `{hexadecimal(item['object'])}`, argc `{item['argument_count']}`; "
			f"**{assessment.get('classification', 'unreviewed')}** - {assessment.get('role', 'no reviewed role')}.")
	lines += ["", f"Assessment coverage: **{'complete' if lifecycle['coverage_complete'] else 'incomplete'}**; "
		f"missing {len(lifecycle['missing_assessments'])}, stale {len(lifecycle['stale_assessments'])}.", ""]
	dynamic = result["dynamic_packed_events"]
	lines += ["### Runtime-built packed events", "",
		f"The extractor leaves {dynamic['unresolved_calls']} packed-event values runtime-built; "
		f"{dynamic['assessed_calls']} have reviewed bounds. Assessment coverage is "
		f"**{'complete' if dynamic['coverage_complete'] else 'incomplete'}**.",
		f"Reviewed two-word candidates: {len(dynamic['object_bearing_candidates'])}; "
		f"can publish lifecycle event `0x05e0`: **{'yes' if dynamic['can_publish_05e0'] else 'no'}**.", ""]
	for item in dynamic["object_bearing_candidates"]:
		events = ", ".join(f"`{event}`" for event in item.get("possible_events", [])) or "unbounded"
		lines.append(f"- `{item['callsite']}`: events {events}; **{item['classification']}** - {item['role']}.")
	lines.append("")
	lines += ["### Registration-relevant intersection", "",
		"Intersecting these constructors with callbacks that contain direct `0x05e8` publishers leaves owners `0x21`, `0x22`, `0x26`, `0x3c`, and `0x54`, plus the dynamic `0x35..0x3a` classifier. Exhaustive concrete dispatch maps every classifier mode exclusively from proactive-SIM events `0x1777..0x1782`, excluding the whole classifier from ordinary registration. Backward tracing excludes every other member from the coherent bootstrap: callback `0x24` builds `0x21` only in framework mode 9 and emits the `0x0388` consumed by `0x26` only in mode 11, while the coherent run remains in mode 0; `0x22` is constructed by `0x21`, `0x3c` by `0x51`/`0x25`, and `0x54` by `0x55`/`0x42`. These are later fallback/cleanup convergence paths, not demonstrated ordinary-registration predecessors.", ""]
	scratch = result.get("dispatcher_scratch_contract")
	if scratch:
		writers = ", ".join(f"`{pc}`" for pc in scratch["runtime_writer_pcs"])
		lines += ["## Dispatcher scratch contract", "",
			f"- Range: `{scratch['address_range']}` ({scratch['classification']})",
			f"- Argument 1 / object slot when supplied by the callback ABI: `{scratch['argument_1']}`",
			f"- Coherent runtime writer PCs: {writers}",
			f"- Coverage: {scratch['coverage']}", ""]
	lines += ["## Acceptance chains", ""]
	for edge in result["semantic_edges"]:
		mark = "PASS" if edge["anchors_valid"] else "FAIL"
		lines.append(f"- **{mark}** `{edge['id']}`: `{edge['input_status']}` -> `{edge['output_status']}` ({edge['classification']}, {edge['provenance']})")
	lines += ["", "## 0x05e8 boundary", "",
		"Callback-table entry `0x28` (`0x2618e9`) accepts numeric status `0x05e8` and returns `0x05ea`. Reviewed control flow shows that branch does not consume an object argument from dispatcher scratch `0x110f1c`; the earlier object-bearing interpretation was incorrect.", "",
		"The census recovers argumentless in-ROM generators of global event `0x05e8` (`0xbd << 3`). That is the expected ABI: the packed event becomes the callback input even with zero argument words. These are genuine candidate publishers, not incomplete object constructors.", "",
		"The mapped task-21 `0x120c` -> `A0/12` -> D0 -> `0x177x` path is GSM 11.14 SIM Toolkit, not the ordinary registration predecessor. The current EF_PHASE=2 card correctly leaves it dormant; posting downstream events remains an invalid substitute.", ""]
	observed = [item for item in result["runtime"] if item["subsystem"] == "generic_service" and item["kind"] == "callback"]
	if observed:
		statuses = sorted({item["fields"]["status"] for item in observed})
		lines.append("Observed service-5 callback inputs in supplied coherent logs: " + ", ".join(f"`{hexadecimal(x)}`" for x in statuses) + ".")
	elif subsystem_runtime_available(result["runtime_manifests"], "generic_service"):
		lines.append("The supplied generic-service manifest contained no service-5 callback observations.")
	else:
		lines.append("No generic-service runtime manifest was supplied; reviewed runtime claims below are retained and are not replaced by contact-only evidence.")
	claims = [item for item in result["runtime_claims"] if item["subsystem"] == "generic_service"]
	if claims:
		lines += ["", "Reviewed runtime claims:"]
		for claim in claims:
			lines.append(f"- `{claim['id']}` ({claim['manifest']}, {claim['classification']}): {claim['statement']}")
	if result["runtime_status_inventory"]:
		lines += ["", "Target-chain statuses observed as task messages: " + ", ".join(
			f"`{int(status):#06x}`={count}" for status, count in result["runtime_status_inventory"].items()) + "."]
	contact = result["contact_service"]
	lines += ["", "## Contact-service command family", "",
		f"The ROM scan recovered {contact['constructor_callsites_scanned']} calls to `contact_message_alloc_234634`. "
		"The five target commands each have exactly one constructor; constructor existence is not treated as proof of an initiating producer.", ""]
	for command in contact["commands"]:
		constructors = ", ".join(f"`{item['callsite']:#08x}`/len `{item['arguments'].get('payload_length')}`" for item in command["constructors"]) or "none"
		runtime = command["runtime"]
		lines += [f"### Command `{command['command']:#04x}`: {command['name']}", "",
			f"- Incoming consumer: `{command['consumer']:#08x}`",
			f"- MCU constructor(s): {constructors} ({command['constructor_role']})",
			f"- Initiating-producer classification: **{command['producer_class']}** ({command['confidence']})",
			f"- Runtime construct/send/receive occurrences: {runtime['contact_construct']['occurrences']} / {runtime['contact_send']['occurrences']} / {runtime['contact_receive']['occurrences']}",
			f"- Evidence: {command['evidence']}", ""]
	lines += ["## Contact-service transport boundary", "",
		result["contact_service_boundary"]["statement"], "",
		"The numeric command and scheduler event `0x74` are separate namespaces. The ROM contains direct MCU producers of scheduler event `0x74` at `0x213fcc` and `0x214836`; they do not construct contact-service command `0x74`.", ""]
	lines += ["", "## Phase-two decision", "",
		"This bounded contact-service phase classifies the available MCU constructors and the observed transport behavior. A future full-ROM contract census can reuse the same distinction between an initiating request, a response/acknowledgement with the same id, and a scheduler event in another namespace.", ""]
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
	parser.add_argument("--runtime-manifest", type=Path, action="append", default=[])
	parser.add_argument("--inventory-status", type=number, action="append", default=[],
		help="add a status to the machine-readable static producer inventory")
	parser.add_argument("--require-runtime-subsystem", action="append", default=[],
		help="fail before writing outputs unless an available manifest covers this subsystem")
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
	descriptors = extract_descriptors(profile, calls, by_address, data, base)
	runtime, runtime_manifests = load_runtime(profile, args.runtime_manifest, args.runtime_log)
	missing_runtime = [subsystem for subsystem in args.require_runtime_subsystem
		if not subsystem_runtime_available(runtime_manifests, subsystem)]
	if missing_runtime:
		print("message_census: required runtime subsystem(s) unavailable: " + ", ".join(missing_runtime),
			file=sys.stderr)
		return 2
	contact_service = contact_service_inventory(profile, calls, runtime)
	catalogue_contract = profile.get("status_translation_table")
	inventory_05e8 = status_inventory(instructions, calls, descriptors, data, base, 0x05e8,
		catalogue_contract)
	requested_status_inventories = {
		hexadecimal(status): status_inventory(instructions, calls, descriptors, data, base, status,
			catalogue_contract)
		for status in dict.fromkeys(args.inventory_status)
	}
	lifecycle_05e0 = object_lifecycle_inventory(calls, profile.get("object_lifecycle_assessments", []))
	dynamic_packed_events = dynamic_packed_event_inventory(calls,
		profile.get("dynamic_packed_event_assessments", []))
	target_statuses = (0x05e8, 0x05ea, 0x07dd, 0x09d8, 0x0434)
	runtime_status_inventory = {}
	if subsystem_runtime_available(runtime_manifests, "generic_service"):
		runtime_status_inventory = {str(status): sum(item["count"] for item in runtime
			if item["subsystem"] == "generic_service" and item["kind"] == "message" and item["fields"].get("status") == status)
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
		"profile": {"name": profile["name"], "path": str(args.profile), "rom": str(args.rom)},
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
			"descriptor_storage": {kind: sum(item["storage"] == kind for item in descriptors)
				for kind in sorted({item["storage"] for item in descriptors})},
			"descriptor_service5_candidacy": {kind: sum(item["service5_candidacy"] == kind for item in descriptors)
				for kind in sorted({item["service5_candidacy"] for item in descriptors})},
			"unresolved_descriptor_service5_candidacy": {kind: sum(item["resolution"] != "rom" and item["service5_candidacy"] == kind for item in descriptors)
				for kind in sorted({item["service5_candidacy"] for item in descriptors if item["resolution"] != "rom"})},
			"runtime_observations": len(runtime)},
		"calls": calls, "callbacks": callbacks, "descriptor_registrations": descriptors,
		"consumers": consumers, "semantic_edges": edges,
		"status_inventory_05e8": inventory_05e8,
		"requested_status_inventories": requested_status_inventories,
		"object_lifecycle_05e0": lifecycle_05e0,
		"dynamic_packed_events": dynamic_packed_events,
		"status_05e8_publishers": profile.get("status_05e8_publishers", []),
		"dispatcher_scratch_contract": profile.get("dispatcher_scratch_contract"),
		"unresolved_contract": {"status": 0x05e8, "classification": "unresolved",
			"boundary": "organic 0x05e8 publisher activation and downstream readiness before callback-table index 0x28 dispatch",
			"external": "not_proven"},
		"runtime_status_inventory": runtime_status_inventory, "runtime": runtime,
		"runtime_manifests": runtime_manifests,
		"runtime_claims": profile.get("runtime_claims", []),
		"dynamic_descriptor_assessments": profile.get("dynamic_descriptor_assessments", []),
		"nodes": profile.get("nodes", [])
	}
	result["contact_service"] = contact_service
	result["contact_service_boundary"] = profile["contact_service_boundary"]
	report = render_report(result)
	if args.json:
		args.json.parent.mkdir(parents=True, exist_ok=True)
		args.json.write_text(json.dumps(result, indent=2) + "\n")
	if args.report:
		args.report.parent.mkdir(parents=True, exist_ok=True)
		args.report.write_text(report)
	if not args.json and not args.report:
		print(report)
	if args.check and (not all(edge["anchors_valid"] for edge in edges) or not callback_extent_valid or
			not contact_service["all_constructor_anchors_valid"] or not lifecycle_05e0["coverage_complete"] or
			not dynamic_packed_events["coverage_complete"] or dynamic_packed_events["can_publish_05e0"]):
		failed = [edge["id"] for edge in edges if not edge["anchors_valid"]]
		if not callback_extent_valid:
			failed.append("callback_table_extent")
		if not contact_service["all_constructor_anchors_valid"]:
			failed.append("contact_service_constructor_anchors")
		if not lifecycle_05e0["coverage_complete"]:
			failed.append("object_lifecycle_assessment_coverage")
		if not dynamic_packed_events["coverage_complete"]:
			failed.append("dynamic_packed_event_assessment_coverage")
		if dynamic_packed_events["can_publish_05e0"]:
			failed.append("dynamic_packed_event_05e0_candidate")
		print(f"message_census: failed checks: {', '.join(failed)}", file=sys.stderr)
		return 1
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
