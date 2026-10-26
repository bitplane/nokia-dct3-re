#!/usr/bin/env python3
"""Isolated native C54x conformance run; intentional completion exits with code 3."""
import argparse
import re
from pathlib import Path
import sys
import tempfile

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.process_runner import run_emulator
from tools.c54x_profiles import machine_for_profile

# Keep the established checker imports available to callers.
from tools.c54x_test_oracles import (
    IMMEDIATE_MAPPED_REQUIRED,
    DST_BYPASS_REQUIRED,
    IMMEDIATE_MULTIPLY_DISASM,
    REQUIRED,
    COMPLETION,
    BUS_REGISTER_REQUIRED,
    BUS_REGISTER_COMPLETION,
    BUS_IO_REQUIRED,
    BUS_IO_COMPLETION,
    ATOMIC_IO_REQUIRED,
    ATOMIC_IO_COMPLETION,
    COMPOSE_REQUIRED,
    COMPOSE_COMPLETION,
    BUS_STM_REQUIRED,
    BUS_STM_COMPLETION,
    READ_A_MAPPED_REQUIRED,
    READ_A_MAPPED_COMPLETION,
    DP_ABSOLUTE_REQUIRED,
    DP_ABSOLUTE_COMPLETION,
    EXTRA_PROFILES,
)

NAMED_SCOPES = {
    'bus_registers': ('bus-registers', BUS_REGISTER_REQUIRED, BUS_REGISTER_COMPLETION),
    'bus_io': ('bus-io', BUS_IO_REQUIRED, BUS_IO_COMPLETION),
    'atomic_io': ('bus-io-atomic', ATOMIC_IO_REQUIRED, ATOMIC_IO_COMPLETION),
    'compose': ('bus-io-compose', COMPOSE_REQUIRED, COMPOSE_COMPLETION),
    'bus_stm': ('bus-stm', BUS_STM_REQUIRED, BUS_STM_COMPLETION),
    'mapped_read_a': ('read-a-mapped', READ_A_MAPPED_REQUIRED, READ_A_MAPPED_COMPLETION),
    'dp_absolute': ('dp-absolute', DP_ABSOLUTE_REQUIRED, DP_ABSOLUTE_COMPLETION),
}


def check_result(text, returncode, *, bus_registers=False, bus_io=False, atomic_io=False, compose=False, bus_stm=False, mapped_read_a=False, dp_absolute=False, profile=None, verbose_teardown=False):
    if re.search(r'^c54x_failure:', text, re.MULTILINE):
        raise ValueError('conformance assertion diagnostic is present')
    if re.search(r'invalid BIOS|reverting to default', text, re.IGNORECASE):
        raise ValueError('fixture BIOS selection fell back to another configuration')
    enabled = dict(bus_registers=bus_registers, bus_io=bus_io, atomic_io=atomic_io,
                   compose=compose, bus_stm=bus_stm, mapped_read_a=mapped_read_a,
                   dp_absolute=dp_absolute)
    selected = [(marker, ending) for flag, (_, marker, ending) in NAMED_SCOPES.items()
                if enabled[flag]]
    if profile is not None:
        if profile not in EXTRA_PROFILES:
            raise ValueError(f"unknown isolated profile: {profile}")
        selected.append(EXTRA_PROFILES[profile])
    if len(selected) > 1:
        raise ValueError("choose one isolated conformance scope")
    if returncode != 3:
        raise ValueError(f"unexpected MAME exit status {returncode}; expected test completion exit 3")
    lines = text.splitlines()
    # Verbose SDL shutdown follows the intentional fatal-error completion.
    # Admit only the exact suffix, never arbitrary output after the result.
    if verbose_teardown and lines[-2:] == ['Enter sdlwindow_exit', 'Leave sdlwindow_exit']:
        lines = lines[:-2]
    if "Errors:" in lines:
        raise ValueError("MAME reported driver validity errors")
    required, completion = ([selected[0][0]], selected[0][1]) if selected else (REQUIRED, COMPLETION)
    if selected and isinstance(required[0], tuple):
        required = required[0]
    for marker in required:
        if sum(line == marker or (not selected and line.startswith(marker + " ")) for line in lines) != 1:
            raise ValueError(f"missing or duplicated conformance marker: {marker}")
    if not lines or lines[-1] != completion:
        raise ValueError("missing terminal C54x test completion")
    if any(line.startswith("Fatal error:") and line != completion for line in lines):
        raise ValueError("unexpected fatal error before completion")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--rompath", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, default=Path("tmp"))
    parser.add_argument("--verbose", action="store_true", help="retain exact opcode/extension execution inventory")
    parser.add_argument("--bus-registers", action="store_true",
                        help="run opt-in C5416 register conformance, not bus timing or full core acceptance")
    parser.add_argument("--bus-io", action="store_true", help="run staged external I/O wait/READY conformance")
    parser.add_argument("--atomic-io", action="store_true", help="run atomic external I/O wait/READY conformance")
    parser.add_argument("--compose", action="store_true", help="run atomic waited I/O repeat/delay/INTM composition")
    parser.add_argument("--bus-stm", action="store_true", help="run staged C5416 bus-control write conformance")
    parser.add_argument("--mapped-read-a", action="store_true", help="run staged mapped-accumulator READA conformance")
    parser.add_argument("--dp-absolute", action="store_true", help="run staged absolute memory-to-DP conformance")
    parser.add_argument("--profile", choices=sorted(EXTRA_PROFILES), help="run an additional isolated instruction profile")
    args = parser.parse_args()
    if sum((args.bus_registers, args.bus_io, args.atomic_io, args.compose, args.bus_stm, args.mapped_read_a, args.dp_absolute, bool(args.profile))) > 1:
        parser.error('choose one isolated bus profile')
    profile = args.profile or next((scope[0] for flag, scope in NAMED_SCOPES.items()
                                    if getattr(args, flag)), None)
    try:
        machine = machine_for_profile(profile) if profile else 'tms54test'
    except ValueError as error:
        parser.error(str(error))
    args.output_root.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix="run_c54x_core_", dir=args.output_root.resolve()))
    command = [str(args.mame.resolve()), machine, "-rompath", str(args.rompath.resolve()),
               "-video", "none", "-sound", "none", "-joystickprovider", "none",
               "-midiprovider", "none", "-nothrottle", "-noreadconfig",
               # This is a stop bound, not acceptance: every marker and the
               # intentional test-completion exit remain mandatory.
               "-log", "-seconds_to_run", "5"]
    if args.verbose:
        command.append("-verbose")
    if profile:
        command.extend(["-bios", profile])
    print(f"C54x evidence: {run}", flush=True)
    with (run / "console.log").open("w") as output:
        returncode = run_emulator(command, run, output)
    text = (run / "console.log").read_text()
    print(text, end="")
    try:
        check_result(text, returncode, bus_registers=args.bus_registers, bus_io=args.bus_io, atomic_io=args.atomic_io, compose=args.compose, bus_stm=args.bus_stm, mapped_read_a=args.mapped_read_a, dp_absolute=args.dp_absolute, profile=args.profile, verbose_teardown=args.verbose)
    except ValueError as error:
        print(f"C54x conformance: FAIL: {error}", file=sys.stderr)
        return 1
    print(f"C54x {args.profile}: PASS (bounded instruction profile; silicon phases unvalidated)" if args.profile else
          "C54x absolute memory DP: PASS (silicon phases unvalidated)" if args.dp_absolute else
          "C54x mapped READA: PASS (bounded register forms; silicon phases unvalidated)" if args.mapped_read_a else
          "C54x staged bus STM: PASS (physical mapping unvalidated)" if args.bus_stm else
          "C54x atomic I/O composition: PASS (bounded control forms; physical mapping unvalidated)" if args.compose else
          "C54x atomic I/O conformance: PASS (repeat/guard acceptance separate; physical mapping unvalidated)" if args.atomic_io else
          "C54x staged I/O conformance: PASS (atomic acceptance separate; physical mapping unvalidated)" if args.bus_io else
          "C54x conformance: PASS (isolated native run)" if not args.bus_registers else
          "C54x bus-register conformance: PASS (registers only, not bus timing)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
