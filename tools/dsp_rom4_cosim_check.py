#!/usr/bin/env python3
"""Check and summarize an external Nokia 5110 ROM4 C54x co-sim log."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys


REACH4 = re.compile(
    r"reach4: superloop=(\d+) idle=(\d+) bist=(\d+) daram=(\d+) "
    r"retpad=(\d+) other=(\d+)"
)
DSP_ACKS = re.compile(r"DSP acks=(\d+)")
PORT_ACCESS = re.compile(r"\[dsp54\] PORT([WR]) pa=0x([0-9A-Fa-f]{2})")


def parse_log(text: str) -> dict[str, object]:
    required = {
        "program_loaded": "loaded " in text and "dsp_full.bin (131070 bytes)" in text,
        "drom_loaded": "loaded DROM " in text and "(16384 words)" in text,
        "real_upload": "REALUP: DSP RELEASED" in text,
        "security_screen": "=== LCD framebuffer" in text and "budget reached" in text,
    }
    missing = [name for name, present in required.items() if not present]
    if missing:
        raise ValueError("missing reproduction markers: " + ", ".join(missing))

    match = REACH4.search(text)
    if not match:
        raise ValueError("missing reach4 execution-region summary (run with DSP54_PCHIST)")
    names = ("superloop", "idle", "bist", "daram", "retpad", "other")
    regions = dict(zip(names, (int(value) for value in match.groups())))
    total = sum(regions.values())
    if not total:
        raise ValueError("execution-region summary is empty")

    assists = {
        "resident_daram_reseed": "SEEDDARAM: reloaded" in text,
        "selftest_measurement_patch": "SELFTEST_MEAS: nominal idle measurement staged" in text,
        "cobba_bsp_loopback": "COBBA: BSP loopback" in text,
    }
    ack_matches = DSP_ACKS.findall(text)
    port_counts: dict[str, int] = {}
    for operation, address in PORT_ACCESS.findall(text):
        key = f"{operation.lower()}_0x{address.lower()}"
        port_counts[key] = port_counts.get(key, 0) + 1
    activity = {
        "dsp_acknowledgements": int(ack_matches[-1]) if ack_matches else 0,
        "ring_transitions": text.count("[dsp54] RINGFIQ"),
        "host_doorbells": text.count("[dsp54] HINT->IRQ4"),
        "codec_frame_interrupts": text.count("COBBA: codec frame RINT0"),
        "port_accesses": port_counts,
    }
    return {
        "reproduction": required,
        "executed_instructions": total,
        "regions": {
            name: {"instructions": count, "percent": round(100.0 * count / total, 4)}
            for name, count in regions.items()
        },
        "modeled_assists_observed": assists,
        "interface_activity": activity,
        "promotion_blocked": any(assists.values()),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    try:
        result = parse_log(args.log.read_text(encoding="utf-8", errors="replace"))
    except (OSError, ValueError) as error:
        print(f"ROM4 co-sim rejected: {error}", file=sys.stderr)
        return 1
    if args.json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        print(f"executed instructions classified: {result['executed_instructions']}")
        for name, values in result["regions"].items():
            print(f"  {name}: {values['instructions']} ({values['percent']:.4f}%)")
        active = [name for name, seen in result["modeled_assists_observed"].items() if seen]
        print("modeled assists observed: " + (", ".join(active) if active else "none"))
        activity = result["interface_activity"]
        print(
            "interface activity: "
            f"acks={activity['dsp_acknowledgements']} "
            f"rings={activity['ring_transitions']} "
            f"doorbells={activity['host_doorbells']} "
            f"codec_frames={activity['codec_frame_interrupts']}"
        )
        ports = activity["port_accesses"]
        if ports:
            print("  ports: " + " ".join(f"{name}={count}" for name, count in sorted(ports.items())))
        print("real-DSP promotion: " + ("blocked" if result["promotion_blocked"] else "not blocked by observed assists"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
