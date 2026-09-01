#!/usr/bin/env python3
"""Classify conservatively resolved MAD2 board-I/O accesses across DCT3 ROMs."""

import argparse
import json
from collections import Counter
from pathlib import Path

try:
	from tools.mad2_static_census import analyze_image
except ModuleNotFoundError:
	from mad2_static_census import analyze_image


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_ROMS = (
	("3210-v6.00", ROOT / "roms/noki3210/3210f600a.fls"),
	("3210-v5.01", ROOT / "roms/noki3210/3210f501.fls"),
	("3310-v6.39", ROOT / "roms/noki3310/3310f639e.fls"),
	("3330-v4.50", ROOT / "roms/noki3330/3330f450e.fls"),
	("3410-v5.46", ROOT / "roms/noki3410/3410f546e.fls"),
)

REGIONS = {
	"PUP": frozenset((0x15, 0x1b, 0x1c, 0x1d, 0x1e, 0x20, 0x22, 0x24)),
	"KBGPIO": frozenset(range(0x28, 0x2c)) |
		frozenset(range(0x68, 0x6c)) | frozenset(range(0xa8, 0xac)),
	"UIF": frozenset(range(0x30, 0x34)) |
		frozenset(range(0x70, 0x74)) | frozenset(range(0xb0, 0xb4)) |
		frozenset(range(0xf0, 0xf4)),
	"SELECT": frozenset((0x6f, 0xad, 0xae, 0xaf, 0xed, 0xee, 0xef)),
}


def swap16(data):
	result = bytearray(data)
	result[0::2], result[1::2] = data[1::2], data[0::2]
	return bytes(result)


def region_for_offset(offset):
	for name, offsets in REGIONS.items():
		if offset in offsets:
			return name
	return None


def summarize(label, path, accesses, coverage):
	selected = []
	for access in accesses:
		region = region_for_offset(access["offset"])
		if region:
			selected.append({**access, "region": region})
	counts = Counter((item["region"], item["offset"], item["kind"]) for item in selected)
	return {
		"label": label,
		"rom": str(path.relative_to(ROOT) if path.is_relative_to(ROOT) else path),
		"coverage": coverage,
		"accesses": selected,
		"summary": [
			{"region": region, "offset": offset, "kind": kind, "sites": sites}
			for (region, offset, kind), sites in sorted(counts.items())
		],
	}


def markdown(reports):
	lines = [
		"# MAD2 board-I/O static census", "",
		"Conservative literal-seeded Thumb analysis classifies direct PUP, KBGPIO,",
		"UIF and GENSIO SELECT accesses. Dynamic pointers and table-driven accesses",
		"are excluded, so a zero is bounded absence from this extraction method only.",
		"", "| ROM | PUP sites | KBGPIO sites | UIF sites | SELECT sites |",
		"| --- | ---: | ---: | ---: | ---: |",
	]
	for report in reports:
		by_region = Counter(item["region"] for item in report["accesses"])
		lines.append(f"| {report['label']} | {by_region['PUP']} | {by_region['KBGPIO']} | "
			f"{by_region['UIF']} | {by_region['SELECT']} |")
	for report in reports:
		lines += ["", f"## {report['label']}", "",
			f"Resolved MAD2 sites: {report['coverage']['resolved_accesses']}; "
			f"board-I/O subset: {len(report['accesses'])}.", "",
			"| block | offset | direction | sites | PCs |",
			"| --- | ---: | --- | ---: | --- |"]
		for item in report["summary"]:
			pcs = [f"`0x{access['pc']:08x}`" for access in report["accesses"]
				if access["region"] == item["region"] and
				access["offset"] == item["offset"] and access["kind"] == item["kind"]]
			lines.append(f"| {item['region']} | `0x{item['offset']:02x}` | "
				f"{item['kind']} | {item['sites']} | {', '.join(pcs)} |")
	return "\n".join(lines) + "\n"


def check(reports):
	if {report["label"] for report in reports} != {label for label, _ in DEFAULT_ROMS}:
		raise SystemExit("--check requires all five supported ROM controls")
	for report in reports:
		seen = {(item["region"], item["offset"], item["kind"])
			for item in report["accesses"]}
		for required in (("PUP", 0x15, "read"), ("PUP", 0x15, "write"),
				("KBGPIO", 0x6b, "read"), ("KBGPIO", 0x6b, "write")):
			if required not in seen:
				raise SystemExit(f"{report['label']} missing board-I/O anchor {required}")
	for label in ("3210-v6.00", "3210-v5.01"):
		report = next(item for item in reports if item["label"] == label)
		seen = {(item["region"], item["offset"], item["kind"])
			for item in report["accesses"]}
		for required in (("SELECT", 0x6f, "read"), ("SELECT", 0x6f, "write"),
				("SELECT", 0xaf, "read"), ("SELECT", 0xaf, "write")):
			if required not in seen:
				raise SystemExit(f"{label} missing SELECT anchor {required}")


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("--rom", action="append", nargs=2, metavar=("LABEL", "PATH"))
	parser.add_argument("--json", type=Path)
	parser.add_argument("--markdown", type=Path)
	parser.add_argument("--check", action="store_true")
	args = parser.parse_args()
	roms = [(label, Path(path)) for label, path in args.rom] if args.rom else list(DEFAULT_ROMS)
	reports = []
	for label, path in roms:
		accesses, coverage = analyze_image(swap16(path.read_bytes()))
		reports.append(summarize(label, path, accesses, coverage))
	if args.check:
		check(reports)
	payload = {
		"schema": 1,
		"method": "conservative MAD2 literal-seeded direct-access classification",
		"limitations": "dynamic pointers and indirect/table-driven accesses are excluded",
		"roms": reports,
	}
	if args.json:
		args.json.write_text(json.dumps(payload, indent=2) + "\n")
	if args.markdown:
		args.markdown.write_text(markdown(reports))
	if not args.json and not args.markdown:
		print(markdown(reports), end="")


if __name__ == "__main__":
	main()
