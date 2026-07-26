#!/usr/bin/env python3
"""Check that a firmware trace reaches an evidenced, product-specific radio boundary."""

import argparse
import collections
import pathlib

try:
	from tools.dsp_packet_semantics_census import parse
except ModuleNotFoundError:
	from dsp_packet_semantics_census import parse


def check_nhm5_startup(path: pathlib.Path) -> None:
	packets = parse("nhm5", path)
	tx = [packet for packet in packets if packet["direction"] == "tx"]
	by_type = collections.defaultdict(list)
	for packet in tx:
		by_type[packet["type"]].append(packet)

	expected = {0x20: (2, 68), 0x21: (2, 32), 0x22: (2, 32), 0x56: (1, 160)}
	for packet_type, (count, length) in expected.items():
		observed = by_type[packet_type]
		if len(observed) != count:
			raise SystemExit(
				f"NHM-5 type 0x{packet_type:02x}: expected {count} packets, got {len(observed)}"
			)
		if any(packet["length"] != length for packet in observed):
			raise SystemExit(
				f"NHM-5 type 0x{packet_type:02x}: expected {length}-byte payloads"
			)

	if by_type[0x20][0]["data"] != by_type[0x20][1]["data"]:
		raise SystemExit("NHM-5 type 0x20 startup publications do not repeat")
	if by_type[0x22][0]["data"] != by_type[0x22][1]["data"]:
		raise SystemExit("NHM-5 type 0x22 startup publications do not repeat")
	if by_type[0x21][0]["data"][:-4] != by_type[0x21][1]["data"][:-4]:
		raise SystemExit("NHM-5 type 0x21 startup publications differ before their final word")
	if by_type[0x21][0]["data"][-4:] == by_type[0x21][1]["data"][-4:]:
		raise SystemExit("NHM-5 type 0x21 final words unexpectedly match")
	if by_type[0x56][0]["data"][:4] != "0058" or set(by_type[0x56][0]["data"][4:]) != {"f"}:
		raise SystemExit("NHM-5 type 0x56 does not have its observed 0x0058/erased-fill structure")

	if by_type[0x1A]:
		raise SystemExit("NHM-5 emitted NSE-8 type 0x1a search-list traffic")

	last_startup = max(packet["time"] for packet_type in (0x20, 0x21, 0x22)
			for packet in by_type[packet_type])
	if by_type[0x56][0]["time"] <= last_startup:
		raise SystemExit("NHM-5 type 0x56 did not follow the startup publication group")

	print(
		"NHM-5 radio boundary: "
		"2x20/68, 2x21/32, 2x22/32, 1x56/160; no NSE-8 type 0x1a"
	)


def main() -> None:
	parser = argparse.ArgumentParser()
	parser.add_argument("log", type=pathlib.Path)
	parser.add_argument("--profile", choices=("nhm5-startup",), required=True)
	args = parser.parse_args()
	if args.profile == "nhm5-startup":
		check_nhm5_startup(args.log)


if __name__ == "__main__":
	main()
