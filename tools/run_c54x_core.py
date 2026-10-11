#!/usr/bin/env python3
"""Isolated native C54x conformance run; intentional completion exits with code 3."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

REQUIRED = [
    "TMS320C54x staged frontend: PASS foreground_execute=6 vector_fetch=6 stack_write=7 isr_execute=10 pending_restore=1 coverage=register_one_word native_enabled=0",
    "TMS320C54x staged frontend exclusions: PASS unsupported_stops=1 atomic_fallback=0",
    "TMS320C54x staged empty frontend: PASS irq=1 nmi=1 return_pc=0600 vector_fetch=4 stack_write=5 isr_execute=8",
    "TMS320C54x staged direct load: PASS cases=2 read=5 execute=6 retained_operand=1 restore_after_read=1 bus_reads_per_case=1",
    "TMS320C54x staged multiword: PASS cases=4 literal_fetch=3 complete_decode=4 execute=7 vector_fetch=7 return_pc=0602 pending_decode_restore=4",
    "TMS320C54x staged multiword stream: PASS long_execute=7,9 short_execute=10 literal_reads=2",
    "TMS320C54x staged stores: PASS cases=12 address=Read write=Execute writes_per_leg=1 pending_store_restore=12",
    "TMS320C54x staged indirect loads: PASS cases=8 ar_update=Access read=Read execute=Execute captured_address_restore=8",
    "TMS320C54x staged RMW: PASS cases=12 forms=DP,postincrement,absolute extension_restore=6 sampled_read_restore=6 duplicate_reads=0",
    "TMS320C54x staged mask RMW: PASS read=7 write=8 accepted_irq=1 subsequent_masked=1 pending_write_restore=1 duplicate_reads=0",
    "TMS320C54x staged fast returns: PASS cases=8 target_fetch=9 return_execute=10 delay_execute=11,12 resumed_execute=13 stack_reads=0 restore=8",
    "TMS320C54x staged return multiword delay: PASS cases=4 partial_decode_restore=2 completed_decode_restore=2 delay_execute=12 resumed_execute=13",
    "TMS320C54x staged pending return IRQ: PASS cases=4 request=9 vector_fetch=13,15 stack_write=14,16 continuation=0602,0604 pending_restore=4",
    "TMS320C54x staged branches: PASS cases=8 literal_fetch=3 target_fetch=6 delay_execute=8,9 target_execute=10 restore=8",
    "TMS320C54x staged branch IRQ: PASS cases=8 request=5 vector_fetch=10,12 stack_write=11,13 continuation=0701,0703 pending_restore=8",
    "TMS320C54x staged DAGEN immediate: PASS cases=12 targets=AR,SP,BK write=5 conflicting_write=7 deferred_restore=3 committed_restore=6 duplicate_writes=0",
    "TMS320C54x peripheral MMR timing: PASS cases=20 families=STM,ANDM,ORM,ST,LD extra_cycles=1 rmw_double_charge=0 scratch_wait=0",
    "TMS320C54x staged peripheral waits: PASS cases=16 extra_cycles=1 rmw_double_charge=0 held_stage_restore=8 resumed_access_restore=8 duplicate_bus_accesses=0 silicon_phase_claim=0",
    "TMS320C54x staged peripheral arbitration exclusion: PASS stopped_cycle=6 bus_accesses=0 atomic_fallback=0",
    "TMS320C54x staged immediate stores: PASS cases=8 forms=DP,postincrement,absolute,peripheral retained_literal=1 duplicate_writes=0 restore=8",
    "TMS320C54x staged XPC push: PASS cases=6 source_read=5 stack_write=6 restore=6 duplicate_accesses=0 silicon_phase_claim=0",
    "TMS320C54x staged far return: PASS cases=12 fret=6 frete=6 stack_reads=5,6 target_fetch=8 target_execute=12 restore=12 duplicate_reads=0 silicon_phase_claim=0",
    "TMS320C54x staged INTM protection: PASS cases=12 follower_words=1,2,3 adjacent=6 restore=12 duplicate_vectors=0",
    "TMS320C54x staged status bits: PASS cases=128 registers=2 bits=16 operations=2 initial_patterns=2 restore=128",
    "TMS320C54x staged timer observation: complete cases=8 fidelity_acceptance=0",
    "TMS320C54x FIRS sum: PASS cases=32 sxm=0,1 ovm=0,1 carry=1 sticky_ova=1 pipeline_claim=0",
    "TMS320C54x ABDST overflow: PASS cases=16 frct=0,1 ovm=0,1 sticky_ovb=1 pipeline_claim=0",
    "TMS320C54x POLY boundaries: PASS cases=64 frct=0,1 ovm=0,1 sxm=0,1 sticky_ova=1 pipeline_claim=0",
    "TMS320C54x SQDST difference: PASS cases=32 sxm=0,1 ovm=0,1 carry_borrow=1 sticky_ova=1 pipeline_claim=0",
    "TMS320C54x SQDST B overflow: PASS cases=8 ovm=0,1 sticky_ovb=1 smul=0 pipeline_claim=0",
    "TMS320C54x RND boundaries: PASS cases=128 selectors=4 inputs=8 ovm=0,1 status_preserved=1 pipeline_claim=0",
    "TMS320C54x fast return bus: PASS cases=3 stack_reads=0 delayed_words=2",
    "TMS320C54x extended program conformance: PASS",
    "TMS320C54x program overlay conformance: PASS",
    "TMS320C54x immediate ARP conformance: PASS variants=8",
    "TMS320C54x memory DP conformance: PASS variants=3",
    "TMS320C54x delayed ALEQ conformance: PASS variants=5",
    "TMS320C54x BITF offset conformance: PASS variants=4",
    "TMS320C54x live BIO conformance: PASS variants=28",
    "TMS320C54x live BIO wait release: PASS",
    "TMS320C54x BIT Xmem conformance: PASS variants=512",
    "TMS320C54x BANZ offset conformance: PASS variants=96",
    "TMS320C54x CMPR conformance: PASS variants=192",
    "TMS320C54x BRAF/CPL independence: PASS",
    "TMS320C54x carry branch conformance: PASS variants=8",
    "TMS320C54x CMPM offset conformance: PASS variants=4",
    "TMS320C54x compact load conformance: PASS variants=16",
    "TMS320C54x immediate DP conformance: PASS variants=1024",
    "TMS320C54x compact store conformance: PASS variants=512",
    "TMS320C54x delayed accumulator predicates conformance: PASS variants=60",
    "TMS320C54x delayed conditional call conformance: PASS variants=9",
    "TMS320C54x ASM store conformance: PASS variants=128",
    "TMS320C54x store shift boundaries: PASS cases=120 asm=-16,-1 sxm=0,1 sst=0,1 forms=5 sources=A,B pipeline_claim=0",
    "TMS320C54x SMUL controls: PASS cases=192 families=3 rounded=0,1 frct=0,1 ovm=0,1 smul=0,1 destinations=A,B pipeline_claim=0",
    "TMS320C54x reverse carry conformance: PASS variants=2688",
    "TMS320C54x reverse carry sequences: PASS forward=16 inverse=16",
    "TMS320C54x timer debugger registers: PASS independent=1 reload=1",
    "TMS320C54x INTM protection: PASS one_word=1 three_words=1 return=1",
    "TMS320C54x IDLE1 wake: PASS external_cases=4 timer_cases=2",
    "TMS320C54x IDLE2/3 timer clock: PASS cases=4 save_replay=4",
    "TMS320C54x IDLE2/3 McBSP internal clock: PASS cases=4 save_replay=4",
    "TMS320C54x IDLE2/3 McBSP external clock: PASS cases=4",
    "TMS320C54x IDLE DMA transfer: PASS cases=10 save_replay=10 clock_rate_claim=0",
    "TMS320C54x internal DMA cadence: PASS cases=10 replay=10 words=4 clocks=16",
    "TMS320C54x IDLE3 DMA clock: PASS cases=2 replay=2 system_hz=13000000 input_hz=3250000",
    "TMS320C54x IDLE3 DMA absent clock: PASS cases=2 replay=2 pending_preserved=1",
    "TMS320C54x DMA partial clock loss: PASS cases=2 replay=2 first_word_remaining_clocks=2",
    "TMS320C54x extended save replay: PASS pending_call pending_return xpc stack delay",
    "TMS320C54x accepted interrupt checkpoint: PASS ifr_until_fetch=1 intm_at_iack=1 decode_before_access=1 later_imr=1 source_latched=1 save_replay=1 pipeline_drain_claim=0",
    "TMS320C54x interrupt entry stages: PASS sources=irq,nmi fetch_to_stack_cycles=1 pipeline_drain_claim=0",
    "TMS320C54x entry stage restoration: PASS cases=10 sources=irq,nmi boundaries=5 one_fetch=1 one_ack=1 one_stack_write=1 retained_opcode=1 pipeline_drain_claim=0",
    "TMS320C54x shifted arithmetic selectors: PASS cases=104 operations=2 selectors=4 shifts=-16,-1,0,1,15 sxm=0,1 pipeline_claim=0",
    "TMS320C54x shifted memory disassembler: PASS forms=114688 rejected=130176 captured=18 syntax=20 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x MVPD disassembler: PASS forms=256 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x short shifted store disassembler: PASS forms=1024 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x memory MAC/MAS disassembler: PASS forms=2048 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x accumulator-A multiply disassembler: PASS memory_forms=1024 short_forms=4 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x conditional subtract disassembler: PASS forms=512 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x double arithmetic disassembler: PASS forms=2048 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x memory repeat/store disassembler: PASS forms=768 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x dual/accumulator arithmetic disassembler: PASS dual_forms=1024 accumulator_forms=16 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x logical shift disassembler: PASS forms=128 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x memory multiply/bit disassembler: PASS forms=1792 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x accumulator logical disassembler: PASS forms=256 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x special load disassembler: PASS memory_forms=768 arp=8 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x dual move/DP disassembler: PASS moves=256 dp=512 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x frame/MMR move disassembler: PASS frames=256 moves=81 rejected=175 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x parallel load disassembler: PASS forms=1536 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x WRITA disassembler: PASS forms=256 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x unsigned load disassembler: PASS forms=512 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x immediate load disassembler: PASS forms=34 rejected=34 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x accumulator ALU disassembler: PASS shifts=256 asm=12 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x normalization disassembler: PASS forms=12 rejected=4 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x conditional return disassembler: PASS forms=44 pages=2 named_predicates=22 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x unary disassembler: PASS forms=12 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x ADDM disassembler: PASS forms=256 pages=2 bounded_extensions=1 pipeline_claim=0",
    "TMS320C54x operand fetch order: PASS cases=53 families=12 forms=direct,indirect,long,absolute,preincrement",
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
    "TMS320C54x SACCD boundaries: PASS cases=96 asm=-16,-1,0,15 sxm=0,1 sst_ignored=1 sources=A,B false_store=1 pipeline_claim=0",
    "TMS320C54x RCD accumulator conditions: PASS variants=36",
    "TMS320C54x MIN/MAX conformance: PASS variants=256",
    "TMS320C54x BSP interrupt sources: PASS cases=9 save_replay=9 handset_audio_claim=0",
    "TMS320C54x CLKMD write protection: PASS cases=16 physical_clock_claim=0",
    "TMS320C54x PLL transition timing: PASS lock_delay=1 protected_exit=1 cancel=1 retained_lock=1 multiplier_relock=1 save_replay=1 execution_rate_claim=0",
    "TMS320C54x CLKMD rates: PASS div_hz=5997600 pll_hz=119952000 timer=1 dma=1 input_clock_dma=1 internal_mcbsp=1 save_restore=1 reset_profile=1 physical_rate_claim=0",
    "TMS320C54x IDLE3 PLL relock: PASS div_wake=1 power_held=1 lock_invalidated=1 software_reentry=1 automatic_pll_wake_claim=0",
    "TMS320C54x vector prefetch: PASS sources=irq,nmi before_stack=1 sp_access=1 retained_opcode=1 extension_fetch=1 save_replay=2 single_ack=1 later_mask=1 reset=1 debugger_redirect=1 physical_latency_claim=0",
    "TMS320C54x ROLTC conformance: PASS variants=256",
    "TMS320C54x core conformance: PASS",
    "TMS320C54x ASM accumulator add conformance: PASS left_shift=1 overflow_variants=4",
    "TMS320C54x ASM accumulator subtract conformance: PASS overflow_variants=4 source_preserved=1",
    "TMS320C54x ASM arithmetic shifts: PASS variants=512 shifts=32 sxm_settings=2 ovm_settings=2 sticky_settings=2",
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
    "TMS320C54x rounded multiply boundaries: PASS vectors=26 destinations=2 addressing_modes=2 sticky_overflow_states=2",
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
