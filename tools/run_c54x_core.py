#!/usr/bin/env python3
"""Isolated native C54x conformance run; intentional completion exits with code 3."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

REQUIRED = [
    "TMS320C54x parallel load disassembler: PASS forms=1536 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x WRITA disassembler: PASS forms=256 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x unsigned load disassembler: PASS forms=512 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x immediate load disassembler: PASS forms=34 rejected=34 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x accumulator ALU disassembler: PASS shifts=256 asm=12 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x normalization disassembler: PASS forms=12 rejected=4 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x conditional return disassembler: PASS forms=44 pages=2 named_predicates=22 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x unary disassembler: PASS forms=12 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x ADDM disassembler: PASS forms=256 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x operand fetch order: PASS cases=41 families=11 forms=direct,indirect,long,absolute",
    "TMS320C54x accumulator call fast return: PASS cases=2 sources=A,B",
    "TMS320C54x native gap arithmetic/load: PASS words=3 cases=6 sxm=0,1",
    "TMS320C54x native gap branch/address: PASS words=6 cases=12 zero_nonzero=1 ar0_signed=1 stores=low,high",
    "TMS320C54x native gap math: PASS words=4 cases=8 sxm=0,1 frct=0,1 carry=0,1",
    "TMS320C54x native gap memory: PASS words=6 cases=12 sxm=0,1 frct=0,1 circular=0,8 double_step=2",
    "TMS320C54x native final gaps: PASS words=6 cases=12 signed_shift=1 saturation=1 compact_store=1",
    "TMS320C54x move/test disassembler: PASS decoded_modes=2048 pages=2 bounded_extensions=1 operand_order=1 pipeline_claim=0",
    "TMS320C54x conditional disassembler: PASS forms=132 pages=2 named_predicates=22 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x load disassembler: PASS modes=1536 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x memory logical disassembler: PASS modes=1536 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x memory ALU disassembler: PASS modes=4096 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x accumulator disassembler: PASS shift_forms=384 rotate_forms=6 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x immediate ALU disassembler: PASS forms=340 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x dual multiply disassembler: PASS forms=4608 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x MAR disassembler: PASS indirect=128 direct_unclaimed=128 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x high memory ALU disassembler: PASS modes=2048 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x double memory disassembler: PASS modes=1024 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x BANZ disassembler: PASS indirect=240 unclaimed=272 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x MMR disassembler: PASS modes=1344 rejected=192 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x parallel disassembler: PASS forms=4352 complement=4 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x store/short disassembler: PASS stores=2048 immediates=544 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x move/stack disassembler: PASS forms=960 rejected=64 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x indirect POPM: PASS cases=4 pre_post=1 compatibility=1 sp_destination=1 pipeline_claim=0",
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
               "-log", "-seconds_to_run", "2"]
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

