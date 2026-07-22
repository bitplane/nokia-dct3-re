#!/usr/bin/env python3
"""Summarize the bounded MAME MAD2-ledger register trace."""

import argparse
import json
import re
from pathlib import Path


LINE = re.compile(
	r"mad2_ledger: (?P<direction>[RW]) (?:bus=(?P<bus>[A-Z]+) )?off=(?P<offset>[0-9a-f]{2}) "
	r"data=(?P<data>[0-9a-f]{2})(?: old=(?P<old>[0-9a-f]{2}))? "
	r"pc=(?P<pc>[0-9a-f]{8}) t=(?P<time>[0-9.]+) (?P<description>.*)$",
	re.IGNORECASE,
)


def parse(text: str) -> list[dict]:
	accesses = []
	for line_number, line in enumerate(text.splitlines(), 1):
		match = LINE.search(line)
		if not match:
			continue
		item = match.groupdict()
		item.update({
			"bus": item["bus"] or "IO",
			"line": line_number,
			"offset": int(item["offset"], 16),
			"data": int(item["data"], 16),
			"old": int(item["old"], 16) if item["old"] is not None else None,
			"pc": int(item["pc"], 16),
			"time": float(item["time"]),
		})
		accesses.append(item)
	return accesses


def summarize(accesses: list[dict]) -> dict:
	seen = set()
	duplicates = []
	for item in accesses:
		key = (item["bus"], item["direction"], item["offset"])
		if key in seen:
			duplicates.append(item)
		seen.add(key)
	return {
		"schema": 1,
		"records": len(accesses),
		"read_offsets": sorted([item["bus"], item["offset"]] for item in accesses if item["direction"] == "R"),
		"write_offsets": sorted([item["bus"], item["offset"]] for item in accesses if item["direction"] == "W"),
		"unknown_offsets": sorted({(item["bus"], item["offset"]) for item in accesses
			if item["description"] == "<Unknown>"}),
		"duplicate_first_accesses": len(duplicates),
		"accesses": accesses,
	}


def markdown(summary: dict, source: Path) -> str:
	lines = [
		"# MAD2 access census",
		"",
		f"Source: `{source}`",
		"",
		f"Records: **{summary['records']}**; reads: **{len(summary['read_offsets'])}**; "
		f"writes: **{len(summary['write_offsets'])}**; duplicate first-access keys: "
		f"**{summary['duplicate_first_accesses']}**.",
		"",
		"| bus | dir | offset | value | old | PC | time (s) | description |",
		"| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |",
	]
	for item in summary["accesses"]:
		old = "-" if item["old"] is None else f"`0x{item['old']:02x}`"
		lines.append(f"| {item['bus']} | {item['direction']} | `0x{item['offset']:02x}` | `0x{item['data']:02x}` | "
			f"{old} | `0x{item['pc']:08x}` | {item['time']:.6f} | {item['description']} |")
	return "\n".join(lines) + "\n"


def main() -> None:
	parser = argparse.ArgumentParser()
	parser.add_argument("log", type=Path)
	parser.add_argument("--json", type=Path)
	parser.add_argument("--report", type=Path)
	parser.add_argument("--check", action="store_true",
		help="fail unless the input is a bounded first-access trace")
	args = parser.parse_args()

	summary = summarize(parse(args.log.read_text(errors="replace")))
	if args.check and (not summary["records"] or summary["duplicate_first_accesses"]):
		raise SystemExit("MAD2 census is empty or contains duplicate first-access keys")
	if args.json:
		args.json.parent.mkdir(parents=True, exist_ok=True)
		args.json.write_text(json.dumps(summary, indent=2) + "\n")
	if args.report:
		args.report.parent.mkdir(parents=True, exist_ok=True)
		args.report.write_text(markdown(summary, args.log))
	print(f"mad2 census: {summary['records']} records, {len(summary['read_offsets'])} reads, "
		f"{len(summary['write_offsets'])} writes, {len(summary['unknown_offsets'])} unknown offsets")


if __name__ == "__main__":
	main()
