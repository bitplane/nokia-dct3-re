#!/usr/bin/env python3
"""Isolated native C54x conformance run; intentional completion exits with code 3."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

REQUIRED = [
    "TMS320C54x core conformance: PASS",
    "TMS320C54x ASM accumulator add conformance: PASS left_shift=1 overflow_variants=4",
    "TMS320C54x ASM accumulator subtract conformance: PASS overflow_variants=4 source_preserved=1",
    "TMS320C54x ASM arithmetic shifts: PASS variants=128 shifts=32 sxm_settings=2",
    "TMS320C54x shifted high store conformance: PASS variants=128",
    "TMS320C54x XF output: PASS status_variants=8 debugger=1 restore=1",
    "TMS320C54x stack address latency conformance: PASS",
    "TMS320C54x RC ALT conformance: PASS",
    "TMS320C54x software interrupt conformance: PASS",
    "TMS320C54x software interrupt fast return: PASS variants=64",
    "TMS320C54x immediate repeat conformance: PASS variants=256",
    "TMS320C54x short immediate load conformance: PASS variants=512 sxm_settings=2",
    "TMS320C54x rounded multiply boundaries: PASS vectors=22 destinations=2 addressing_modes=2 sticky_overflow_states=2",
    "TMS320C54x NMI idle wake: PASS cases=12 cycle_accuracy_claim=0",
    "TMS320C54x NMI pending restore: PASS cases=12 held_line_cases=12 timing_claim=0",
    "TMS320C54x control disassembler: PASS vectors=47 pages=2"
]
COMPLETION = "Fatal error: TMS320C54x core tests complete"


def check_result(text, returncode):
    if returncode != 3:
        raise ValueError(f"unexpected MAME exit status {returncode}; expected test completion exit 3")
    lines = text.splitlines()
    for marker in REQUIRED:
        if sum(line == marker or line.startswith(marker + " ") for line in lines) != 1:
            raise ValueError(f"missing or duplicated conformance marker: {marker}")
    if not lines or lines[-1] != COMPLETION:
        raise ValueError("missing terminal C54x test completion")
    if any(line.startswith("Fatal error:") and line != COMPLETION for line in lines):
        raise ValueError("unexpected fatal error before completion")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--rompath", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, default=Path("."))
    args = parser.parse_args()
    args.output_root.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix="run_c54x_core_", dir=args.output_root.resolve()))
    command = [str(args.mame.resolve()), "tms54test", "-rompath", str(args.rompath.resolve()),
               "-video", "none", "-sound", "none", "-nothrottle", "-noreadconfig",
               "-log", "-seconds_to_run", "1"]
    print(f"C54x evidence: {run}", flush=True)
    with (run / "console.log").open("w") as output:
        result = subprocess.run(command, cwd=run, stdout=output, stderr=subprocess.STDOUT)
    text = (run / "console.log").read_text()
    print(text, end="")
    try:
        check_result(text, result.returncode)
    except ValueError as error:
        print(f"C54x conformance: FAIL: {error}", file=sys.stderr)
        return 1
    print("C54x conformance: PASS (isolated native run)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

