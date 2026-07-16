#!/usr/bin/env python3
"""Correlate DSP-owned shared-RAM transitions with MCU triggers and consumers."""

import argparse
import json
import pathlib
import re


PEER_WRITE = re.compile(
	r"peer RAM W off=(?P<offset>[0-9a-f]+)(?: old=(?P<old>[0-9a-f]+))? data=(?P<data>[0-9a-f]+) t=(?P<time>[0-9.]+)",
	re.IGNORECASE,
)
MCU_WRITE = re.compile(
	r"(?<!peer )RAM W off=(?P<offset>[0-9a-f]+) data=(?P<data>[0-9a-f]+) t=(?P<time>[0-9.]+)",
	re.IGNORECASE,
)
READ = re.compile(
	r"dsp_shared_(?:read|observe): off=(?P<offset>[0-9a-f]+) data=(?P<data>[0-9a-f]+) "
	r"pc=(?P<pc>[0-9a-f]+) t=(?P<time>[0-9.]+)",
	re.IGNORECASE,
)
DOORBELL = re.compile(
	r"doorbell command=(?P<command>[0-9a-f]+) pending=(?P<pending>[0-9a-f]+) t=(?P<time>[0-9.]+)",
	re.IGNORECASE,
)


def transition_role(offset: int, time: float) -> tuple[str, str]:
	if offset <= 0x004:
		return "bootstrap_ready", "64 bootstrap exchanges complete"
	if offset in (0x0FE, 0x100):
		return "bootstrap_ack", "MCU zero-write request"
	if offset == 0x0E0:
		return "shared_control_busy", "reset publication" if time < 0.001 else "DSPIF command-4 doorbell"
	if offset == 0x0A6:
		return "tx_consumer", "peer consumes committed TX packet"
	if offset == 0x0E4:
		return "service_counter", "peer completes pending service work"
	if offset == 0x1C8:
		return "rx_producer", "peer enqueues RX packet"
	return "peer_scalar", "unclassified peer action"


def parse(label: str, path: pathlib.Path) -> list[dict]:
	events = []
	for sequence, line in enumerate(path.read_text(errors="replace").splitlines()):
		for kind, pattern in (("peer_write", PEER_WRITE), ("mcu_write", MCU_WRITE),
				("read", READ), ("doorbell", DOORBELL)):
			match = pattern.search(line)
			if match:
				event = {"kind": kind, "sequence": sequence,
						"time": float(match.group("time"))}
				for field in ("offset", "old", "data", "pc", "command", "pending"):
					if field in match.groupdict() and match.group(field) is not None:
						event[field] = int(match.group(field), 16)
				events.append(event)
				break

	transitions = []
	for index, event in enumerate(events):
		if event["kind"] != "peer_write":
			continue
		role, trigger_class = transition_role(event["offset"], event["time"])
		trigger = None
		for prior in reversed(events[:index]):
			if event["time"] - prior["time"] > 5.0:
				break
			if role == "shared_control_busy" and prior["kind"] == "doorbell":
				trigger = prior
				break
			if role == "bootstrap_ack" and prior["kind"] == "mcu_write" and prior.get("offset") == event["offset"]:
				trigger = prior
				break
			if role == "bootstrap_ready" and prior["kind"] == "mcu_write" and prior.get("offset") == 0x100:
				trigger = prior
				break
			if role == "tx_consumer" and prior["kind"] == "mcu_write" and prior.get("offset") == 0x0A4:
				trigger = prior
				break
			if role == "service_counter" and prior["kind"] == "mcu_write" and prior.get("offset") == 0x0E4:
				trigger = prior
				break
			if role == "rx_producer" and prior["kind"] in ("mcu_write", "doorbell"):
				trigger = prior
				break

		consumer = None
		for later in events[index + 1:]:
			if later["kind"] == "peer_write" and later.get("offset") == event["offset"]:
				break
			if (later["kind"] == "read" and later.get("offset") == event["offset"] and
					later.get("data") == event["data"]):
				consumer = later
				break

		transitions.append({
			"profile": label,
			"offset": event["offset"],
			"old": event.get("old"),
			"data": event["data"],
			"time": event["time"],
			"role": role,
			"trigger_class": trigger_class,
			"trigger": None if trigger is None else {
				"kind": trigger["kind"], "time": trigger["time"],
				**{key: trigger[key] for key in ("offset", "data", "command", "pending") if key in trigger},
			},
			"consumer": None if consumer is None else {
				"pc": consumer["pc"], "time": consumer["time"],
				"latency": consumer["time"] - event["time"],
			},
		})
	return transitions


def render_markdown(items: list[dict]) -> str:
	profiles = sorted({item["profile"] for item in items})
	offsets = sorted({item["offset"] for item in items})
	changed = [item for item in items if item["old"] is None or item["old"] != item["data"]]
	consumed = [item for item in changed if item["consumer"]]
	lines = [
		"# DSP-owned shared-memory transition census",
		"",
		f"Profiles: {', '.join(f'`{profile}`' for profile in profiles)}. "
		f"Peer-written scalar offsets: **{len(offsets)}**.",
		"",
		f"The traces contain **{len(items)}** peer writes, of which **{len(changed)}** change the stored value. "
		f"Firmware subsequently observes **{len(consumed)}** changed values before the next peer write to that word.",
		"",
		"This is a reachable-runtime transaction inventory, not proof that dormant DSP paths use no additional words. "
		"A missing consumer means only that the traced firmware did not subsequently read that changed value.",
		"",
		"| profile | offset | writes / changes | peer role | evidenced trigger | values written | observing PCs |",
		"| --- | ---: | ---: | --- | --- | --- | --- |",
	]
	for profile in profiles:
		for offset in offsets:
			group = [item for item in items if item["profile"] == profile and item["offset"] == offset]
			if not group:
				continue
			group_changed = [item for item in group if item["old"] is None or item["old"] != item["data"]]
			values = sorted({item["data"] for item in group})
			consumer_pcs = sorted({item["consumer"]["pc"] for item in group_changed if item["consumer"]})
			consumer_text = ", ".join(f"`0x{pc:08x}`" for pc in consumer_pcs) or "none observed"
			value_text = ", ".join(f"`0x{value:04x}`" for value in values)
			item = group[0]
			trigger_text = "; ".join(sorted({value["trigger_class"] for value in group}))
			lines.append(
				f"| {profile} | `0x{offset:03x}` | {len(group)} / {len(group_changed)} | "
				f"{item['role']} | {trigger_text} | {value_text} | {consumer_text} |"
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
		raise SystemExit("DSP shared-transition census is empty")
	changed = [item for item in items if item["old"] is None or item["old"] != item["data"]]
	result = {"schema": 1, "peer_writes": len(items), "records": len(changed), "transitions": changed}
	if args.json:
		args.json.parent.mkdir(parents=True, exist_ok=True)
		args.json.write_text(json.dumps(result, indent=2) + "\n")
	if args.report:
		args.report.parent.mkdir(parents=True, exist_ok=True)
		args.report.write_text(render_markdown(items))
	print(f"DSP shared-transition census: {len(items)} peer writes, "
			f"{len({item['offset'] for item in items})} scalar offsets")


if __name__ == "__main__":
	main()
