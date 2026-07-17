#!/usr/bin/env python3
"""Validate charger-originated restart of the powered-off baseband."""

import argparse
import pathlib
import re
import sys


def parse_summary(text):
    return dict(line.split("=", 1) for line in text.splitlines() if "=" in line)


def check(log_text, values):
    errors = []
    modes = set(filter(None, values.get("startup_modes", "").split(",")))

    off = re.search(r"ccont_power: event=off t=([0-9.]+)", log_text)
    wake = re.search(r"ccont_power: event=wake cause=04 t=([0-9.]+)", log_text)
    cause_reads = [
        (int(data, 16), float(time))
        for data, time in re.findall(
            r"ccont_power: event=cause_read data=([0-9a-f]{2}) t=([0-9.]+)",
            log_text,
            re.IGNORECASE,
        )
    ]
    charger_samples = [
        (int(raw, 16), float(time))
        for raw, time in re.findall(
            r"ccont_input: adc_select=5 raw=([0-9a-f]{3}).*?t=([0-9.]+)",
            log_text,
            re.IGNORECASE,
        )
    ]

    if not off:
        errors.append("CCONT never removed baseband power")
    if not wake:
        errors.append("charger edge never restored CCONT baseband power with cause 04")
    if off and wake and float(wake.group(1)) <= float(off.group(1)):
        errors.append("charger wake did not follow the power-off transition")
    if wake:
        wake_time = float(wake.group(1))
        if not any(time >= wake_time and data & 0x04 for data, time in cause_reads):
            errors.append("firmware never read the charger power-on cause after restart")
        if not any(time >= wake_time and raw >= 0x64 for raw, time in charger_samples):
            errors.append("firmware never sampled charger-present VCHAR after restart")

    for wanted in ("0004", "000C", "0005"):
        if wanted not in modes:
            errors.append(f"wake lifecycle never observed startup mode {wanted}")
    if values.get("final_startup_mode") != "0005":
        errors.append("charger-originated restart did not settle in acting-dead mode 0005")
    if values.get("final_sim_enable") != "00":
        errors.append("acting-dead restart did not leave SIM disabled")
    if int(values.get("lcd_data_writes", "0")) == 0:
        errors.append("charger-originated restart produced no LCD traffic")
    return errors


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("summary", type=pathlib.Path)
    args = parser.parse_args(argv)
    errors = check(args.log.read_text(errors="replace"), parse_summary(args.summary.read_text()))
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print("charger wake: powered-off baseband restarted into acting-dead mode")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
