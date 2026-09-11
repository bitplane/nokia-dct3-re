#!/usr/bin/env python3
"""Summarize firmware-driven external EEPROM line activity."""

import argparse
import json
import re
from pathlib import Path


FIELD = re.compile(r"(?P<name>eeprom_starts|eeprom_signal_writes)=(?P<value>\d+)")


def analyze(label, path):
	values = {match.group("name"): int(match.group("value"))
		for match in FIELD.finditer(Path(path).read_text(errors="replace"))}
	missing = {"eeprom_starts", "eeprom_signal_writes"} - values.keys()
	if missing:
		raise ValueError(f"{label} missing summary fields: {', '.join(sorted(missing))}")
	return {
		"label": label,
		"source": "organic two-second boot structural summary",
		**values,
		"serial_eeprom_active": (
			values["eeprom_starts"] > 1 and values["eeprom_signal_writes"] > 1000),
	}


def build_payload(summaries):
	reports = [analyze(label, path) for label, path in summaries]
	return {
		"schema_version": 1,
		"method": "physical PUP SDA/SCL transition counters from bounded organic boots",
		"limitations": (
			"a START-shaped edge alone is not a decoded I2C transaction; activity requires "
			"both repeated START edges and substantial serial line traffic"),
		"roms": reports,
	}


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("--summary", action="append", nargs=2, required=True,
		metavar=("LABEL", "PATH"))
	parser.add_argument("--json", type=Path)
	parser.add_argument("--check", action="store_true")
	args = parser.parse_args()
	payload = build_payload([(label, Path(path)) for label, path in args.summary])
	if args.check:
		if len(payload["roms"]) != 5:
			raise SystemExit("checked runtime census requires five supported ROM controls")
		active = {item["label"] for item in payload["roms"] if item["serial_eeprom_active"]}
		if active != {"3210-v6.00", "3210-v5.01"}:
			raise SystemExit(f"unexpected external EEPROM activity set: {sorted(active)}")
	output = json.dumps(payload, indent=2) + "\n"
	if args.json:
		args.json.parent.mkdir(parents=True, exist_ok=True)
		args.json.write_text(output)
	else:
		print(output, end="")


if __name__ == "__main__":
	main()
