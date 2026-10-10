#!/usr/bin/env python3
"""Isolated native C54x conformance run; intentional completion exits with code 3."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

REQUIRED = [
    "TMS320C54x loop XC protection: PASS cases=4 body_words=1,2 sources=irq,nmi",
    "TMS320C54x nested repeats: PASS cases=4 counts=zero,two forms=short,long",
    "TMS320C54x repeat endpoints: PASS cases=12 endpoints=ordinary,ffff,0000 counts=short,long sources=irq,nmi",
    "TMS320C54x BRAF control: PASS cases=4 clear=RSBX,STM,POPM restored_loop=1",
    "TMS320C54x XC interrupt protection: PASS cases=4 spans=1,2 accepted,rejected=1 physical_pipeline_claim=0",
    "TMS320C54x literal disassembler: PASS decoded_modes=992 pages=2 invalid_mmr=32 bounded_extensions=1 operand_order=1 pipeline_claim=0",
    "TMS320C54x MMR bus: PASS unified_state=1 partial_write=1 ifr_clear=1 passive_tap=1 reserved_gap=1",
    "TMS320C54x zero BK addressing: PASS variants=320",
    "TMS320C54x SACCD conformance: PASS variants=576",
    "TMS320C54x RCD accumulator conditions: PASS variants=36",
    "TMS320C54x MIN/MAX conformance: PASS variants=256",
    "TMS320C54x BSP interrupt sources: PASS cases=9 save_replay=9 handset_audio_claim=0",
    "TMS320C54x CLKMD write protection: PASS cases=16 physical_clock_claim=0",
    "TMS320C54x vector prefetch: PASS sources=irq,nmi before_stack=1 sp_access=1 retained_opcode=1 extension_fetch=1 save_replay=2 single_ack=1 later_mask=1 reset=1 debugger_redirect=1 physical_latency_claim=0",
    "TMS320C54x ROLTC conformance: PASS variants=256",
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
    "TMS320C54x TRAP conformance: PASS variants=64",
    "TMS320C54x TRN store conformance: PASS variants=12",
    "TMS320C54x indirect PSHM conformance: PASS variants=8",
    "TMS320C54x parallel load multiply conformance: PASS variants=64",
    "TMS320C54x rounded load conformance: PASS variants=64",
    "TMS320C54x immediate repeat conformance: PASS variants=256",
    "TMS320C54x short immediate load conformance: PASS variants=512 sxm_settings=2",
    "TMS320C54x rounded multiply boundaries: PASS vectors=22 destinations=2 addressing_modes=2 sticky_overflow_states=2",
    "TMS320C54x NMI idle wake: PASS cases=12 cycle_accuracy_claim=0",
    "TMS320C54x NMI pending restore: PASS cases=12 held_line_cases=12 timing_claim=0",
    "TMS320C54x control disassembler: PASS vectors=48 pages=2"
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
    parser.add_argument("--output-root", type=Path, default=Path("tmp"))
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

