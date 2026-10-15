#!/usr/bin/env python3
"""Isolated native C54x conformance run; intentional completion exits with code 3."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

REQUIRED = [
    "TMS320C54x staged STM IMR: PASS cases=4608 restore=4608 irq_bits=16 masks=4 initial_masks=2 intm=2 sources=none,IRQ,NMI pages=2 literal_fetches=1 writes_each=1 destination_reads=0 silicon_phase_claim=0",
    "TMS320C54x staged long MVKD: PASS cases=19200 forms=25 offsets=4 sources=selected_AR,AR2,RAM,alias values=4 compatibility=2 pages=2 restore=19200 reads_each=1 writes_each=1 extensions_each=2 silicon_phase_claim=0",
    "TMS320C54x staged MPY mapped consumer: PASS cases=3072 restore=3072 sources=AL,AH,AG,BL,BH,BG gaps=0,1 reads_each=1 literal_fetches=1 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged mapped BITF: PASS cases=13824 registers=AL,AH,AG,BL,BH,BG,T,TRN masks=4 initial_tc=2 producer_cases=1536 restore=13824 reads_each=1 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged mapped word arithmetic: PASS cases=6912 registers=AL,AH,AG,BL,BH,BG,T,TRN selectors=4 forms=direct,absolute producer_cases=768 restore=6912 reads_each=1 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged NEG: PASS cases=13824 selectors=4 patterns=16 status_states=32 pages=2 producer_cases=1536 restore=13824 data_accesses=0 silicon_phase_claim=0",
    "TMS320C54x staged RMW forwarding: PASS cases=768 restore=768 same_and_different=1 gap_words=0,1 flags=4 pages=2 cpl=2 silicon_phase_claim=0",
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
    "TMS320C54x staged MMR push: PASS cases=198 sources=32 pages=2 source_read=5 stack_write=6 restore=198 duplicate_accesses=0 silicon_phase_claim=0",
    "TMS320C54x staged far return: PASS cases=36 plain=12 delayed=24 multiword_delay=12 restore=36 duplicate_reads=0 silicon_phase_claim=0",
    "TMS320C54x staged cross-page far IRQ: PASS cases=8 masked=4 restored_vector_page=1 continuations=0700,0702 restore=8 duplicate_vectors=0 silicon_phase_claim=0",
    "TMS320C54x staged INTM protection: PASS cases=12 follower_words=1,2,3 adjacent=6 restore=12 duplicate_vectors=0",
    "TMS320C54x staged status bits: PASS cases=128 registers=2 bits=16 operations=2 initial_patterns=2 restore=128 xf_output=1",
    "TMS320C54x staged CPL/DP latency: PASS cases=32 fields=2 directions=2 consumers=LD,ST gaps=0,1,2,3 restore=32",
    "TMS320C54x staged immediate DP: PASS cases=1024 pages=512 nearest_producer=512 old_st0_until_execute=1 restore=1024",
    "TMS320C54x staged FRAME: PASS offsets=256 restore=768 adjacent_push=768 duplicate_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged stack SP latency: PASS cases=16 producers=PSHM,POPM consumers=LD,ST gap_nops=0,1 restore=16",
    "TMS320C54x staged MMR pop: PASS cases=114 destinations=19 pages=2 stack_read=5 dagen_write=5 other_write=6 restore=114 duplicate_accesses=0 silicon_phase_claim=0",
    "TMS320C54x staged POPM DAGEN conflict: PASS cases=12 bank_conflicts=6 ram_controls=6 restore=12 duplicate_accesses=0",
    "TMS320C54x staged original MU4 context: PASS words=37 cases=6 context_saves=8 context_restored=1 returned=4044 restore=6 full_handler_claim=0",
    "TMS320C54x staged DLD: PASS cases=128 destinations=A,B sxm=0,1 c16=0,1 parity=0,1 addressing=DP,CPL restore=128 duplicate_reads=0 silicon_phase_claim=0",
    "TMS320C54x staged immediate ADD: PASS cases=256 selectors=4 shifts=0,1,7,15 sxm=0,1 ovm=0,1 restore=256 duplicate_literals=0",
    "TMS320C54x staged DST: PASS cases=192 sources=A,B sst=0,1 parity=0,1 addressing=DP,CPL writes=6,7 restore=192 duplicate_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged LDM: PASS cases=288 sources=9 destinations=A,B pages=2 sxm=0,1 cpl=0,1 restore=288 duplicate_reads=0",
    "TMS320C54x staged logical immediates: PASS cases=768 operations=AND,OR,XOR selectors=4 shifts=0,1,7,15 restore=768 duplicate_literals=0",
    "TMS320C54x staged memory DP: PASS pages=512 cases=1536 source_read=5 producer_execute=8 consumer_execute=9 restore=1536 duplicate_reads=0 silicon_phase_claim=0",
    "TMS320C54x staged conditional branches: PASS cases=96 conditions=AEQ,ANEQ,BEQ,BNEQ pages=2 outcomes=2 memory_dp_delay=48 restore=96 duplicate_fetches=0 silicon_phase_claim=0",
    "TMS320C54x staged plain returns: PASS cases=36 forms=RET,RETE pages=2 adjacent_old_word=12 nop_new_word=12 restore=36 duplicate_accesses=0",
    "TMS320C54x staged calls: PASS cases=132 forms=CALL,CALLD pages=2 adjacent_push=66 stm_t_delay=108 parameters=9 restore=132 duplicate_stack_writes=0",
    "TMS320C54x staged MVDK: PASS cases=108 direct=60 indirect=48 restore=108 bank_conflicts=9 source_ar_alias=12 duplicate_accesses=0",
    "TMS320C54x staged original worker XC prefix: PASS cases=14 restore=14 composed_seven_words=4 composed_nine_words=6 duplicate_pushes=0",
    "TMS320C54x staged unconditional XC IRQ protection: PASS cases=4 forms=XC1,XC2 restore=4 protected_words=2 vector_fetches=1",
    "TMS320C54x staged XC arithmetic IRQ: PASS cases=32 forms=XC1,XC2 operations=absolute_ADD,long_SUB destinations=A,B pages=2 restore=32 duplicate_source_reads=0 duplicate_vectors=0",
    "TMS320C54x staged XC TC sampling: PASS cases=128 forms=TC,NTC words=1,2 restore=128 two_word_slots=32 producer_cases=64 access_sampled=1 rejected_writes=0",
    "TMS320C54x staged XC accumulator sampling: PASS cases=48 conditions=AEQ,ANEQ,BEQ,BNEQ words=1,2 restore=48 access_sampled=1 rejected_writes=0",
    "TMS320C54x staged plain conditional branches: PASS cases=48 conditions=AEQ,ANEQ,BEQ,BNEQ pages=2 outcomes=2 restore=48 duplicate_fetches=0 taken_data_reads=0",
    "TMS320C54x staged delayed returns: PASS cases=24 pages=2 delay_forms=3 restore=24 xc_irq_cases=12 duplicate_stack_reads=0 third_delay_fetches=0",
    "TMS320C54x staged original worker return tail: PASS cases=10 original_words=7 saved_intm_states=2 restore=10 duplicate_stack_reads=0 irq_after_continuation=1",
    "TMS320C54x staged original worker roundtrip: PASS cases=16 original_words=20 route=skip_body saved_intm_states=2 restore=16 duplicate_stack_reads=0 duplicate_source_reads=0",
    "TMS320C54x staged original context call route: PASS cases=40 original_words=67 pages=2 saved_intm_states=2 restore=40 context_restored=1 counter_increments=1 full_isr_claim=0",
    "TMS320C54x staged ADDM: PASS cases=768 address_forms=4 value_pairs=8 status_combinations=8 restore=768 duplicate_reads=0 duplicate_writes=0",
    "TMS320C54x staged PSHD: PASS cases=44 address_forms=4 compiler_latency=6 addm_forwarded=6 restore=44 duplicate_reads=0 duplicate_stack_writes=0",
    "TMS320C54x staged RETD DARAM ordering: PASS cases=12 adjacent_old_word=6 nop_new_word=6 restore=12 duplicate_accesses=0",
    "TMS320C54x staged POPD: PASS cases=50 address_forms=4 compiler_latency=6 original_copy_words=6 copy_cases=12 copy_sxm_states=2 restore=50 duplicate_stack_reads=0 duplicate_writes=0",
    "TMS320C54x staged short register stores: PASS cases=75 sources=6 address_forms=4 multi_store_cases=3 restore=75 duplicate_writes=0",
    "TMS320C54x staged CMPR: PASS cases=435 conditions=4 auxiliary_registers=8 unsigned_pairs=4 producer_latency_cases=48 producers=STM,MVDK,POPM,STL original_store_sequences=3 restore=435",
    "TMS320C54x staged MAR: PASS cases=294 forms=6 auxiliary_registers=8 compatibility_states=2 original_worker_words=6 original_xc_outcomes=2 restore=294 duplicate_data_reads=0 duplicate_offset_fetches=0",
    "TMS320C54x staged memory logic: PASS cases=576 operations=3 accumulators=2 address_forms=4 value_pairs=4 sxm_states=2 restore=576 duplicate_reads=0",
    "TMS320C54x staged original MU4 logical tail: PASS cases=5 original_words=6 page=2 restore=5 duplicate_reads=0 duplicate_writes=0",
    "TMS320C54x staged complete MU4 worker: PASS cases=48 original_worker_words=38 call_words=4 control=zero cmpr_outcomes=2 saved_intm_states=2 sxm_states=2 restore=48 duplicate_reads=0 duplicate_writes=0",
    "TMS320C54x staged complete MU4 context body: PASS cases=128 original_words=89 pages=2 saved_intm_states=2 sxm_states=2 cmpr_outcomes=2 restore=128 context_restored=1 counter_increments=1 full_isr_claim=0",
    "TMS320C54x staged FB: PASS cases=40 source_pages=2 target_pages=5 restore=40 duplicate_literal_fetches=0 duplicate_target_fetches=0",
    "TMS320C54x staged original MU4 timer ISR: PASS cases=48 original_words=94 foreground_pages=2 sxm_states=2 cmpr_outcomes=2 restore=48 timer_vector=1 original_frete=1 foreground_retired=1",
    "TMS320C54x staged recurring MU4 timer: PASS cases=96 returns_per_leg=8 periods=2 foreground_pages=2 sxm_states=2 control_outcomes=2 restore=96 exact_return_cycle=1 firmware_reseeding=0",
    "TMS320C54x staged absolute LD: PASS cases=64 accumulators=2 sxm_states=2 values=4 restore=64 duplicate_literal_fetches=0 duplicate_operand_reads=0",
    "TMS320C54x staged FCALL/FRET: PASS cases=80 source_pages=2 target_pages=5 restore=80 stack_words=2 duplicate_stack_writes=0 duplicate_stack_reads=0",
    "TMS320C54x staged explicit SP: PASS cases=320 producers=POPM,STM,MVDK,FRAME,POPM_conflict compiler_consumers=LD,ST gap_nops=0,1,2,3 restore=320 duplicate_reads=0 duplicate_writes=0",
    "TMS320C54x staged original idle SP tail: PASS cases=4 original_words=4 restore=4 duplicate_reads=0 duplicate_sp_writes=0",
    "TMS320C54x staged IDLE1: PASS cases=48 wake_profiles=6 source_pages=2 restore=48 suspended_younger_words=1 timer_runs=1 real_irq_return=1",
    "TMS320C54x staged original idle call: PASS cases=8 original_words=44 setup_paths=2 stack_alignments=2 restore=8 external_wake=1 isr_claim=0",
    "TMS320C54x staged original idle timer wake: PASS cases=8 idle_words=44 isr_words=94 stack_alignments=2 restore=8 original_vector=1 original_frete=1",
    "TMS320C54x staged original idle zero/TINT: PASS cases=4 original_words=138 stack_alignments=2 restore=4 mask_drained=1 forced_continuation=0 native_profile_claim=0",
    "TMS320C54x staged STM PMST: PASS cases=64 values=4 source_pages=2 target_regions=2 restore=64 duplicate_literal_fetches=0 duplicate_register_writes=0",
    "TMS320C54x staged MVPD: PASS cases=128 addressing_modes=4 source_pages=2 source_regions=2 ovly_states=2 restore=128 duplicate_program_reads=0 duplicate_data_writes=0",
    "TMS320C54x staged RPT MVPD: PASS cases=128 counts=4 pages=2 ovly_states=2 destination_modes=2 restore=128 count_plus_one=1 duplicate_transfers=0",
    "TMS320C54x staged RPT interrupts: PASS cases=16 sources=IRQ,NMI counts=4 pages=2 restore=16 deferred_until_final_write=1 correct_continuation=1",
    "TMS320C54x staged SP RMW: PASS cases=256 initial_values=4 deltas=4 sxm_states=2 pages=2 restore=256 ordered_reads=1 ordered_writes=1",
    "TMS320C54x staged immediate ARP: PASS cases=512 arp_values=8 encoded_modes=2 cmpt_states=2 postincrement_states=2 pages=2 restore=512 zero_latency=1 duplicate_reads=0 duplicate_auxiliary_updates=0",
    "TMS320C54x staged absolute READA AR1: PASS cases=192 source_pages=3 source_regions=2 ovly_states=2 consumer_gaps=4 restore=192 retained_accumulator_source=1 duplicate_program_reads=0 duplicate_register_writes=0",
    "TMS320C54x staged short READA RAM: PASS cases=48 source_pages=3 source_regions=2 ovly_states=2 restore=48 retained_accumulator_source=1 postincrement_once=1 duplicate_program_reads=0 duplicate_data_writes=0",
    "TMS320C54x staged BANZ: PASS cases=288 values=3 modifications=3 encoded_modes=2 cmpt_states=2 pages=2 restore=288 premodify_condition=1 duplicate_auxiliary_updates=0",
    "TMS320C54x staged memory RPT READA: PASS cases=384 counts=4 source_pages=3 ovly_states=2 source_regions=2 count_sources=AR,RAM restore=384 retained_count=1 retained_program_pointer=1 duplicate_transfers=0",
    "TMS320C54x staged memory RPT interrupts: PASS cases=16 counts=4 sources=IRQ,NMI restore=16 deferred_until_final_write=1 one_word_continuation=1",
    "TMS320C54x staged absolute ADD: PASS cases=2048 operands=4 initial_values=4 sxm_states=2 ovm_states=2 accumulators=2 sources=AR,RAM pages=2 restore=2048 duplicate_reads=0 independent_arithmetic=1",
    "TMS320C54x staged direct RMW: PASS cases=768 operations=ANDM,ORM,ADDM offsets=2 values=4 immediates=2 cpl_states=2 pages=2 restore=768 retained_context=1 duplicate_reads=0 duplicate_writes=0",
    "TMS320C54x staged peripheral logic: PASS cases=864 operations=AND,OR,XOR addressing_modes=3 operands=3 accumulators=2 sxm_states=2 pages=2 restore=864 extra_cycles=1 duplicate_reads=0 peripheral_writes=0",
    "TMS320C54x staged PORTW: PASS cases=320 source_modes=5 values=4 sxm_states=2 pages=2 restore=320 retained_literals=1 duplicate_source_reads=0 duplicate_io_writes=0",
    "TMS320C54x staged direct immediate ST: PASS cases=128 values=4 offsets=2 cpl_states=2 pages=2 restore=128 retained_context=1 destination_reads=0 duplicate_writes=0",
    "TMS320C54x staged STLM AR: PASS cases=768 registers=8 accumulators=2 patterns=4 sst=0,1 sxm=0,1 gaps=0,1,2,3 restore=768 duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged extended DST: PASS cases=3072 forms=offset,preincrement,circular,absolute registers=8 accumulators=2 offsets=4 compatibility=0,1 sst=0,1 restore=3072 duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged short-indirect DST: PASS cases=9216 modes=12 registers=8 accumulators=2 parity=even,odd compatibility=0,1 sst=0,1 restore=9216 duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged long-offset LD: PASS ram_cases=12288 mapped_ar_cases=384 forms=offset,preincrement,circular,absolute registers=8 accumulators=2 offsets=8 sxm_ovm=4 compatibility=2 restore=12672 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged LDU: PASS ram_cases=55296 mapped_ar_cases=2304 address_forms=18 registers=8 accumulators=2 values=8 sxm_ovm=4 compatibility=2 restore=57600 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged memory LD shift16: PASS ram_cases=55296 mapped_ar_cases=2304 address_forms=18 registers=8 accumulators=2 values=8 sxm_ovm=4 compatibility=2 restore=57600 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged variable memory LD: PASS cases=110592 address_forms=18 registers=2 accumulators=2 values=4 shifts=32 sxm_ovm=4 restore=110592 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged MPYU: PASS ram_cases=30720 mapped_cases=24576 accumulators=2 t_values=2 patterns=8 flag_states=16 restore=55296 t_retained=1 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged MPY and MPYR: PASS cases=55296 accumulators=2 rounding=0,1 t_retained=1 restore=55296 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged memory LD T: PASS cases=55296 ram_and_mapped=1 restore=55296 read_publication=1 retirement_rewrite=0 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged LD T multiply pair: PASS cases=768 restore=768 t_read=6 multiply_read=8 publication=9 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged shifted memory ADD/SUB: PASS ram_cases=147456 mapped_ar_cases=98304 selectors=8 shifts=32 sxm_ovm=4 patterns=4 restore=245760 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged SUBC: PASS ram_cases=18432 mapped_ar_cases=12288 value_pairs=64 sxm_ovm=4 accumulators=2 restore=30720 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged repeat SUBC: PASS cases=46080 forms=6 rpt=short,long counts=0,1,15,32,255 sources=none,IRQ,NMI pages=2 restore=46080 count_plus_one=1 divisor_reads_each_iteration=1 duplicate_body_fetches=0 silicon_phase_claim=0",
    "TMS320C54x staged repeat immediate ST: PASS cases=38400 forms=5 rpt=short,long counts=0,1,15,32,255 values=4 sst_sxm_ovm=8 sources=none,IRQ,NMI pages=2 restore=38400 count_plus_one=1 literal_fetches=1 body_fetches=1 destination_reads=0 cycles_each=2 silicon_phase_claim=0",
    "TMS320C54x staged BANZD: PASS cases=3072 registers=8 values=4 modifications=3 cmpt_states=2 pages=2 delay_forms=two_single,one_double restore=3072 duplicate_target_fetches=0 data_accesses=0 silicon_phase_claim=0",
    "TMS320C54x staged STLM T/TRN: PASS cases=12288 sources=2 patterns=8 sst_sxm_ovm_frct=16 gaps=0,1,2,3 consumers=MPY,MPYR,MPYU,LDM restore=12288 register_write=Execute consumer_sample=Read zero_gap_claim=projection_only duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged STLM BRC: PASS cases=3072 sources=2 patterns=8 flags=16 gaps=0,1,2,3 restore=3072 register_write=Execute ldm_sample=Read block_repeat_claim=0 duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged original swap body: PASS cases=1536 patterns=16 flags=16 pages=2 restore=1536 read=Read write=Execute ar6_update=Read duplicate_reads=0 duplicate_writes=0 block_repeat_claim=0 silicon_phase_claim=0",
    "TMS320C54x staged original swap join: PASS cases=1536 patterns=16 flags=16 pages=2 restore=1536 bodies=2 gap_words=0 reads=2 writes=2 ar6_updates=2 block_repeat_claim=0 silicon_phase_claim=0",
    "TMS320C54x staged RPTB: PASS cases=6144 irq_nmi_cases=24 restore=6168 counts=0,1,2,7 zero_overhead_body=5 counter=Decode final_braf=Decode silicon_phase_claim=0",
    "TMS320C54x staged RPTB edges: PASS cases=16 restore=16 one_word=2 two_word=2 exclusions=12 atomic_fallback=0 silicon_phase_claim=0",
    "TMS320C54x staged immediate MPY: PASS isolated_cases=12288 stlm_cases=73728 value_pairs=64 selectors=4 flags=16 restore=86016 literal_fetches=1 t_retained=1 data_reads=0 projection_only_stlm_gap=0 silicon_phase_claim=0",
    "TMS320C54x staged indirect DLD: PASS cases=24576 forms=16 registers=8 accumulators=2 patterns=4 sxm=0,1 c16=0,1 compatibility=0,1 restore=24576 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged long-offset BANZ: PASS cases=2304 forms=offset,preincrement,circular registers=8 offset_pairs=8 compatibility=0,1 pages=2 restore=2304 offset_fetches=1 target_fetches=1 data_accesses=0 silicon_phase_claim=0",
    "TMS320C54x staged immediate arithmetic: PASS sub_cases=26112 add16_cases=1536 producer_cases=144 selectors=4 shifts=0..16 patterns=8 status_states=8 pages=2 restore=27792 duplicate_literals=0 data_accesses=0 silicon_phase_claim=0",
    "TMS320C54x staged register arithmetic: PASS cases=196608 producer_cases=288 operations=ADD,SUB selectors=4 shifts=-16..15 shift_sources=opcode,ASM patterns=8 status_states=8 pages=2 restore=196896 duplicate_opcodes=0 data_accesses=0 silicon_phase_claim=0",
    "TMS320C54x staged register LD: PASS cases=196608 producer_cases=144 selectors=4 shifts=-16..15 shift_sources=opcode,ASM patterns=8 status_states=16 pages=2 restore=196752 duplicate_opcodes=0 data_accesses=0 carry_preserved=1 silicon_phase_claim=0",
    "TMS320C54x staged memory multiply: PASS ram_cases=30720 mapped_cases=24576 operations=MPYA,MASA,MACA,MACAR patterns=8 flag_states=16 restore=55296 t_read_owned=1 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged accumulator logic: PASS cases=6144 operations=AND,OR,XOR selectors=4 shifts=-16..15 patterns=4 sxm=0,1 restore=6144 duplicate_fetches=0 data_accesses=0",
    "TMS320C54x staged CMPL: PASS cases=64 selectors=4 patterns=4 sxm=0,1 restore=64 duplicate_fetches=0 data_accesses=0",
    "TMS320C54x staged MVKD: PASS cases=672 forms=DP,CPL,postincrement,postdecrement,preincrement,absolute,AR sources=AR1,RAM,alias,AR2 values=4 pages=2 category_i_gaps=0,1 restore=672 duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged accumulator shifts: PASS cases=32768 operations=SFTA,SFTL selectors=4 shifts=-16..15 patterns=8 sxm=0,1 ovm=0,1 carry=0,1 restore=32768 duplicate_fetches=0 data_accesses=0 left_c_policy=manual_formula silicon_phase_claim=0",
    "TMS320C54x staged DSUB: PASS cases=6912 address_forms=9 destinations=A,B patterns=8 c16=0,1 sxm=0,1 ovm=0,1 parity=0,1 restore=6912 duplicate_reads=0 memory_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged DADD: PASS cases=13824 address_forms=9 selectors=4 patterns=8 c16=0,1 sxm=0,1 ovm=0,1 parity=0,1 restore=13824 duplicate_reads=0 memory_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged long immediate ST: PASS cases=6144 forms=offset,preincrement,circular registers=8 offsets=4 values=4 compatibility=0,1 pages=2 restore=6144 duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged BIO branches: PASS cases=256 forms=BC,BCD conditions=BIO,NBIO levels=0,1 transitions=4 pages=2 restore=256 line_sample=Read duplicate_callbacks=0 duplicate_fetches=0 silicon_phase_claim=0",
    "TMS320C54x staged MVMM: PASS cases=1944 pairs=81 values=4 pages=2 restore=1944 source_read=Decode destination_write=Access duplicate_reads=0 duplicate_writes=0 zero_gap_ar=1",
    "TMS320C54x staged MVMM SP: PASS cases=24 cpl=0,1 gap_nops=0,1 pages=2 restore=24 compiler_latency=1 duplicate_reads=0 duplicate_writes=0",
    "TMS320C54x staged MVMM producers: PASS cases=48 producers=STM,STLM gap_nops=0,1,2,3 pages=2 restore=48 duplicate_reads=0 duplicate_writes=0",
    "TMS320C54x staged repeat NOP: PASS cases=240 forms=short,long counts=0,1,3,32,255 sources=none,IRQ,NMI pages=2 restore=240 count_plus_one=1 counter_each_clock=1 duplicate_body_fetches=0 deferred_interrupts=1",
    "TMS320C54x staged BIT: PASS cases=9216 xmem_modes=16 bitcodes=16 values=2 initial_tc=2 xc_gaps=0,1,2 restore=9216 duplicate_reads=0",
    "TMS320C54x staged PORTR: PASS cases=416 destination_modes=13 values=4 pages=2 restore=416 retained_literals=1 duplicate_port_reads=0 duplicate_data_writes=0 destination_reads=0",
    "TMS320C54x staged long STL/STH: PASS cases=38400 forms=25 offsets=4 selectors=4 sst_sxm=4 patterns=4 compatibility=2 restore=38400 duplicate_writes=0 destination_reads=0",
    "TMS320C54x staged shifted STL/STH: PASS cases=172032 forms=7 offsets=4 selectors=4 shifts=32 sst_sxm=4 patterns=4 restore=172032 duplicate_writes=0 destination_reads=0 silicon_phase_claim=0",
    "TMS320C54x staged long MVDK: PASS cases=38400 forms=25 offsets=4 destinations=12 values=4 compatibility=2 restore=38400 source_alias=1 zero_gap_cmpr=1 duplicate_reads=0 duplicate_writes=0",
    "TMS320C54x staged original long MVDK pair: PASS cases=8 restore=8 stl_write=7 source_read=8 ar2_write=8 zero_gap_cmpr=1 duplicate_writes=0",
    "TMS320C54x staged TC branches: PASS cases=1536 forms=BC,BCD conditions=TC,NTC producers=BIT,CMPR_EQ,status_bit,CMPR_LT gaps=0,1,2 pages=2 restore=1536 post_evaluate_change=0,1 duplicate_fetches=0",
    "TMS320C54x staged long RMW: PASS cases=153600 operations=ADDM,ANDM,ORM forms=25 offsets=4 value_pairs=8 status_states=8 compatibility=2 restore=153600 duplicate_reads=0 duplicate_writes=0",
    "TMS320C54x staged memory ADD/SUB: PASS cases=110592 forms=18 registers=8 selectors=4 patterns=8 sxm_ovm=4 compatibility=2 restore=110592 duplicate_reads=0 data_writes=0",
    "TMS320C54x staged mapped AR arithmetic: PASS cases=1024 registers=8 selectors=4 sxm_ovm=4 patterns=2 forms=direct,absolute restore=1024 duplicate_reads=0 data_writes=0",
    "TMS320C54x staged signed branches: PASS cases=12288 conditions=GE,LT,NE,EQ,GT,LE accumulators=A,B forms=BC,BCD patterns=8 sxm_ovm=4 pages=2 restore=12288 post_evaluate_change=0,1 duplicate_fetches=0",
    "TMS320C54x staged signed branch producer: PASS cases=12 counts=0,1,2,65535 restore=12 ldm_read=5 sub_read=7 target_fetch=10 duplicate_reads=0 data_writes=0",
    "TMS320C54x staged BITF: PASS cases=55296 forms=18 registers=8 mask_pairs=8 initial_tc=2 sxm_ovm=4 compatibility=2 restore=55296 duplicate_reads=0 data_writes=0",
    "TMS320C54x staged mapped BITF branch: PASS cases=96 source=AR7 masks=0001,007f conditions=TC,NTC initial_tc=2 restore=96 source_read=7 target_fetch=10 duplicate_reads=0 data_writes=0",
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
BUS_REGISTER_REQUIRED = "TMS320C54x C5416 bus registers: PASS encodings=65536 saved_restore=1 reset=1 partial_writes=1 clkout_system_independent=1 logical_ack_with_iackoff=1 bus_timing_acceptance=0 physical_mapping=unvalidated"
BUS_REGISTER_COMPLETION = "Fatal error: TMS320C54x isolated C5416 bus registers complete"
BUS_IO_REQUIRED = "TMS320C54x staged external I/O waits: PASS cases=576 restore=576 software_waits=0..14 ready_threshold=2 directions=read,write sources=none,IRQ,NMI timer_continues=1 duplicate_io=0 atomic_acceptance=separate physical_mapping=unvalidated"
BUS_IO_COMPLETION = "Fatal error: TMS320C54x isolated staged external I/O complete"
ATOMIC_IO_REQUIRED = BUS_IO_REQUIRED.replace("staged external", "atomic external").replace("cases=576 restore=576", "cases=2880 restore=2880 forms=short,offset,absolute,peripheral,AR_alias").replace(" atomic_acceptance=separate", "")
ATOMIC_IO_COMPLETION = BUS_IO_COMPLETION.replace("staged external", "atomic external")
COMPOSE_REQUIRED = "TMS320C54x atomic I/O composition: PASS cases=24 restore=24 contexts=repeat,delay,INTM pointer_updates=once interrupt_after_transfer=1 physical_mapping=unvalidated"
COMPOSE_COMPLETION = "Fatal error: TMS320C54x isolated atomic I/O composition complete"
BUS_STM_REQUIRED = "TMS320C54x staged bus STM: PASS cases=36 restore=36 registers=SWWSR,BSCR,SWCR literal_fetches=1 writes=1 destination_reads=0 publish_clock=8 peripheral_extra_clock=1 physical_mapping=unvalidated"
BUS_STM_COMPLETION = "Fatal error: TMS320C54x isolated staged bus STM complete"
READ_A_MAPPED_REQUIRED = "TMS320C54x mapped READA: PASS cases=288 restore=288 registers=AL,AH,AG,BL,BH,BG forms=direct,absolute source_pages=2 source_reads=1 writes=1 status_preserved=1 silicon_phase_claim=0"
READ_A_MAPPED_COMPLETION = "Fatal error: TMS320C54x isolated mapped READA complete"
DP_ABSOLUTE_REQUIRED = "TMS320C54x staged absolute memory DP: PASS pages=512 cases=1536 restore=1536 source_read=6 producer_execute=9 consumer_execute=10 extension_fetches=1 duplicate_reads=0 silicon_phase_claim=0"
DP_ABSOLUTE_COMPLETION = "Fatal error: TMS320C54x isolated absolute memory DP complete"
EXTRA_PROFILES = {
    "logic-edges": (
        "TMS320C54x long logic exclusions: PASS cases=24 restore=24 mapped_peripheral=12 repeats=6 control_delays=6 data_reads=0 data_writes=0 pointer_updates=0 atomic_fallback=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated long logic exclusions complete",
    ),
    "logic-short": (
        "TMS320C54x staged memory logic: PASS cases=576 operations=3 accumulators=2 address_forms=4 value_pairs=4 sxm_states=2 restore=576 duplicate_reads=0",
        "Fatal error: TMS320C54x isolated short memory logic complete",
    ),
    "logic-peripheral": (
        "TMS320C54x staged peripheral logic: PASS cases=864 operations=AND,OR,XOR addressing_modes=3 operands=3 accumulators=2 sxm_states=2 pages=2 restore=864 extra_cycles=1 duplicate_reads=0 peripheral_writes=0",
        "Fatal error: TMS320C54x isolated peripheral memory logic complete",
    ),
    "load-offset": (
        "TMS320C54x staged long-offset LD: PASS ram_cases=12288 mapped_ar_cases=384 forms=offset,preincrement,circular,absolute registers=8 accumulators=2 offsets=8 sxm_ovm=4 compatibility=2 restore=12672 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated offset memory load complete",
    ),
    "logic-offset": (
        "TMS320C54x staged long memory logic: PASS cases=294912 restore=294912 operations=AND,OR,XOR accumulators=2 modes=24 offsets=4 value_pairs=4 sxm_ovm=4 cmpt=2 bk=0,13 pages=2 snapshots=4 literal_fetches=1 source_reads=1 data_writes=0 pointer_updates_once=1 native_offset1462=1 mapped_acceptance=0 repeat_acceptance=0 irq_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated long memory logic complete",
    ),
    "bacc": (
        "TMS320C54x staged BACC: PASS cases=216 restore=216 accumulators=2 caller_pages=3 upper_targets=3 snapshots=4 target_sample=5 target_fetch=8 target_execute=12 sources=none,IRQ,NMI frame_words=0 target_retained=1 sequential_flushed=1 delayed_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated staged BACC complete",
    ),
    "fbacc": (
        "TMS320C54x staged FBACC: PASS cases=216 restore=216 accumulators=2 caller_pages=3 upper_targets=3 snapshots=4 target_sample=5 target_fetch=8 target_execute=12 sources=none,IRQ,NMI frame_words=0 target_retained=1 sequential_flushed=1 delayed_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated staged FBACC complete",
    ),
    "bacc-producer": (
        "TMS320C54x staged BACC producer: PASS cases=432 restore=432 producers=DLD,READA accumulators=2 gaps=0,1,2 caller_pages=3 upper_targets=3 snapshots=4 frame_words=0 target_retained=1 mapped_publication=1 delayed_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated BACC producer complete",
    ),
    "fbacc-producer": (
        "TMS320C54x staged FBACC producer: PASS cases=432 restore=432 producers=DLD,READA accumulators=2 gaps=0,1,2 caller_pages=3 upper_targets=3 snapshots=4 frame_words=0 target_retained=1 mapped_publication=1 delayed_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated FBACC producer complete",
    ),
    "popd-edges": (
        "TMS320C54x POPD edges: PASS cases=31 restore=31 exclusions=15 dagen_conflicts=16 conflict_read=6 conflict_write=7 same_and_different_registers=1 active_controls_rejected=4 memory_count_rejected=1 source_capture=1 destination_capture=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated POPD edges complete",
    ),
    "popd-ram": (
        "TMS320C54x staged POPD: PASS cases=50 address_forms=4 compiler_latency=6 original_copy_words=6 copy_cases=12 copy_sxm_states=2 restore=50 duplicate_stack_reads=0 duplicate_writes=0",
        "Fatal error: TMS320C54x isolated RAM POPD complete",
    ),
    "popd-mapped": (
        "TMS320C54x mapped POPD: PASS cases=4032 restore=4032 registers=21 forms=direct,postincrement,postdecrement patterns=4 cmpt=2 pages=2 snapshots=4 reads=1 writes=1 captured_destination=1 captured_source=1 dagen_write=Read data_write=Execute active_block_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated mapped POPD complete",
    ),
    "popd-repeat": (
        "TMS320C54x repeated POPD: PASS cases=6528 restore=6528 layouts=RAM,fixed,forward_alias,back_alias,PMST_to_BK,AR6_to_BL forms=short,long counts=0,1,15,4,11 patterns=4 cmpt=2 pages=2 snapshots=4 sources=none,IRQ,NMI reads_each=1 writes_each=1 body_fetches=1 literal_fetches=once read_before_older_write=1 interrupt_return=1 active_block_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated repeated POPD complete",
    ),
    "logic-absolute": (
        "TMS320C54x staged absolute logic: PASS cases=1536 restore=1536 operations=AND,OR,XOR accumulators=2 addresses=4 values=4 sxm_states=2 pages=2 snapshots=4 literal_fetches=1 source_reads=1 data_writes=0 retained_address=1 retained_source=1 mapped_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated absolute logic complete",
    ),
    "mvdd": (
        "TMS320C54x staged MVDD: PASS cases=49152 restore=49152 encodings=256 layouts=distinct,equal,overlap values=4 bk=0,16 pages=2 snapshots=4 address_access=4 source_read=5 destination_write=6 same_ar_xmod_priority=1 captured_addresses=1 captured_source=1 reads=1 writes=1 mapped_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated staged MVDD complete",
    ),
    "mvdd-repeat": (
        "TMS320C54x repeated MVDD: PASS cases=221184 restore=221184 encodings=256 counts=0,1,15 forms=short,long layouts=distinct,equal,overlap bk=0,16 pages=2 snapshots=4 sources=none,IRQ,NMI read_before_older_write=1 same_ar_xmod_priority=1 opcode_fetches=1 literal_fetches=once issue_interval=1 interrupt_return=1 mapped_acceptance=0 memory_count_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated repeated MVDD complete",
    ),
    "cala": (
        "TMS320C54x staged CALA: PASS cases=216 restore=216 accumulators=2 caller_pages=3 ignored_upper_targets=3 target_sample=5 stack_write=5 target_fetch=8 callee_execute=12 sources=none,IRQ,NMI frame_words=1 frame_roundtrip=1 delayed_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated staged CALA complete",
    ),
    "cala-producer": (
        "TMS320C54x staged CALA producer: PASS cases=216 restore=216 producer=DLD accumulators=2 gaps=0,1,2 caller_pages=3 ignored_upper_targets=3 ram_reads=2 target_retained=1 frame_words=1 frame_roundtrip=1 delayed_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated CALA producer complete",
    ),
    "xmem-load": (
        "TMS320C54x staged Xmem LD: PASS cases=196608 restore=196608 encodings=512 xmem_modes=16 shifts=0..15 values=8 accumulators=2 status_states=8 pages=2 snapshots=3 source_read=5 result_publish=6 pointer_access=4 captured_source=1 bk_zero_and_wrap=1 reads=1 data_writes=0 mapped_acceptance=0 repeat_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated Xmem LD complete",
    ),
    "adds": (
        "TMS320C54x staged ADDS: PASS cases=55296 restore=55296 forms=18 registers=8 accumulators=2 patterns=8 sxm_ovm=4 compatibility=2 source_unsigned=1 reads=1 data_writes=0 mapped_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated staged ADDS complete",
    ),
    "exp-t": (
        "TMS320C54x staged EXP T: PASS cases=12288 restore=12288 accumulators=2 vectors=16 gaps=0,1,2 status_states=8 producers=none,LDU pages=2 snapshots=4 publish=Execute adjacent_T=old gap_T=new retained_consumer=1 flags_preserved=1 data_writes=0 norm_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated EXP T complete",
    ),
    "pshd-repeat": (
        "TMS320C54x repeated PSHD: PASS cases=768 restore=768 counts=0,1,4,11 forms=short,long sources=RAM_up,RAM_down,MMR_up,MMR_down pages=2 interrupts=none,IRQ,NMI snapshots=4 reads_each=1 writes_each=1 issue_interval=1 opcode_fetches=1 pending_irq_deferred=1 isr_return=1 excluded_aliases=2 memory_count_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated repeated PSHD complete",
    ),
    "pshd-mapped": (
        "TMS320C54x mapped PSHD: PASS cases=288 restore=288 registers=12 patterns=4 pages=2 snapshots=3 address_access=4 source_read=5 stack_write=6 reads=1 writes=1 source_retained=1 repeat_coverage=0 dagen_source_coverage=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated mapped PSHD complete",
    ),
    "timer-stm": (
        "TMS320C54x staged timer STM: PASS cases=36 restore=36 registers=TIM,PRD,TCR literal_fetches=1 writes=1 destination_reads=0 publish_clock=8 peripheral_extra_clock=1 stopped_timer=1 trb_reload=1 psc_write_mask=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated staged timer STM complete",
    ),
    "cmpm-direct": (
        "TMS320C54x staged direct CMPM: PASS cases=384 restore=384 layouts=DP,SP values=4 literals=2 initial_tc=2 conditions=TC,NTC pages=2 snapshots=3 source_read=6 tc_publish=7 target_fetch=10 reads=1 writes=0 literal_fetches=1 address_retained=1 mapped_source_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated direct CMPM complete",
    ),
    "ldu-st1": (
        "TMS320C54x staged LDU ST1: PASS cases=288 restore=288 forms=direct,absolute,indirect values=8 accumulators=2 pages=2 snapshots=3 register_reads=1 writes=0 source_retained=1 unsigned_full_word=1 producer_latency_claim=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated LDU ST1 complete",
    ),
    "stack-irq-return": (
        "TMS320C54x staged stack IRQ return: PASS cases=80 restore=80 forms=PSHM,PSHD,POPM_T,POPM_SP sources=IRQ,NMI pages=2 snapshots=5 acknowledgements=1 return_reads=1 caller_cleanup=push_forms resumed_idle=1 intm_cleared=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated stack IRQ return complete",
    ),
    "stack-irq": (
        "TMS320C54x staged stack IRQ ordering: PASS cases=48 restore=48 forms=PSHM,PSHD,POPM_T,POPM_SP sources=IRQ,NMI pages=2 snapshots=3 stack_read=5 interrupt_reserve=5 foreground_write=6 interrupt_write=7 acknowledgements=1 older_first=1 isr_return_claim=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated stack IRQ ordering complete",
    ),
    "pop-brc": (
        "TMS320C54x staged POPM BRC repeat: PASS cases=30 restore=30 counts=0,1,2,7,127 pages=2 snapshots=3 stack_reads=1 counter_reads=0 counter_writes=1 gap_nops=1 source_retained=1 count_plus_one=1 active_repeat_write_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated POPM BRC repeat complete",
    ),
    "pop-repeat": (
        "TMS320C54x staged repeat-control POPM: PASS cases=192 restore=192 registers=BRC,RSA,REA values=4 source_pages=2 target_regions=2 snapshots=4 stack_reads=1 writes=1 source_retained=1 active_repeat_acceptance=0 excluded_active=3 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated repeat-control POPM complete",
    ),
    "pmst-iptr": (
        "TMS320C54x staged PMST IPTR: PASS cases=1024 restore=1024 values=4 source_pages=2 irq_bits=16 irq_timings=2 snapshots=4 vector_prefetch_owned=1 pending_latch_fixture=1 isr_return_claim=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated PMST IPTR complete",
    ),
    "pmst-prefetch": (
        "TMS320C54x staged PMST prefetch ownership: PASS cases=64 restore=64 values=4 source_pages=2 target_regions=2 snapshots=4 debugger_state_fixture=1 firmware_ordering_claim=0",
        "Fatal error: TMS320C54x isolated PMST prefetch complete",
    ),
    "pmst-pop": (
        "TMS320C54x staged POPM PMST: PASS cases=64 restore=64 values=4 source_pages=2 target_regions=2 stack_reads=1 writes=1 source_retained=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated POPM PMST complete",
    ),
    "pmst-map": (
        "TMS320C54x staged STM PMST: PASS cases=64 values=4 source_pages=2 target_regions=2 restore=64 duplicate_literal_fetches=0 duplicate_register_writes=0",
        "Fatal error: TMS320C54x isolated PMST mapping complete",
    ),
    "stlm-imr": (
        "TMS320C54x staged STLM IMR: PASS cases=9216 restore=9216 accumulators=2 irq_bits=16 masks=4 initial_masks=2 intm=2 sources=none,IRQ,NMI pages=2 snapshots=3 source_execute_live=1 writes_each=1 destination_reads=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated STLM IMR complete",
    ),
    "mcbsp-control": (
        "TMS320C54x staged McBSP control: PASS cases=16 restore=16 pages=2 sxm_states=2 snapshots=4 selector_writes=1 status_reads=1 reset_writes=1 captured_ready=1 busy_after_read=1 device_reset=1 physical_mapping=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated McBSP control complete",
    ),
    "rmw-long-periph": (
        "TMS320C54x staged long peripheral RMW: PASS cases=368640 restore=368640 operations=ADDM,ANDM,ORM registers=8 forms=offset,preincrement offsets=3b,39,49 spans=peripheral,RAM value_pairs=8 status_states=8 compatibility=2 pages=2 snapshots=5 reads=1 writes=1 literal_fetches=2 peripheral_extra=1 ram_extra=0 double_charge=0 device_semantics=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated long peripheral RMW complete",
    ),
    "st-long-periph": (
        "TMS320C54x staged long peripheral ST: PASS cases=6144 restore=6144 registers=8 forms=offset,preincrement values=4 compatibility=2 offsets=3b,38,49 spans=peripheral,RAM pages=2 snapshots=4 literal_fetches=2 reads=0 writes=1 peripheral_extra=1 ram_extra=0 address_retained=1 device_semantics=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated long peripheral ST complete",
    ),
    "preinc-st": (
        "TMS320C54x staged preincrement ST: PASS cases=1536 restore=1536 registers=8 literals=4 sst_sxm=4 compatibility=2 pages=2 snapshots=3 literal_fetches=1 reads=0 writes=1 pointer_updates=1 wrap_imr=1 source_literal_retained=1 single_repeat_acceptance=0 interrupt_composition=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated preincrement ST complete",
    ),
    "stm-brc": (
        "TMS320C54x staged STM BRC: PASS cases=30 restore=30 counts=0,1,2,7,127 pages=2 snapshots=3 literal_fetches=1 counter_reads=0 counter_writes=1 adjacent_rptb=1 latency=0 in_loop_acceptance=0 interrupt_composition=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated STM BRC complete",
    ),
    "preinc-store": (
        "TMS320C54x staged preincrement store: PASS cases=6144 restore=6144 selectors=STL_A,STL_B,STH_A,STH_B registers=8 patterns=4 sst_sxm=4 compatibility=2 pages=2 snapshots=3 reads=0 writes=1 pointer_updates=1 wrap_imr=1 source_execute_live=1 interrupt_composition=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated preincrement store complete",
    ),
    "ldu-imr": (
        "TMS320C54x staged LDU IMR: PASS cases=384 restore=384 forms=direct,absolute,indirect,MVDK values=4 sxm_states=2 accumulators=2 pages=2 snapshots=3 imr_reads=1 producer_reads=1 producer_writes=1 source_retained=1 interrupt_composition=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated LDU IMR complete",
    ),
    "memory-shift16": (
        "TMS320C54x staged memory shift16: PASS cases=221184 restore=221184 forms=18 registers=8 selectors=8 patterns=8 sxm_ovm=4 compatibility=2 reads=1 data_writes=0 partial_carry=1 mapped_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated memory shift16 complete",
    ),
    "mvmd-status-irq": (
        "TMS320C54x staged MVMD status IRQ: PASS cases=576 restore=576 sources=IRQ,NMI intm_transitions=4 imr_states=2 arrivals=4,6,8 pages=2 snapshots=3 followers=STLM,STM source_reads=1 status_writes=1 accepted_frame=1 masked_frame=0 foreground_once=1 rsbx_guard_inherited=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated MVMD status IRQ complete",
    ),
    "mvmd-cmpt": (
        "TMS320C54x staged MVMD CMPT: PASS cases=16 restore=16 directions=2 consumers=LD,ST gaps=0,1,2,3 latency=2 pointer_updates=1 interrupt_composition=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated MVMD CMPT complete",
    ),
    "mvmd-block": (
        "TMS320C54x staged MVMD block: PASS cases=24 restore=24 counts=0,1,2,7 pages=2 snapshots=3 adjacent_rptb=1 braf_preserved=1 interrupt_composition=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated MVMD block complete",
    ),
    "mvmd-status-alu": (
        "TMS320C54x staged MVMD status ALU: PASS cases=768 restore=768 consumers=SXM_LD,ASM_LD sxm_states=2 asm_values=32 pages=2 snapshots=3 source_reads=1 status_writes=1 latency=0 interrupt_composition=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated MVMD status ALU complete",
    ),
    "mvmd-cpl": (
        "TMS320C54x staged MVMD CPL: PASS cases=16 restore=16 directions=2 consumers=LD,ST gaps=0,1,2,3 latency=2 interrupt_composition=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated MVMD CPL complete",
    ),
    "mvmd-st1": (
        "TMS320C54x staged MVMD ST1: PASS cases=288 restore=288 sources=AL,BL producers=POPM,STL gaps=0,1,2 pages=2 values=4 source_reads=1 status_writes=1 destination_reads=0 consumer_latency=unvalidated interrupt_composition=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated MVMD ST1 complete",
    ),
    "mvmd-acc": (
        "TMS320C54x staged MVMD accumulator: PASS cases=288 restore=288 sources=AL,BL producers=POPM,STL gaps=0,1,2 pages=2 values=4 source_reads=1 ram_writes=1 mapped_no_conflict=1 status_destination=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated MVMD accumulator complete",
    ),
    "subs": (
        "TMS320C54x staged SUBS: PASS cases=55296 restore=55296 forms=18 registers=8 accumulators=2 patterns=8 sxm_ovm=4 compatibility=2 source_unsigned=1 reads=1 data_writes=0 mapped_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated staged SUBS complete",
    ),
    "conditional-ret": (
        "TMS320C54x staged conditional return: PASS cases=768 restore=768 conditions=AGEQ,ALT,ANEQ,AEQ,AGT,ALEQ,BGEQ,BLT,BNEQ,BEQ,BGT,BLEQ patterns=8 pages=2 snapshots=4 taken_reads=1 data_writes=0 untaken_stack_accesses=0 predicate_retained=1 stack_target_retained=1 delayed_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated conditional return complete",
    ),
    "conditional-call": (
        "TMS320C54x staged conditional call: PASS cases=64 restore=64 conditions=AEQ,BEQ patterns=4 pages=2 literal_fetches=1 taken_frame=1 untaken_stack_accesses=0 predicate_retained=1 delayed_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated conditional call complete",
    ),
    "fcala-producer": (
        "TMS320C54x staged FCALA producer: PASS cases=216 restore=216 producer=DLD accumulators=2 gaps=0,1,2 caller_pages=3 target_pages=3 ram_reads=2 target_retained=1 frame_roundtrip=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated FCALA producer complete",
    ),
    "fcala": (
        "TMS320C54x staged FCALA: PASS cases=216 restore=216 accumulators=2 caller_pages=3 target_pages=3 target_sample=5 stack_writes=5,6 target_fetch=8 callee_execute=12 sources=none,IRQ,NMI frame_roundtrip=1 producer_latency=unvalidated irq_composition=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated staged FCALA complete",
    ),
    "far-atomic": (
        "TMS320C54x atomic far transfers: PASS encodings=16 caller_page=1 target_page=2 call_return_frame=1 delayed_target_retained=1 staged_acceptance=0 save_replay=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated atomic far transfers complete",
    ),
    "bitf-peripheral": (
        "TMS320C54x staged peripheral BITF: PASS cases=384 restore=384 forms=offset,absolute pages=2 conditions=TC,NTC source_read=8 tc_publish=9 target_fetch=11 reads=1 destination_writes=0 peripheral_extra_clock=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated peripheral BITF complete",
    ),
    "t-peripheral": (
        "TMS320C54x staged peripheral T/TRN store: PASS cases=864 restore=864 sources=T,TRN producer=STLM gaps=0,1,2 forms=indirect,postincrement destinations=3b,48,49 pages=2 destination_reads=0 writes=1 peripheral_extra_clock=1 pointer_updates=once silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated peripheral T/TRN store complete",
    ),
    "mvkd-peripheral": (
        "TMS320C54x staged peripheral MVKD: PASS cases=648 restore=648 sources=AR,BK,RAM forms=indirect,postincrement,absolute destinations=3b,48,49 pages=2 reads=1 writes=1 peripheral_extra_clock=1 pointer_updates=once bk_producer_latency=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated peripheral MVKD complete",
    ),
    "cmpm-absolute": (
        "TMS320C54x staged absolute CMPM: PASS cases=96 restore=96 source=RAM conditions=TC,NTC initial_tc=2 source_read=7 target_fetch=10 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated absolute CMPM complete",
    ),
    "ifr-pair": (
        "TMS320C54x staged IFR pair: PASS cases=96 restore=96 accumulators=2 patterns=4 pending_only=1 late_irq=1 accepted_irq=1 nmi=1 interrupt_return=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated IFR pair complete",
    ),
    "mvmd-ar": (
        "TMS320C54x staged MVMD AR: PASS cases=576 restore=576 registers=8 destinations=RAM,self_AR,other_AR pages=2 source_read=5 ram_write=6 mmr_write=5 literal_fetches=1 reads=1 writes=1 status_preserved=1 repeat_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated MVMD AR complete",
    ),
    "mvkd-delay": (
        "TMS320C54x staged MVKD delay: PASS cases=1728 restore=1728 registers=8 forms=AR_source,postincrement,same_RAM outcomes=taken,untaken sources=none,IRQ,NMI source_read=9 ram_write=10 literal_fetches=2 reads=1 writes=1 interrupt_return=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated MVKD delay complete",
    ),
    "mac-smem": (
        "TMS320C54x staged MAC/MAS: PASS cases=73728 restore=73728 operations=MAC,MACR,MAS,MASR destinations=A,B forms=indirect_RAM,absolute_RAM,mapped_AR1 pairs=64 flags=16 source_reads=1 data_writes=0 t_retained=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated MAC/MAS complete",
    ),
    "far-call-delay": (
        "TMS320C54x staged FCALLD: PASS cases=243 restore=243 caller_pages=3 target_pages=3 layouts=probe_XPC_STLM,STM,original_NOP_LDM sources=none,IRQ,NMI frame_words=2 complete_delays=2 return_roundtrip=1 xpc_delay_visibility=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated delayed far call complete",
    ),
    "mvdm-delay": (
        "TMS320C54x staged MVDM delay: PASS cases=576 restore=576 registers=8 outcomes=taken,untaken sources=none,IRQ,NMI literal_fetches=2 reads=1 writes=1 interrupt_return=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated MVDM delay complete",
    ),
    "mvdm-ar": (
        "TMS320C54x staged MVDM AR: PASS cases=1152 restore=1152 registers=8 sources=RAM,self_AR,other_AR gaps=0,1 pages=2 literal_fetches=1 reads=1 writes=1 repeat_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated MVDM AR complete",
    ),
}


def check_result(text, returncode, *, bus_registers=False, bus_io=False, atomic_io=False, compose=False, bus_stm=False, mapped_read_a=False, dp_absolute=False, profile=None):
    scopes = [(bus_registers, BUS_REGISTER_REQUIRED, BUS_REGISTER_COMPLETION),
              (bus_io, BUS_IO_REQUIRED, BUS_IO_COMPLETION),
              (atomic_io, ATOMIC_IO_REQUIRED, ATOMIC_IO_COMPLETION),
              (compose, COMPOSE_REQUIRED, COMPOSE_COMPLETION),
              (bus_stm, BUS_STM_REQUIRED, BUS_STM_COMPLETION),
              (mapped_read_a, READ_A_MAPPED_REQUIRED, READ_A_MAPPED_COMPLETION),
              (dp_absolute, DP_ABSOLUTE_REQUIRED, DP_ABSOLUTE_COMPLETION)]
    selected = [(marker, ending) for enabled, marker, ending in scopes if enabled]
    if profile is not None:
        if profile not in EXTRA_PROFILES:
            raise ValueError(f"unknown isolated profile: {profile}")
        selected.append(EXTRA_PROFILES[profile])
    if len(selected) > 1:
        raise ValueError("choose one isolated conformance scope")
    if returncode != 3:
        raise ValueError(f"unexpected MAME exit status {returncode}; expected test completion exit 3")
    lines = text.splitlines()
    if "Errors:" in lines:
        raise ValueError("MAME reported driver validity errors")
    required, completion = ([selected[0][0]], selected[0][1]) if selected else (REQUIRED, COMPLETION)
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
    args.output_root.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix="run_c54x_core_", dir=args.output_root.resolve()))
    command = [str(args.mame.resolve()), "tms54test", "-rompath", str(args.rompath.resolve()),
               "-video", "none", "-sound", "none", "-nothrottle", "-noreadconfig",
               # This is a stop bound, not acceptance: every marker and the
               # intentional test-completion exit remain mandatory.
               "-log", "-seconds_to_run", "5"]
    if args.bus_registers:
        command.extend(["-bios", "bus-registers"])
    if args.bus_io:
        command.extend(["-bios", "bus-io"])
    if args.atomic_io:
        command.extend(["-bios", "bus-io-atomic"])
    if args.compose:
        command.extend(["-bios", "bus-io-compose"])
    if args.bus_stm:
        command.extend(["-bios", "bus-stm"])
    if args.mapped_read_a:
        command.extend(["-bios", "read-a-mapped"])
    if args.dp_absolute:
        command.extend(["-bios", "dp-absolute"])
    if args.profile:
        command.extend(["-bios", args.profile])
    print(f"C54x evidence: {run}", flush=True)
    with (run / "console.log").open("w") as output:
        result = subprocess.run(command, cwd=run, stdout=output, stderr=subprocess.STDOUT)
    text = (run / "console.log").read_text()
    print(text, end="")
    try:
        check_result(text, result.returncode, bus_registers=args.bus_registers, bus_io=args.bus_io, atomic_io=args.atomic_io, compose=args.compose, bus_stm=args.bus_stm, mapped_read_a=args.mapped_read_a, dp_absolute=args.dp_absolute, profile=args.profile)
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
