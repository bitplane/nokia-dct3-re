#!/usr/bin/env python3
"""Validate the mapped-pin 24C128 write-cycle and persistence fixture."""

import argparse
import re
from pathlib import Path


EVENT = re.compile(
	r"eeprom_fixture: mode=(?P<mode>write|read) "
	r"initial_ack=(?P<initial>[01]) busy_ack=(?P<busy>[01]) "
	r"ready_ack=(?P<ready>[01]) reads_ok=(?P<reads>[01]) "
	r"data_ok=(?P<data_ok>[01]) data=(?P<data>[0-9a-f,]+)")


def check(text, mode):
	matches = [item for item in EVENT.finditer(text) if item.group("mode") == mode]
	if len(matches) != 1:
		return [f"expected one {mode} fixture result, found {len(matches)}"]
	item = matches[0]
	errors = []
	if item.group("reads") != "1" or item.group("data_ok") != "1":
		errors.append(f"{mode} fixture did not read wrapped page data: {item.group('data')}")
	if mode == "write":
		if item.group("initial") != "1":
			errors.append("page write was not acknowledged")
		if item.group("busy") != "0":
			errors.append("device acknowledged during its self-timed write cycle")
		if item.group("ready") != "1":
			errors.append("device did not acknowledge after the write-cycle bound")
	return errors


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("log", type=Path)
	parser.add_argument("--mode", choices=("write", "read"), required=True)
	args = parser.parse_args()
	errors = check(args.log.read_text(errors="replace"), args.mode)
	if errors:
		raise SystemExit("EEPROM contract failed: " + "; ".join(errors))
	print(f"EEPROM contract: {args.mode} page-wrap data reproduced")


if __name__ == "__main__":
	main()
