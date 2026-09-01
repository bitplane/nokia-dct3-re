#!/usr/bin/env python3
"""Census direct permanent-storage windows in supported DCT3 ROMs."""

import argparse
import json
from pathlib import Path

try:
	from tools.mad2_static_census import ROOT, analyze_image, is_return, summarize
	from tools.message_census import decode_image
except ModuleNotFoundError:
	from mad2_static_census import ROOT, analyze_image, is_return, summarize
	from message_census import decode_image


EEPROMSEL_BASE = 0x00A00000
EEPROMSEL_SIZE = 0x4000
PUP_OFFSETS = {0x20: "signal", 0x24: "direction"}
DEFAULT_ROMS = (
	("3210-v6.00", ROOT / "roms/noki3210/3210f600a.fls"),
	("3210-v5.01", ROOT / "roms/noki3210/3210f501.fls"),
	("3310-v6.39", ROOT / "roms/noki3310/3310f639e.fls"),
	("3330-v4.50", ROOT / "roms/noki3330/3330f450e.fls"),
	("3410-v5.46", ROOT / "roms/noki3410/3410f546e.fls"),
)


def swap16(data):
	result = bytearray(data)
	result[0::2], result[1::2] = data[1::2], data[0::2]
	return bytes(result)


def build_report(label, path):
	data = swap16(Path(path).read_bytes())
	parallel, coverage = analyze_image(
		data, window_base=EEPROMSEL_BASE, window_size=EEPROMSEL_SIZE)
	instructions = decode_image(data, 0x200000)
	index_by_pc = {item.address: index for index, item in enumerate(instructions) if item}
	for access in parallel:
		index = index_by_pc[access["seed_pc"]]
		context = [item for item in instructions[max(0, index - 64):index] if item]
		last_push = max((item.address for item in context if item.mnemonic == "push"), default=-1)
		last_return = max((item.address for item in context if is_return(item)), default=-1)
		access["classification"] = (
			"post-return data candidate" if last_return > last_push else "executable candidate")
	mad2, _ = analyze_image(data)
	return {
		"label": label,
		"rom": str(Path(path).relative_to(ROOT)),
		"parallel_window": summarize(label, path, parallel, coverage),
		"pup_accesses": [item for item in mad2 if item["offset"] in PUP_OFFSETS],
	}


def build_payload(roms):
	reports = [build_report(label, path) for label, path in roms]
	return {
		"schema_version": 1,
		"method": "conservative Thumb literal-seed propagation into EEPROMSelX and MAD2 PUP",
		"limitations": "dynamic pointers and table-driven accesses are excluded",
		"coverage": {
			"roms": len(reports),
			"parallel_literal_seeds": sum(
				item["parallel_window"]["coverage"]["literal_seeds"] for item in reports),
			"parallel_resolved_accesses": sum(
				item["parallel_window"]["coverage"]["resolved_accesses"] for item in reports),
			"parallel_executable_candidates": sum(
				item["classification"] == "executable candidate"
				for report in reports for item in report["parallel_window"]["accesses"]),
			"pup_resolved_accesses": sum(len(item["pup_accesses"]) for item in reports),
		},
		"roms": reports,
	}


def markdown(payload):
	lines = [
		"# Permanent-storage static census", "",
		"Conservative direct-access results for the five supported ROM controls.",
		"Dynamic pointers and table-driven accesses are outside coverage.", "",
		"| ROM | EEPROMSelX seeds | executable | rejected data | PUP signal R/W | PUP direction R/W |",
		"| --- | ---: | ---: | ---: | ---: | ---: |",
	]
	for report in payload["roms"]:
		window = report["parallel_window"]
		executable = sum(item["classification"] == "executable candidate" for item in window["accesses"])
		def counts(offset):
			items = [item for item in report["pup_accesses"] if item["offset"] == offset]
			return sum(item["kind"] == "read" for item in items), sum(item["kind"] == "write" for item in items)
		signal, direction = counts(0x20), counts(0x24)
		lines.append(f"| {report['label']} | {window['coverage']['literal_seeds']} | {executable} | "
			f"{len(window['accesses']) - executable} | {signal[0]}/{signal[1]} | {direction[0]}/{direction[1]} |")
	lines += ["", "Resolved PUP accesses identify wiring, not protocol. The absence proof covers",
		"only direct literal-derived accesses to the former parallel alias.", ""]
	return "\n".join(lines)


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("--json", type=Path)
	parser.add_argument("--markdown", type=Path)
	parser.add_argument("--check", action="store_true")
	args = parser.parse_args()
	payload = build_payload(DEFAULT_ROMS)
	if args.check and payload["coverage"]["parallel_executable_candidates"]:
		raise SystemExit("executable EEPROMSelX consumer found")
	if args.json:
		args.json.parent.mkdir(parents=True, exist_ok=True)
		args.json.write_text(json.dumps(payload, indent=2) + "\n")
	if args.markdown:
		args.markdown.write_text(markdown(payload))
	if not args.json and not args.markdown:
		print(json.dumps(payload, indent=2))


if __name__ == "__main__":
	main()
