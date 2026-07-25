#!/usr/bin/env python3
"""Inventory the packet vocabulary exercised by the DSP HLE boundary."""

import argparse
import collections
import json
import pathlib
import re


PACKET = re.compile(
	r"dspif_transport: (?P<direction>TX consume|RX enqueue) type=(?P<type>[0-9a-f]+) "
	r"payload=(?P<length>[0-9]+).*? data=(?P<data>[0-9a-f]*) t=(?P<time>[0-9.]+)",
	re.IGNORECASE,
)
NOTIFY = re.compile(r"dspif_transport: FIQ0 notify .* t=(?P<time>[0-9.]+)", re.IGNORECASE)


def external_fields(data: bytes) -> tuple[int | None, int | None]:
	if len(data) < 9 or data[0] != 0x1E:
		return None, None
	return data[3], data[8]


def classify(direction: str, packet_type: int, data: bytes) -> tuple[str, str]:
	if direction == "tx":
		if packet_type == 0x05:
			if len(data) >= 4 and data[0] == 0x1E and data[3] == 0xF4:
				return "external_discovery_control", "one-way discovery-side control publication"
			frame_class, command = external_fields(data)
			if frame_class == 0xD0 and len(data) >= 7 and data[6] == 0x01:
				return "external_discovery_request", "derived discovery echo/completion"
			if frame_class == 0xD0 and len(data) >= 7 and data[6] == 0x05:
				return "external_discovery_close", "one-way acknowledgement of peer state-4 completion"
			if frame_class == 0x40 and command in (0x64, 0x70):
				return "external_service_reply", "derived transport acknowledgement"
			if frame_class == 0x00 and command == 0x5F:
				return "external_poll", "derived transport acknowledgement"
			if frame_class == 0x00 and command == 0x62:
				return "service_empty_report", "one-way packet; completion uses DSP shared control"
			if frame_class == 0x7F:
				return "external_transport_ack", "consumed; no response"
			return "external_unanswered", "consumed; no modeled response"
		if packet_type == 0x70 and data == bytes((0x0D, 0x00)):
			return "service_control_request", "request-derived type-0x74 completion"
		if packet_type == 0x70 and data == bytes((0x0A, 0x09)):
			return "service_control_followup", "one-way publication after type-0x74 completion"
		if packet_type == 0x70 and len(data) == 6 and data[:2] == bytes((0x13, 0x04)):
			return "bootstrap_platform_word", "one-way DSP bootstrap publication"
		if packet_type == 0x70 and len(data) in (14, 22, 26) and data[0] in (0x14, 0x15, 0x16):
			return "bootstrap_table", "one-way DSP bootstrap publication"
		if packet_type == 0x70:
			return "control_publication", "consumed; no modeled response"
		if packet_type == 0x51:
			return "segmented_dsp_memory_upload", "one-way command-0x22 DSP memory image"
		if packet_type == 0x1A:
			return "search_list", "asynchronous GSM channel search command"
		if packet_type == 0x14 and len(data) == 12:
			return "cipher_control", "one-way DSP cipher-control publication"
		if packet_type == 0x0D and len(data) == 66:
			return "indexed_64_byte_block_upload", "one-way DSP configuration publication"
		if packet_type == 0x3C and len(data) == 156:
			return "selector_lookup_table_upload", "one-way DSP configuration publication"
		return "unmodeled_dsp_uplink", "consumed; no modeled response"

	if packet_type == 0x74 and data == bytes((0x0D, 0x00)):
		return "service_control_completion", "derived from type-0x70/0d00"
	if packet_type == 0x8E:
		frame_class, command = external_fields(data)
		if frame_class == 0xD0 and len(data) >= 7:
			return ("external_discovery_echo" if data[6] == 0x01 else "external_discovery_completion",
					"derived from discovery request")
		if frame_class == 0x7F:
			return "external_transport_ack", "derived from MCU external frame"
		if frame_class == 0x40 and command == 0x64:
			return "external_registration", "peer-initiated canned service frame"
		if frame_class == 0x40 and command == 0x70:
			return "external_channel_map", "peer-initiated canned channel map"
	return "unclassified_inbound", "unknown"


def parse(label: str, path: pathlib.Path) -> list[dict]:
	packets = []
	for line in path.read_text(errors="replace").splitlines():
		match = PACKET.search(line)
		if match:
			data = bytes.fromhex(match.group("data"))
			length = int(match.group("length"))
			if len(data) != length:
				raise ValueError(f"{path}: payload length mismatch")
			direction = "tx" if match.group("direction").lower().startswith("tx") else "rx"
			packet_type = int(match.group("type"), 16)
			semantic, disposition = classify(direction, packet_type, data)
			packets.append({
				"profile": label, "direction": direction, "type": packet_type,
				"length": length, "data": data.hex(), "time": float(match.group("time")),
				"semantic": semantic, "disposition": disposition, "notified": False,
			})
			continue
		match = NOTIFY.search(line)
		if match:
			notify_time = float(match.group("time"))
			for packet in reversed(packets):
				if packet["direction"] == "rx" and abs(packet["time"] - notify_time) < 0.000002:
					packet["notified"] = True
					break
	return packets


def render_markdown(items: list[dict]) -> str:
	profiles = sorted({item["profile"] for item in items})
	lines = [
		"# DSP packet-semantics census",
		"",
		f"Profiles: {', '.join(f'`{profile}`' for profile in profiles)}. "
		f"Observed packets: **{len(items)}**.",
		"",
		"This report classifies behavior implemented at the current HLE boundary. "
		"A packet marked unmodeled is valid firmware traffic that the HLE presently discards, not evidence that real DSP hardware ignores it.",
		"Types `0x0d` and `0x3c` are named for their recovered wire structure only: the ROM-4 DSP consumer and physical purpose remain unidentified.",
		"",
		"| direction | type | semantic family | disposition | v5.01 | v6.00 | lengths | payload prefix examples |",
		"| --- | ---: | --- | --- | ---: | ---: | --- | --- |",
	]
	groups = collections.defaultdict(list)
	for item in items:
		groups[(item["direction"], item["type"], item["semantic"], item["disposition"])].append(item)
	for key, group in sorted(groups.items()):
		direction, packet_type, semantic, disposition = key
		counts = collections.Counter(item["profile"] for item in group)
		lengths = ", ".join(str(value) for value in sorted({item["length"] for item in group}))
		examples = sorted({item["data"][:32] for item in group})[:3]
		lines.append(
			f"| {direction.upper()} | `0x{packet_type:02x}` | {semantic} | {disposition} | "
			f"{counts.get('v501', 0)} | {counts.get('v600', 0)} | {lengths} | "
			f"{', '.join(f'`{value}`' for value in examples)} |"
		)

	for profile in profiles:
		tx = [item for item in items if item["profile"] == profile and item["direction"] == "tx"]
		rx = [item for item in items if item["profile"] == profile and item["direction"] == "rx"]
		unmodeled = [item for item in tx if item["disposition"] == "consumed; no modeled response"]
		unmodeled_text = ("1 outbound packet is" if len(unmodeled) == 1 else
				f"{len(unmodeled)} outbound packets are")
		lines.extend([
			"",
			f"- `{profile}`: {len(tx)} TX, {len(rx)} RX, {sum(item['notified'] for item in rx)} RX notifications; "
			f"{unmodeled_text} consumed without modeled semantics.",
		])
	return "\n".join(lines) + "\n"


def main() -> None:
	parser = argparse.ArgumentParser()
	parser.add_argument("input", nargs="+", help="PROFILE=LOG")
	parser.add_argument("--json", type=pathlib.Path)
	parser.add_argument("--report", type=pathlib.Path)
	parser.add_argument("--check", action="store_true")
	args = parser.parse_args()
	items = []
	for value in args.input:
		label, separator, filename = value.partition("=")
		if not separator:
			parser.error(f"input must be PROFILE=LOG: {value}")
		items.extend(parse(label, pathlib.Path(filename)))
	if args.check:
		for profile in {item["profile"] for item in items}:
			profile_items = [item for item in items if item["profile"] == profile]
			if not any(item["direction"] == "tx" for item in profile_items) or not any(item["direction"] == "rx" for item in profile_items):
				raise SystemExit(f"{profile}: missing packet direction")
	result = {"schema": 1, "records": len(items), "packets": items}
	if args.json:
		args.json.parent.mkdir(parents=True, exist_ok=True)
		args.json.write_text(json.dumps(result, indent=2) + "\n")
	if args.report:
		args.report.parent.mkdir(parents=True, exist_ok=True)
		args.report.write_text(render_markdown(items))
	print(f"DSP packet census: {len(items)} packets")


if __name__ == "__main__":
	main()
