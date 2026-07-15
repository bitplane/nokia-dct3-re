#!/usr/bin/env python3
"""Validate the observed MAD2 GENSIO/CCONT transaction grammar."""

import argparse
import pathlib
import re
import sys


ACCESS_RE = re.compile(
    r"gensio: ([RW]) off=([0-9a-fA-F]{2}) data=([0-9a-fA-F]{2})"
)

ADC_PROFILES = {
    "sane": (0x000, 0x200, 0x2D0, 0x280, 0x200, 0x000, 0x200, 0x000),
}


def parse_accesses(text):
    return [
        (direction, int(offset, 16), int(data, 16))
        for direction, offset, data in ACCESS_RE.findall(text)
    ]


def decode_transactions(accesses):
    errors = []
    selected = None
    ccont_phase = None
    pending_read = False
    read_ready_seen = False
    read_count = 0
    write_count = 0
    transactions = []
    command = None

    for direction, offset, data in accesses:
        if direction == "W" and offset == 0x2D:
            selected = data
            if data == 0x25:
                ccont_phase = "command"
                pending_read = False
                read_ready_seen = False
                command = None
            continue

        if selected != 0x25:
            continue

        if direction == "R" and offset == 0x6D:
            if pending_read and data == 0x07:
                read_ready_seen = True
            continue

        if direction == "W" and offset == 0x2C:
            if ccont_phase == "command":
                command = data
                pending_read = bool(data & 0x04)
                read_ready_seen = False
                ccont_phase = "data"
            elif ccont_phase == "data":
                if pending_read:
                    errors.append("CCONT read command was followed by a data write")
                if command is not None:
                    transactions.append(("W", (command >> 3) & 0x0F, data))
                write_count += 1
                ccont_phase = "command"
            else:
                errors.append("CCONT byte observed without endpoint selection")
            continue

        if direction == "R" and offset == 0x6C:
            if ccont_phase != "data" or not pending_read:
                errors.append("CCONT data read without a pending read command")
            if not read_ready_seen:
                errors.append("CCONT data consumed before GENSIO status 0x07")
            if command is not None:
                transactions.append(("R", (command >> 3) & 0x0F, data))
            read_count += 1
            pending_read = False
            read_ready_seen = False
            ccont_phase = "command"

    if not accesses:
        errors.append("no GENSIO trace records found")
    if read_count == 0:
        errors.append("no complete CCONT read transactions found")
    if write_count == 0:
        errors.append("no complete CCONT write transactions found")

    return (
        errors,
        {"accesses": len(accesses), "reads": read_count, "writes": write_count},
        transactions,
    )


def check_accesses(accesses):
    errors, counts, _ = decode_transactions(accesses)
    return errors, counts


def check_adc(transactions, values):
    errors = []
    selector = None
    observed = set()
    sample_reads = 0

    for direction, address, data in transactions:
        if direction == "W" and address == 0:
            selector = (data >> 4) & 0x07
            observed.add(selector)
        elif direction == "R" and address in (2, 3) and selector is not None:
            expected = values[selector]
            if address == 2:
                wanted = expected & 0xFF
            else:
                wanted = 0xB0 | ((expected >> 8) & 0x03)
            if data != wanted:
                errors.append(
                    f"ADC selector {selector} register {address} returned "
                    f"0x{data:02x}, expected 0x{wanted:02x}"
                )
            sample_reads += 1

    missing = sorted(set(range(len(values))) - observed)
    if missing:
        errors.append("ADC selectors not exercised: " + ", ".join(map(str, missing)))
    if sample_reads == 0:
        errors.append("no ADC result reads found")
    return errors, {"adc_reads": sample_reads, "adc_selectors": len(observed)}


def check_charger_irq(transactions, summary_text):
    errors = []
    status_seen = False
    ack_seen = False
    clear_seen = False
    bit3_unmasked = False

    for direction, address, data in transactions:
        if direction == "W" and address == 0x0F and not (data & 0x08):
            bit3_unmasked = True
        elif direction == "R" and address == 0x0E:
            if data & 0x08:
                status_seen = True
            elif ack_seen:
                clear_seen = True
        elif direction == "W" and address == 0x0E and status_seen and (data & 0x08):
            ack_seen = True

    if not bit3_unmasked:
        errors.append("CCONT charger source was never observed unmasked")
    if not status_seen:
        errors.append("CCONT status bit 3 was never observed")
    if not ack_seen:
        errors.append("CCONT status bit 3 was not acknowledged write-one-clear")

    irq_seen = re.search(r"^irq_seen=([0-9A-Fa-f]{2})$", summary_text, re.MULTILINE)
    final_irq = re.search(r"^final_irq_status=([0-9A-Fa-f]{2})$", summary_text, re.MULTILINE)
    if not irq_seen or not (int(irq_seen.group(1), 16) & 0x40):
        errors.append("MAD2 IRQ6 was not observed")
    if not final_irq or (int(final_irq.group(1), 16) & 0x40):
        errors.append("MAD2 IRQ6 did not deassert")

    return errors, {
        "charger_status": int(status_seen),
        "charger_ack": int(ack_seen),
        "charger_clear_read": int(clear_seen),
    }


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--adc-profile", choices=sorted(ADC_PROFILES))
    parser.add_argument("--require-charger-irq", action="store_true")
    parser.add_argument("--summary", type=pathlib.Path)
    args = parser.parse_args(argv)

    accesses = parse_accesses(args.log.read_text(errors="replace"))
    errors, counts, transactions = decode_transactions(accesses)
    print(
        "GENSIO contract: "
        f"{counts['accesses']} accesses, {counts['reads']} CCONT reads, "
        f"{counts['writes']} CCONT writes"
    )
    if args.adc_profile:
        adc_errors, adc_counts = check_adc(transactions, ADC_PROFILES[args.adc_profile])
        errors.extend(adc_errors)
        print(
            f"ADC contract: {adc_counts['adc_reads']} result reads, "
            f"{adc_counts['adc_selectors']} selectors"
        )
    if args.require_charger_irq:
        if not args.summary:
            parser.error("--require-charger-irq requires --summary")
        irq_errors, irq_counts = check_charger_irq(
            transactions, args.summary.read_text(errors="replace")
        )
        errors.extend(irq_errors)
        print(
            "CCONT charger IRQ: "
            f"status={irq_counts['charger_status']} ack={irq_counts['charger_ack']} "
            f"clear-read={irq_counts['charger_clear_read']}"
        )
    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
