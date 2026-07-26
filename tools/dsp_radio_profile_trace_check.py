#!/usr/bin/env python3
"""Check that a firmware trace reaches an evidenced, product-specific radio boundary."""

import argparse
import collections
import hashlib
import pathlib
import re

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
		# Four-byte type-0x55 terminal/control constructor reached from the
		# unsuccessful result path. It is not the start of the active sweep.
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
	trace = path.read_text(errors="replace")
	search = next(packet for packet in packets
			if packet["direction"] == "tx" and packet["type"] == 0x56)
	def first(direction: str, packet_type: int, after: float) -> dict | None:
		return next((packet for packet in packets
				if packet["direction"] == direction and packet["type"] == packet_type
				and packet["time"] >= after), None)

	sch = first("rx", 0x80, search["time"])
	configure = first("tx", 0x02, sch["time"] if sch else search["time"])
	no_psw_left = first("rx", 0x8F, configure["time"] if configure else search["time"])
	channel_changed = first("rx", 0x89, no_psw_left["time"] if no_psw_left else search["time"])
	ra_info = first("rx", 0x84, channel_changed["time"] if channel_changed else search["time"])
	bcch = first("rx", 0x80, ra_info["time"] if ra_info else search["time"])
	si1 = next((packet for packet in packets
			if packet["direction"] == "rx" and packet["type"] == 0x80
			and packet["time"] >= (ra_info["time"] if ra_info else search["time"])
			and (data := bytes.fromhex(packet["data"]))[0] == 0x50
			and data[12] == 0x19), None)
	idle_configure = next((packet for packet in packets
			if packet["direction"] == "tx" and packet["type"] == 0x02
			and packet["time"] > configure["time"]
			and (data := bytes.fromhex(packet["data"]))[8] == 0x60), None)
	idle_pch = next((packet for packet in packets
			if idle_configure is not None and packet["direction"] == "rx"
			and packet["type"] == 0x80 and packet["time"] > idle_configure["time"]
			and (data := bytes.fromhex(packet["data"]))[0] == 0x60
			and data[12] == 0x21), None)
	idle_bcch = next((packet for packet in packets
			if idle_pch is not None and packet["direction"] == "rx"
			and packet["type"] == 0x80 and packet["time"] > idle_pch["time"]
			and bytes.fromhex(packet["data"])[0] == 0x50), None)
	no_psw_found = first("rx", 0x8A, search["time"])

	if sch is None or sch["length"] != 34:
		raise SystemExit("NHM-5 active candidate window did not receive its SCH block")
	sch_data = bytes.fromhex(sch["data"])
	if sch_data[0] != 0x40 or sch_data[6:8] != b"\x00\x58":
		raise SystemExit("NHM-5 SCH did not identify requested channel 0x0058")
	if configure is None or configure["length"] != 20:
		raise SystemExit("NHM-5 SCH did not organically publish CHANNEL_CONFIGURE type 0x02/20")
	configure_data = bytes.fromhex(configure["data"])
	if configure_data[:3] != b"\x04\x12\x02" or configure_data[10:12] != b"\x00\x58":
		raise SystemExit("NHM-5 CHANNEL_CONFIGURE did not preserve SCH/BSIC/channel fields")
	if no_psw_left is None or channel_changed is None or ra_info is None:
		raise SystemExit("NHM-5 channel-change completion sequence is incomplete")
	if bcch is None or bytes.fromhex(bcch["data"])[0] != 0x50:
		raise SystemExit("NHM-5 did not reach serving BCCH reception")
	if si1 is None:
		raise SystemExit("NHM-5 serving BCCH did not carry SI1")
	si1_data = bytes.fromhex(si1["data"])[10:]
	if si1_data[8] != 0x80 or any(si1_data[index]
			for index in range(3, 19) if index != 8):
		raise SystemExit("NHM-5 SI1 does not advertise its selected ARFCN 0x0058")
	if no_psw_found is not None:
		raise SystemExit("NHM-5 successful acquisition also received NO_PSW_FOUND")
	if idle_configure is None or idle_pch is None or idle_bcch is None:
		raise SystemExit(
			"NHM-5 did not coexist idle channel-0x60 PCH with serving BCCH"
		)
	parsed_si = [
		int(match.group(1), 16)
		for match in re.finditer(
			r"nhm5_bcch_parse: channel=50 .*?06\D+([0-9a-fA-F]{2})", trace
		)
	]
	for message_type in (0x19, 0x1A, 0x1B, 0x1C):
		if message_type not in parsed_si:
			raise SystemExit(
				f"NHM-5 firmware parser did not consume SI{message_type - 0x18}"
			)
	if not re.search(
			r"nhm5_si_result: message=19 changed=01 result=00000000 "
			r"channel=50 flags=0f/00/33 status=0002 ready=00 gate=00000004",
			trace,
	):
		raise SystemExit("NHM-5 firmware did not complete its SI1-SI4 state")
	if not (
			search["time"] <= sch["time"] <= configure["time"] <=
			no_psw_left["time"] <= channel_changed["time"] <=
			ra_info["time"] <= bcch["time"]
	):
		raise SystemExit("NHM-5 acquisition transaction order changed")
	print(
		"NHM-5 acquisition: 56/160 candidate 0058 -> SCH -> organic 02/20 "
		"CHANNEL_CONFIGURE -> NO_PSW_LEFT -> CHANNEL_CHANGED_CNF -> RA_INFO -> "
		"BCCH SI1 advertising 0058; firmware accepted a complete SI1-SI4 set -> "
		"idle PCH/BCCH coexistence"
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
