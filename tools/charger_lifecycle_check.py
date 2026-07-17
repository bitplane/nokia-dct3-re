#!/usr/bin/env python3
"""Check the recovered charger-present task-1 lifecycle."""

import argparse
import pathlib
import sys


def parse_summary(text):
    return dict(line.split("=", 1) for line in text.splitlines() if "=" in line)


def check(kind, values):
    errors = []
    modes = set(filter(None, values.get("startup_modes", "").split(",")))
    final_mode = values.get("final_startup_mode")

    if kind == "connected":
        if "0009" not in modes:
            errors.append("charger-present startup never entered mode 0009")
        if final_mode != "0009":
            errors.append(f"charger-present startup ended in mode {final_mode}, expected 0009")
        if values.get("final_sim_enable") != "01":
            errors.append("charger-present startup did not retain the enabled SIM lifecycle")
    else:
        for wanted in ("0009", "000C", "0005"):
            if wanted not in modes:
                errors.append(f"acting-dead lifecycle never entered mode {wanted}")
        if final_mode != "0005":
            errors.append(f"acting-dead lifecycle ended in mode {final_mode}, expected 0005")
        if values.get("final_sim_enable") != "00":
            errors.append("acting-dead lifecycle did not disable the SIM")
        if int(values.get("lcd_data_writes", "0")) == 0:
            errors.append("acting-dead lifecycle produced no LCD traffic")

    if not (int(values.get("irq_seen", "0"), 16) & 0x04):
        errors.append("charger lifecycle did not observe MAD2 IRQ2")
    return errors


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("kind", choices=("connected", "acting-dead"))
    parser.add_argument("summary", type=pathlib.Path)
    args = parser.parse_args(argv)

    errors = check(args.kind, parse_summary(args.summary.read_text()))
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print(f"charger lifecycle: {args.kind} reproduced")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
