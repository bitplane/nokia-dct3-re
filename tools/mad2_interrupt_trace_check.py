#!/usr/bin/env python3
"""Validate focused MAD2 interrupt aggregation and routing traces."""

import argparse
import pathlib
import re
import sys


LINE_RE = re.compile(r"mad2_interrupt: (?P<body>.*)")
FIELD_RE = re.compile(r"(?P<key>[a-z_]+)=(?P<value>[A-Za-z0-9_.]+)")
HEX_FIELDS = {
    "pending", "pending_before", "pending_after", "mask", "fiq", "irq",
    "fiqmask", "irqmask", "ctrl", "extctrl", "off", "data",
}
DECIMAL_FIELDS = {"line", "active", "keypad", "ccont"}


def parse(text):
    events = []
    for line in text.splitlines():
        match = LINE_RE.search(line)
        if not match:
            continue
        event = {}
        for field in FIELD_RE.finditer(match.group("body")):
            key, value = field.group("key"), field.group("value")
            if key in HEX_FIELDS:
                event[key] = int(value, 16)
            elif key in DECIMAL_FIELDS:
                event[key] = int(value, 10)
            elif key == "t":
                event[key] = float(value)
            else:
                event[key] = value
        events.append(event)
    return events


def _summary_value(summary_text, name):
    match = re.search(rf"^{re.escape(name)}=([0-9A-Fa-f]+)$", summary_text, re.MULTILINE)
    return int(match.group(1), 16) if match else None


def check_overlap(events, summary_text):
    errors = []
    overlap = [e for e in events if
               e.get("pending_after", e.get("pending", 0)) & 0x41 == 0x41]
    delivered = [e for e in events if e.get("event") == "route" and e.get("domain") == "IRQ" and e.get("active") == 1 and e.get("pending", 0) & 0x41 == 0x41]
    independent_ack = [e for e in events if e.get("event") == "ack" and e.get("domain") == "IRQ" and e.get("mask", 0) & 0x01 and e.get("pending_before", 0) & 0x41 == 0x41 and e.get("pending_after", 0) & 0x40]
    final_irq = _summary_value(summary_text, "final_irq_status")

    if not overlap:
        errors.append("keypad IRQ0 and CCONT IRQ6 were never pending simultaneously")
    if not delivered:
        errors.append("simultaneous IRQ0/IRQ6 pending state was not routed to the CPU")
    if not independent_ack:
        errors.append("acknowledging IRQ0 did not preserve pending CCONT IRQ6")
    if final_irq is None:
        errors.append("boot summary does not record final IRQ status")
    elif final_irq & 0x41:
        errors.append("physical IRQ0/IRQ6 sources did not settle")
    return errors, {"overlap": len(overlap), "independent_acks": len(independent_ack)}


def check_mask(events):
    errors = []
    mask_index = next((i for i, e in enumerate(events) if e.get("event") == "reg_W" and e.get("off") == 0x0B and e.get("data", 0) & 0x01), None)
    pending_index = next((i for i, e in enumerate(events) if e.get("event") == "levels" and e.get("pending_after", 0) & 0x01), None)
    delivered_index = next((i for i, e in enumerate(events) if e.get("event") == "route" and e.get("domain") == "IRQ" and e.get("active") == 1 and e.get("pending", 0) & 0x01), None)
    cleared = [e for e in events if e.get("event") == "ack" and e.get("domain") == "IRQ" and e.get("mask", 0) & 0x01 and e.get("pending_before", 0) & 0x01 and not (e.get("pending_after", 0) & 0x01)]

    if mask_index is None:
        errors.append("IRQ0 was not masked")
    if pending_index is None:
        errors.append("IRQ0 did not become pending")
    if delivered_index is None:
        errors.append("pending IRQ0 was not delivered after unmask/global enable")
    if not cleared:
        errors.append("IRQ0 write-one-clear acknowledgement was not observed")
    if None not in (mask_index, pending_index, delivered_index) and not (mask_index < pending_index < delivered_index):
        errors.append("IRQ0 mask/pending/delivery events occurred out of order")
    return errors, {"clearing_acks": len(cleared)}


def check_fiq8(events):
    errors = []
    assertions = [e for e in events if e.get("event") == "assert" and e.get("domain") == "FIQ" and e.get("line") == 8 and e.get("pending_after", 0) & 0x100]
    projected = [e for e in events if e.get("event") == "reg_R" and e.get("off") == 0x16 and e.get("data", 0) & 0x02 and e.get("fiq", 0) & 0x100]
    delivered = [e for e in events if e.get("event") == "route" and e.get("domain") == "FIQ" and e.get("active") == 1 and e.get("pending") == 0x100 and e.get("extctrl") == 0x01]
    gated = [e for e in events if e.get("event") == "route" and e.get("domain") == "FIQ" and e.get("active") == 0 and e.get("pending") == 0x100]
    cleared = [e for e in events if e.get("event") == "ack" and e.get("domain") == "FIQ" and e.get("mask", 0) & 0x100 and e.get("pending_before", 0) & 0x100 and not (e.get("pending_after", 0) & 0x100)]

    if not assertions:
        errors.append("FIQ8 did not assert the extended pending bit")
    if not projected:
        errors.append("extended pending was not projected through register 0x16 bit 1")
    if not delivered:
        errors.append("unmasked extended FIQ was not routed to the CPU")
    if not gated:
        errors.append("extended FIQ did not remain pending when CPU delivery was gated")
    if not cleared:
        errors.append("extended FIQ write-one-clear acknowledgement was not observed")
    return errors, {"assertions": len(assertions), "projected_reads": len(projected), "clearing_acks": len(cleared)}


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("overlap", "mask", "fiq8"))
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--summary", type=pathlib.Path)
    args = parser.parse_args(argv)

    events = parse(args.log.read_text(errors="replace"))
    if args.mode == "overlap":
        if not args.summary:
            parser.error("overlap mode requires --summary")
        errors, counts = check_overlap(events, args.summary.read_text(errors="replace"))
    elif args.mode == "mask":
        errors, counts = check_mask(events)
    else:
        errors, counts = check_fiq8(events)
    print(f"MAD2 interrupt contract ({args.mode}): {counts}")
    for error in errors:
        print(f"error: {error}", file=sys.stderr)
    return int(bool(errors))


if __name__ == "__main__":
    raise SystemExit(main())
