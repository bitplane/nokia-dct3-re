#!/usr/bin/env python3
"""Check that a firmware trace reaches an evidenced, product-specific radio boundary."""

import argparse
import collections
import hashlib
import pathlib

try:
	from tools.dsp_packet_semantics_census import parse
except ModuleNotFoundError:
	from dsp_packet_semantics_census import parse


NHM5_V639_SHA256 = "975ec791205f026d647254ee772d7fa32691fa50c72a68eecdaff7c8a5921442"


def swap16(data: bytes) -> bytes:
	result = bytearray(data)
	for offset in range(0, len(result) - 1, 2):
		result[offset], result[offset + 1] = result[offset + 1], result[offset]
	return bytes(result)


def check_nhm5_static(path: pathlib.Path, packets: list[dict]) -> None:
	rom = path.read_bytes()
	digest = hashlib.sha256(rom).hexdigest()
	if digest != NHM5_V639_SHA256:
		raise SystemExit(f"NHM-5 static check requires v6.39 {NHM5_V639_SHA256}, got {digest}")

	unique = {(packet["type"], packet["data"]): packet for packet in packets
			if packet["direction"] == "tx"}
	type20 = next((packet for (packet_type, _), packet in unique.items() if packet_type == 0x20), None)
	type22 = next((packet for (packet_type, _), packet in unique.items() if packet_type == 0x22), None)
	if type20 is None or type22 is None:
		raise SystemExit("NHM-5 static check needs observed type 0x20 and 0x22 packets")

	def require_payload(packet: dict, address: int) -> None:
		offset = address - 0x200000
		payload = bytes.fromhex(packet["data"])
		if rom[offset:offset + len(payload)] != payload:
			raise SystemExit(
				f"NHM-5 type 0x{packet['type']:02x} does not match ROM table at 0x{address:08x}"
			)

	require_payload(type22, 0x00325E28)
	require_payload(type20, 0x00325EF0)

	image = swap16(rom)
	constructors = {
		0x002C2A02: bytes.fromhex("022020802020a0702220e070"),
		0x002C2A5E: bytes.fromhex("022020804420a0702020e070"),
		0x002C2AB8: bytes.fromhex("022020802020a0702120e070"),
		# Allocate/clear 0xa4 bytes, publish a 0xa0-byte type-0x56 payload,
		# then fill all 160 candidate bytes with the erased-entry sentinel.
		0x002A7DD8: bytes.fromhex(
			"10b5a420f2f718fd041c0021a42249f051ff"
			"a020a070022020805620e070"
		),
		# Four-byte type-0x55 power-sweep constructor reached only after task 10
		# consumes the measurement results.
		0x002A7BAA: bytes.fromhex(
			"70b50d1c061c0820f2f72dfe041c002108224af066f8"
			"0420a070022020805520e070"
		),
		# The 0x8b consumer advances forty four-byte records, reading the
		# big-endian channel at object offsets 6/7 and signed RSSI at offset 9.
		0x0028AA4C: bytes.fromhex(
			"0120824601360435002e2cd500980069c188828889180089"
			"4018404523dae879a979090240180004070c697a08060016"
		),
		# The 0x8a NO_PSW_FOUND consumer reads its big-endian channel at report
		# object offsets 4/5 and resolves it against the populated candidates.
		0x0028A19C: bytes.fromhex(
			"30b50d1ca04c41790079000208180004000ca168fef74fff"
			"a060002812d0"
		),
	}
	for address, expected in constructors.items():
		offset = address - 0x200000
		if image[offset:offset + len(expected)] != expected:
			raise SystemExit(f"NHM-5 constructor anchor changed at 0x{address:08x}")

	print(
		"NHM-5 static boundary: type 22/32 and 20/68 are exact profile-selected ROM "
		"tables; type 21/32 is a separately constructed two-table composite; "
		"type 56/160 is a firmware-built 80-entry candidate-channel list"
	)


def check_nhm5_startup(path: pathlib.Path, rom: pathlib.Path | None = None) -> None:
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
	if rom is not None:
		check_nhm5_static(rom, packets)


def check_nhm5_search(path: pathlib.Path, rom: pathlib.Path | None = None) -> None:
	check_nhm5_startup(path, rom)
	packets = parse("nhm5", path)
	search = next(packet for packet in packets
			if packet["direction"] == "tx" and packet["type"] == 0x56)
	results = next((packet for packet in packets
			if packet["direction"] == "rx" and packet["type"] == 0x8B), None)
	control = next((packet for packet in packets
			if packet["direction"] == "tx" and packet["type"] == 0x55), None)
	unsolicited_result = next((packet for packet in packets
			if packet["direction"] == "rx" and packet["time"] >= control["time"]
			and packet["type"] in (0x80, 0x8A)), None) if control is not None else None
	if results is None or results["length"] != 166:
		raise SystemExit("NHM-5 search did not receive its 166-byte type 0x8b result array")
	if not results["data"].startswith("0010005800c4"):
		raise SystemExit(
			"NHM-5 type 0x8b did not report requested channel 0x0058 "
			"at the laboratory cell's measured -60 dBm"
		)
	if control is None or control["length"] != 4 or control["data"] != "03050000":
		raise SystemExit("NHM-5 did not organically publish the observed type 0x55 control")
	if unsolicited_result is not None:
		raise SystemExit(
			"NHM-5 power-sweep frontier synthesized an unproved "
			f"type 0x{unsolicited_result['type']:02x} result"
		)
	if not search["time"] <= results["time"] < control["time"]:
		raise SystemExit("NHM-5 search/control transaction order changed")
	print(
		"NHM-5 search frontier: 56/160 candidate 0058 -> 8b/166 measured results -> "
		"55/4 power-sweep request; no invented success or NO_PSW_FOUND result"
	)


def main() -> None:
	parser = argparse.ArgumentParser()
	parser.add_argument("log", type=pathlib.Path)
	parser.add_argument("--profile", choices=("nhm5-startup", "nhm5-search"), required=True)
	parser.add_argument("--rom", type=pathlib.Path)
	args = parser.parse_args()
	if args.profile == "nhm5-startup":
		check_nhm5_startup(args.log, args.rom)
	elif args.profile == "nhm5-search":
		check_nhm5_search(args.log, args.rom)


if __name__ == "__main__":
	main()
