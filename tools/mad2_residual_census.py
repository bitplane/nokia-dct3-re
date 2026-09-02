#!/usr/bin/env python3
"""Census the unresolved MAD2 CTSI surfaces across supported DCT3 ROMs."""

import argparse
import json
from pathlib import Path

try:
	from tools.board_io_static_census import swap16
	from tools.mad2_static_census import analyze_image
except ModuleNotFoundError:  # Direct execution from tools/.
	from board_io_static_census import swap16
	from mad2_static_census import analyze_image


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_ROMS = (
	("3210-v6.00", ROOT / "roms/noki3210/3210f600a.fls"),
	("3210-v5.01", ROOT / "roms/noki3210/3210f501.fls"),
	("3310-v6.39", ROOT / "roms/noki3310/3310f639e.fls"),
	("3330-v4.50", ROOT / "roms/noki3330/3330f450e.fls"),
	("3410-v5.46", ROOT / "roms/noki3410/3410f546e.fls"),
)
REVIEWED_OFFSETS = (0x01, 0x02, 0x03, 0x0d, 0x0e, 0x16)


def summarize(label, path, accesses, coverage):
	counts = {}
	pcs = {}
	for offset in REVIEWED_OFFSETS:
		for kind in ("read", "write"):
			hits = [item for item in accesses
				if item["offset"] == offset and item["kind"] == kind]
			counts[f"{offset:02x}_{kind}"] = len(hits)
			pcs[f"{offset:02x}_{kind}"] = [f"0x{item['pc']:08x}" for item in hits]
	return {
		"label": label,
		"rom": str(path.relative_to(ROOT)),
		"coverage": coverage,
		"counts": counts,
		"pcs": pcs,
	}


def markdown(reports):
	lines = [
		"# MAD2 residual census", "",
		"Conservative literal-seeded Thumb analysis of the remaining CTSI surfaces.",
		"The 32-instruction seed span deliberately excludes distant pointer guesses;",
		"dynamic and table-driven accesses remain outside coverage.", "",
		"| ROM | reset R/W | DSP reset R/W | watchdog R/W | clock R/W | ext status R/W | FIQ8 R/W |",
		"| --- | ---: | ---: | ---: | ---: | ---: | ---: |",
	]
	for report in reports:
		c = report["counts"]
		pair = lambda offset: f"{c[f'{offset:02x}_read']}/{c[f'{offset:02x}_write']}"
		lines.append(f"| {report['label']} | {pair(1)} | {pair(2)} | {pair(3)} | "
			f"{pair(0x0d)} | {pair(0x0e)} | {pair(0x16)} |")
	lines += ["", "## Conclusions", "",
		"- The watchdog register is write-only in all five images (three resolved writes each).",
		"- The external-status bank is read-only in all five images (five or six resolved reads, zero writes).",
		"- Every image has ten clock-control reads and ten matching writes. The paired 3210 decode",
		"  assigns boot bits 2-3, SIMI bits 5-6 and the one-shot ARM-stop bit 1; the census finds",
		"  no additional product branch that can identify the remaining physical consumers.",
		"- Every image contains the FIQ8 control family. Firmware semantics identify it as the",
		"  100 Hz centisecond source, but static code cannot establish oscillator behavior during sleep.",
		"- No emulated component asserts ninth IRQ line 8. Firmware proves its status/acknowledge",
		"  register contract, but its physical owner is not recoverable from these access sites.", "",
		"The unresolved items now require a MAD2 data sheet, physical timing/logic capture, or an",
		"organic product lifecycle that exercises the relevant input. They are not safe targets",
		"for calibrated implementation.", ""]
	return "\n".join(lines)


def validate(reports):
	if {report["label"] for report in reports} != {label for label, _ in DEFAULT_ROMS}:
		raise ValueError("residual census requires all five supported ROMs")
	for report in reports:
		c = report["counts"]
		if (c["03_read"], c["03_write"]) != (0, 3):
			raise ValueError(f"{report['label']} watchdog access shape changed")
		if c["0e_read"] < 5 or c["0e_write"] != 0:
			raise ValueError(f"{report['label']} external-status access shape changed")
		if (c["0d_read"], c["0d_write"]) != (10, 10):
			raise ValueError(f"{report['label']} clock-control access shape changed")
		if c["16_read"] == 0 or c["16_write"] == 0:
			raise ValueError(f"{report['label']} FIQ8 control family disappeared")


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("--json", type=Path)
	parser.add_argument("--markdown", type=Path)
	parser.add_argument("--check", action="store_true")
	args = parser.parse_args()
	reports = []
	for label, path in DEFAULT_ROMS:
		accesses, coverage = analyze_image(swap16(path.read_bytes()), max_seed_span=32)
		reports.append(summarize(label, path, accesses, coverage))
	if args.check:
		validate(reports)
	payload = {"schema": 1, "roms": reports}
	if args.json:
		args.json.write_text(json.dumps(payload, indent=2) + "\n")
	if args.markdown:
		args.markdown.write_text(markdown(reports))
	if not args.json and not args.markdown:
		print(markdown(reports), end="")


if __name__ == "__main__":
	main()
