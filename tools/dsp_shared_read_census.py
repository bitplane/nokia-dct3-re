#!/usr/bin/env python3
"""Inventory first firmware reads from the DSP shared-memory window."""

import argparse
import json
import pathlib
import re


LINE = re.compile(
	r"dsp_shared_read: off=(?P<offset>[0-9a-f]+) data=(?P<data>[0-9a-f]+) "
	r"pc=(?P<pc>[0-9a-f]+) t=(?P<time>[0-9.]+)",
	re.IGNORECASE,
)


def classify(offset: int, data: int | None = None) -> tuple[str, str]:
	if offset <= 0x004 and data == 1:
		return "bootstrap_ready", "DSP peer"
	if offset <= 0x024:
		return "shared_ram_self_test", "MCU echo"
	if offset == 0x0A4:
		return "tx_producer", "MCU"
	if offset == 0x0A6:
		return "tx_consumer", "DSP peer"
	if offset == 0x0A8:
		return "shared_control", "MCU"
	if offset == 0x0DC:
		return "shared_control_request", "MCU request / DSP completion"
	if offset in (0x0DA, 0x0E2, 0x0E4):
		return "service_counter", "DSP peer"
	if offset == 0x0E0:
		return "shared_control_busy", "DSP peer"
	if offset == 0x0FE:
		return "bootstrap_ack", "DSP peer"
	if offset == 0x100:
		return "bootstrap_ack_or_rx_ring_base", "DSP peer"
	if 0x100 <= offset <= 0x1C6:
		return "rx_ring_payload", "DSP peer"
	if offset == 0x1C8:
		return "rx_producer", "DSP peer"
	if offset == 0x1CA:
		return "rx_consumer", "MCU"
	if 0x200 <= offset < 0x600:
		return "staged_parameter_table", "MCU"
	if offset >= 0xE00:
		return "staged_dsp_blob", "MCU"
	return "unclassified", "unknown"


def parse(label: str, path: pathlib.Path) -> list[dict]:
	items = []
	for match in LINE.finditer(path.read_text(errors="replace")):
		offset = int(match.group("offset"), 16)
		data = int(match.group("data"), 16)
		role, owner = classify(offset, data)
		items.append({
			"profile": label,
			"offset": offset,
			"data": data,
			"pc": int(match.group("pc"), 16),
			"time": float(match.group("time")),
			"role": role,
			"owner": owner,
		})
	return items


def render_markdown(items: list[dict]) -> str:
	profiles = sorted({item["profile"] for item in items})
	offsets = sorted({item["offset"] for item in items})
	role_counts = {}
	for item in items:
		role_counts[item["role"]] = role_counts.get(item["role"], 0) + 1
	profile_offsets = {profile: {item["offset"] for item in items if item["profile"] == profile}
		for profile in profiles}
	shared_offsets = set.intersection(*profile_offsets.values()) if profile_offsets else set()
	lines = [
		"# DSP shared-memory firmware-read census",
		"",
		f"Profiles: {', '.join(f'`{profile}`' for profile in profiles)}. "
		f"Unique byte offsets: **{len(offsets)}**.",
		"",
		"This is a reachable-runtime inventory of first MCU reads, not a static proof that other readers do not exist.",
		"",
		f"Both profiles share **{len(shared_offsets)}** offsets; per-profile counts are " +
		", ".join(f"`{profile}`={len(profile_offsets[profile])}" for profile in profiles) + ".",
		"",
		"Observed classifications: " + ", ".join(
			f"`{role}`={count}" for role, count in sorted(role_counts.items())) + ".",
		"",
		"No firmware read is answered outside DSPIF-owned backing RAM. Peer-owned scalar state is limited to bootstrap flags, shared-control counters/requests, ring indices, and RX packet contents.",
		"",
		"The companion [transition census](dsp_shared_memory_transitions.md) correlates the active peer-owned scalar writes with their MCU triggers and observing firmware PCs.",
		"",
		"| profile | offset | value | first PC | time (s) | classification | owner |",
		"| --- | ---: | ---: | ---: | ---: | --- | --- |",
	]
	for item in sorted(items, key=lambda value: (value["offset"], value["profile"])):
		lines.append(
			f"| {item['profile']} | `0x{item['offset']:03x}` | `0x{item['data']:04x}` | "
			f"`0x{item['pc']:08x}` | {item['time']:.6f} | {item['role']} | {item['owner']} |"
		)
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
	if args.check and not items:
		raise SystemExit("DSP shared-read census is empty")
	summary = {"schema": 1, "records": len(items), "accesses": items}
	if args.json:
		args.json.parent.mkdir(parents=True, exist_ok=True)
		args.json.write_text(json.dumps(summary, indent=2) + "\n")
	if args.report:
		args.report.parent.mkdir(parents=True, exist_ok=True)
		args.report.write_text(render_markdown(items))
	print(f"DSP shared-read census: {len(items)} first reads, "
		f"{len({item['offset'] for item in items})} unique offsets")


if __name__ == "__main__":
	main()
