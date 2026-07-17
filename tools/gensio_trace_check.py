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

SELECT_INIT = {
    0x6F: 0x00,
    0xAD: 0xC4,
    0xAE: 0x20,
    0xAF: 0x00,
    0xED: 0x21,
    0xEE: 0x80,
    0xEF: 0x00,
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


def check_select_contract(accesses):
    errors = []
    writes = {}
    reads = set()
    for direction, offset, data in accesses:
        if offset not in SELECT_INIT:
            continue
        if direction == "W":
            writes.setdefault(offset, []).append(data)
        else:
            reads.add(offset)

    for offset, expected in SELECT_INIT.items():
        values = writes.get(offset, [])
        if expected not in values:
            errors.append(
                f"SELECT register 0x{offset:02x} never received 0x{expected:02x}"
            )
    if 0xAF not in reads:
        errors.append("SELECT2 control register 0xaf was never read back")
    if 0x6F not in reads or 0x01 not in writes.get(0x6F, []):
        errors.append("SELECT1 control register 0x6f did not perform its bit-0 update")
    return errors, {
        "select_registers": len(writes),
        "select_reads": len(reads),
    }


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


def check_charger_irq(transactions, summary_text, require_absent=True):
    errors = []
    status_seen = False
    ack_seen = False
    clear_seen = False
    bit3_unmasked = False
    adc_selector = None
    adc_low = None
    vchar_samples = []

    for direction, address, data in transactions:
        if direction == "W" and address == 0x00:
            adc_selector = (data >> 4) & 0x07
            adc_low = None
        elif direction == "R" and address == 0x02 and adc_selector == 5:
            adc_low = data
        elif (direction == "R" and address == 0x03 and adc_selector == 5
              and adc_low is not None):
            vchar_samples.append(adc_low | ((data & 0x03) << 8))
            adc_low = None
        elif direction == "W" and address == 0x0F and not (data & 0x08):
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
    irq_seen = re.search(r"^irq_seen=([0-9A-Fa-f]{2})$", summary_text, re.MULTILINE)
    final_irq = re.search(r"^final_irq_status=([0-9A-Fa-f]{2})$", summary_text, re.MULTILINE)
    if not irq_seen or not (int(irq_seen.group(1), 16) & 0x04):
        errors.append("MAD2 IRQ2 was not observed")
    if not final_irq or (int(final_irq.group(1), 16) & 0x04):
        errors.append("MAD2 IRQ2 did not deassert")
    if not status_seen:
        errors.append("firmware did not read the asserted CCONT charger source")
    if not ack_seen:
        errors.append("firmware did not acknowledge CCONT charger source bit 3")
    if not clear_seen:
        errors.append("CCONT charger source did not read clear after acknowledgement")
    if not any(sample > 0x64 for sample in vchar_samples):
        errors.append("firmware never observed charger-present VCHAR on ADC selector 5")
    if require_absent and not any(sample <= 0x64 for sample in vchar_samples):
        errors.append("firmware never observed charger-absent VCHAR on ADC selector 5")

    return errors, {
        "serial_status": int(status_seen),
        "serial_ack": int(ack_seen),
        "serial_clear_read": int(clear_seen),
        "vchar_samples": len(vchar_samples),
    }


def check_ccont_boot_status(transactions):
    status_reads = [data for direction, address, data in transactions
                    if direction == "R" and address == 0x0E]
    errors = []
    if not status_reads:
        errors.append("CCONT reset status register was not read")
    elif status_reads[0] != 0x03:
        errors.append(
            f"CCONT reset status was 0x{status_reads[0]:02x}, expected 0x03")
    return errors, {"boot_status_reads": len(status_reads)}


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--adc-profile", choices=sorted(ADC_PROFILES))
    parser.add_argument("--require-charger-irq", action="store_true")
    parser.add_argument("--charger-present-only", action="store_true")
    parser.add_argument("--summary", type=pathlib.Path)
    parser.add_argument("--require-select-contract", action="store_true")
    parser.add_argument("--require-ccont-boot-status", action="store_true")
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
            transactions, args.summary.read_text(errors="replace"),
            not args.charger_present_only,
        )
        errors.extend(irq_errors)
        print(
            "CCONT charger IRQ: "
            f"serial-status={irq_counts['serial_status']} "
            f"serial-ack={irq_counts['serial_ack']} "
            f"serial-clear-read={irq_counts['serial_clear_read']}"
        )
    if args.require_select_contract:
        select_errors, select_counts = check_select_contract(accesses)
        errors.extend(select_errors)
        print(
            "GENSIO SELECT contract: "
            f"registers={select_counts['select_registers']} "
            f"read-back={select_counts['select_reads']}"
        )
    if args.require_ccont_boot_status:
        status_errors, status_counts = check_ccont_boot_status(transactions)
        errors.extend(status_errors)
        print(f"CCONT reset status: reads={status_counts['boot_status_reads']} first=03")
    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
