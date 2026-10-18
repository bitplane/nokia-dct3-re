#!/usr/bin/env python3
"""Isolated native C54x conformance run; intentional completion exits with code 3."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

IMMEDIATE_MAPPED_REQUIRED = (
    "TMS320C54x original byte-count prefix: PASS cases=16 restore=16 words=13 sizes=8 pages=2 mapped_reads=2 data_writes=3 silicon_phase_claim=0",
    "TMS320C54x immediate ALU mapped consumer: PASS cases=960 restore=960 operations=ADD,SUB,AND,OR,XOR accumulators=2 parts=3 patterns=4 gaps=0,1 sxm=2 pages=2 reads=1 writes=0 silicon_phase_claim=0",
)
DST_BYPASS_REQUIRED = (
    "TMS320C54x original escape store/load: PASS cases=128 restore=128 lengths=8 stale_values=4 sxm=2 pages=2 mapped_reads=BL,AL writes=0,5 backing_reload_reads=0 silicon_phase_claim=0",
    "TMS320C54x DST read bypass: PASS cases=4608 restore=4608 accumulators=2 consumers=high,low,pair modes=3 patterns=4 gaps=0,1 aliases=2 sxm=2 sst=2 pages=2 save_points=2 writes_each=2 silicon_phase_claim=0",
)

REQUIRED = [
    *IMMEDIATE_MAPPED_REQUIRED,
    *DST_BYPASS_REQUIRED,
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
    "TMS320C54x staged indirect loads: PASS cases=6 ar_update=Access read=Read execute=Execute captured_address_restore=6",
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
    "TMS320C54x staged far return: PASS cases=72 plain=24 delayed=48 multiword_delay=24 preceding_popm=36 restore=72 duplicate_reads=0 silicon_phase_claim=0",
    "TMS320C54x staged cross-page far IRQ: PASS cases=8 masked=4 restored_vector_page=1 continuations=0700,0702 restore=8 duplicate_vectors=0 silicon_phase_claim=0",
    "TMS320C54x staged INTM protection: PASS cases=12 follower_words=1,2,3 adjacent=6 restore=12 duplicate_vectors=0",
    "TMS320C54x staged status bits: PASS cases=128 registers=2 bits=16 operations=2 initial_patterns=2 restore=128 xf_output=1 reserved_st1_bit10=0",
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
    "TMS320C54x staged logical immediates: PASS cases=1536 operations=AND,OR,XOR selectors=4 shifts=0,1,7,15,16 restore=1536 duplicate_literals=0",
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
    "TMS320C54x staged LDU: PASS ram_cases=52224 mapped_ar_cases=2304 address_forms=17 registers=8 accumulators=2 values=8 sxm_ovm=4 compatibility=2 restore=54528 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged memory LD shift16: PASS ram_cases=52224 mapped_ar_cases=2304 address_forms=17 registers=8 accumulators=2 values=8 sxm_ovm=4 compatibility=2 restore=54528 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged variable memory LD: PASS cases=104448 address_forms=17 registers=2 accumulators=2 values=4 shifts=32 sxm_ovm=4 restore=104448 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged MPYU: PASS ram_cases=27648 mapped_cases=24576 accumulators=2 t_values=2 patterns=8 flag_states=16 restore=52224 t_retained=1 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged MPY and MPYR: PASS cases=52224 accumulators=2 rounding=0,1 t_retained=1 restore=52224 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged memory LD T: PASS cases=52224 ram_and_mapped=1 restore=52224 read_publication=1 retirement_rewrite=0 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged LD T multiply pair: PASS cases=768 restore=768 t_read=6 multiply_read=8 publication=9 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged shifted memory ADD/SUB: PASS ram_cases=147456 mapped_ar_cases=98304 selectors=8 shifts=32 sxm_ovm=4 patterns=4 restore=245760 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged SUBC: PASS ram_cases=18432 mapped_ar_cases=12288 value_pairs=64 sxm_ovm=4 accumulators=2 restore=30720 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged repeat SUBC: PASS cases=38400 forms=5 rpt=short,long counts=0,1,15,32,255 sources=none,IRQ,NMI pages=2 restore=38400 count_plus_one=1 divisor_reads_each_iteration=1 duplicate_body_fetches=0 silicon_phase_claim=0",
    "TMS320C54x staged repeat immediate ST: PASS cases=38400 forms=5 rpt=short,long counts=0,1,15,32,255 values=4 sst_sxm_ovm=8 sources=none,IRQ,NMI pages=2 restore=38400 count_plus_one=1 literal_fetches=1 body_fetches=1 destination_reads=0 cycles_each=2 silicon_phase_claim=0",
    "TMS320C54x staged BANZD: PASS cases=3072 registers=8 values=4 modifications=3 cmpt_states=2 pages=2 delay_forms=two_single,one_double restore=3072 duplicate_target_fetches=0 data_accesses=0 silicon_phase_claim=0",
    "TMS320C54x staged STLM T/TRN: PASS cases=12288 sources=2 patterns=8 sst_sxm_ovm_frct=16 gaps=0,1,2,3 consumers=MPY,MPYR,MPYU,LDM restore=12288 register_write=Execute consumer_sample=Read zero_gap_claim=projection_only duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged STLM BRC: PASS cases=3072 sources=2 patterns=8 flags=16 gaps=0,1,2,3 restore=3072 register_write=Execute ldm_sample=Read block_repeat_claim=0 duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged original swap body: PASS cases=1536 patterns=16 flags=16 pages=2 restore=1536 read=Read write=Execute ar6_update=Read duplicate_reads=0 duplicate_writes=0 block_repeat_claim=0 silicon_phase_claim=0",
    "TMS320C54x staged original swap join: PASS cases=1536 patterns=16 flags=16 pages=2 restore=1536 bodies=2 gap_words=0 reads=2 writes=2 ar6_updates=2 block_repeat_claim=0 silicon_phase_claim=0",
    "TMS320C54x staged RPTB: PASS cases=6144 irq_nmi_cases=24 restore=6168 counts=0,1,2,7 zero_overhead_body=5 counter=Decode final_braf=Decode silicon_phase_claim=0",
    "TMS320C54x staged RPTB edges: PASS cases=16 restore=16 one_word=2 two_word=2 exclusions=12 atomic_fallback=0 silicon_phase_claim=0",
    "TMS320C54x staged immediate MPY: PASS isolated_cases=12288 stlm_cases=73728 value_pairs=64 selectors=4 flags=16 restore=86016 literal_fetches=1 t_retained=1 data_reads=0 projection_only_stlm_gap=0 silicon_phase_claim=0",
    "TMS320C54x staged indirect DLD: PASS cases=23040 forms=15 registers=8 accumulators=2 patterns=4 sxm=0,1 c16=0,1 compatibility=0,1 restore=23040 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged long-offset BANZ: PASS cases=2304 forms=offset,preincrement,circular registers=8 offset_pairs=8 compatibility=0,1 pages=2 restore=2304 offset_fetches=1 target_fetches=1 data_accesses=0 silicon_phase_claim=0",
    "TMS320C54x staged immediate arithmetic: PASS sub_cases=26112 add16_cases=1536 producer_cases=144 selectors=4 shifts=0..16 patterns=8 status_states=8 pages=2 restore=27792 duplicate_literals=0 data_accesses=0 silicon_phase_claim=0",
    "TMS320C54x staged register arithmetic: PASS cases=196608 producer_cases=288 operations=ADD,SUB selectors=4 shifts=-16..15 shift_sources=opcode,ASM patterns=8 status_states=8 pages=2 restore=196896 duplicate_opcodes=0 data_accesses=0 silicon_phase_claim=0",
    "TMS320C54x staged register LD: PASS cases=196608 producer_cases=144 selectors=4 shifts=-16..15 shift_sources=opcode,ASM patterns=8 status_states=16 pages=2 restore=196752 duplicate_opcodes=0 data_accesses=0 carry_preserved=1 silicon_phase_claim=0",
    "TMS320C54x staged memory multiply: PASS ram_cases=27648 mapped_cases=24576 operations=MPYA,MASA,MACA,MACAR patterns=8 flag_states=16 restore=52224 t_read_owned=1 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged accumulator logic: PASS cases=6144 operations=AND,OR,XOR selectors=4 shifts=-16..15 patterns=4 sxm=0,1 restore=6144 duplicate_fetches=0 data_accesses=0",
    "TMS320C54x staged CMPL: PASS cases=64 selectors=4 patterns=4 sxm=0,1 restore=64 duplicate_fetches=0 data_accesses=0",
    "TMS320C54x staged MVKD: PASS cases=672 forms=DP,CPL,postincrement,postdecrement,preincrement,absolute,AR sources=AR1,RAM,alias,AR2 values=4 pages=2 category_i_gaps=0,1 restore=672 duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged accumulator shifts: PASS cases=32768 operations=SFTA,SFTL selectors=4 shifts=-16..15 patterns=8 sxm=0,1 ovm=0,1 carry=0,1 restore=32768 duplicate_fetches=0 data_accesses=0 left_c_policy=manual_formula silicon_phase_claim=0",
    "TMS320C54x staged DSUB: PASS cases=6144 address_forms=8 destinations=A,B patterns=8 c16=0,1 sxm=0,1 ovm=0,1 parity=0,1 restore=6144 duplicate_reads=0 memory_writes=0 silicon_phase_claim=0",
    "TMS320C54x staged DADD: PASS cases=12288 address_forms=8 selectors=4 patterns=8 c16=0,1 sxm=0,1 ovm=0,1 parity=0,1 restore=12288 duplicate_reads=0 memory_writes=0 silicon_phase_claim=0",
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
    "TMS320C54x staged memory ADD/SUB: PASS cases=104448 forms=17 registers=8 selectors=4 patterns=8 sxm_ovm=4 compatibility=2 restore=104448 duplicate_reads=0 data_writes=0",
    "TMS320C54x staged mapped AR arithmetic: PASS cases=1024 registers=8 selectors=4 sxm_ovm=4 patterns=2 forms=direct,absolute restore=1024 duplicate_reads=0 data_writes=0",
    "TMS320C54x staged signed branches: PASS cases=12288 conditions=GE,LT,NE,EQ,GT,LE accumulators=A,B forms=BC,BCD patterns=8 sxm_ovm=4 pages=2 restore=12288 post_evaluate_change=0,1 duplicate_fetches=0",
    "TMS320C54x staged signed branch producer: PASS cases=12 counts=0,1,2,65535 restore=12 ldm_read=5 sub_read=7 target_fetch=10 duplicate_reads=0 data_writes=0",
    "TMS320C54x staged BITF: PASS cases=52224 forms=17 registers=8 mask_pairs=8 initial_tc=2 sxm_ovm=4 compatibility=2 restore=52224 duplicate_reads=0 data_writes=0",
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
    "TMS320C54x memory multiply/bit disassembler: PASS forms=2304 pages=2 bounded_extensions=1 pipeline_claim=0",
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
    "squr-smem": (
        "TMS320C54x memory SQUR: PASS cases=52224 restore=52224 forms=RAM,mapped accumulators=2 patterns=8 flags=16 t_publication=Read operand_retained=1 newer_t_preserved=1 duplicate_reads=0 data_writes=0 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated memory SQUR complete",
    ),
    "mvmd-rptz": (
        "TMS320C54x RPTZ MVMD: PASS cases=1536 restore=1536 sources=AL,BL clear=A,B counts=0,1,39,255 patterns=4 flags=4 pages=2 snapshots=3 first_read_after_clear=1 opposite_source_unchanged=1 mid_repeat_source_change=1 literal_fetches=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated RPTZ MVMD complete",
    ),
    "rptb-exit-delay": (
        "TMS320C54x delayed forward block exits: PASS cases=9984 restore=9984 branches=BD,BCD_signed12 counts=0,1,2,7 exits=first,middle,last,never endpoint_layouts=2 delay_layouts=2 pages=2 snapshots=6 delay_words_once=1 decision_before_opposite_load=1 braf_not_auto_cleared=1 outside_rsbx=1 spaced_endpoint_revisit=1 interrupt_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated delayed forward block exits complete",
    ),
    "rptbd-t-load": (
        "TMS320C54x RPTBD delay T load: PASS cases=9216 restore=9216 counts=0,1,7 patterns=4 forms=direct,plain,postinc,postdec cpl=2 slots=2 flags=4 accumulators=2 pages=2 snapshots=3 delay_reads=1 first_body_multiply=1 source_and_opcode_poison=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated RPTBD delay T load complete",
    ),
    "mvmd-repeat": (
        "TMS320C54x repeated MVMD: PASS cases=1536 restore=1536 sources=AL,BL counts=0,1,39,255 patterns=4 flags=4 headers=short,long pages=2 snapshots=3 fixed_source_reads=each destination=EAR_increment literal_fetches=1 mid_repeat_source_change=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated repeated MVMD complete",
    ),
    "shift-per-edge": (
        "TMS320C54x shifted peripheral exclusions: PASS cases=64 restore=64 accumulators=A,B pages=2 snapshots=2 classes=repeat,block,XC,delay,ST1,BRC,ADD,SUB data_reads=0 data_writes=0 atomic_fallback=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated shifted peripheral exclusions complete",
    ),
    "portr-circular": (
        "TMS320C54x circular PORTR: PASS cases=98304 restore=98304 modes=4 registers=8 compatibility=2 bk=4 indices=4 samples=4 ports=2 flags=2 pages=2 snapshots=3 literal_retained=1 sample_retained=1 pointer_updates=once destination_reads=0 excluded_io_reads=0 excluded_writes=0 bk_zero=legacy_linear silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated circular PORTR complete",
    ),
    "portr-block": (
        "TMS320C54x circular PORTR block: PASS cases=1728 restore=1728 counts=2,7,17 bk=1,7,16 registers=8 compatibility=2 start_positions=2 pages=2 snapshots=3 source=volatile_io issue_interval=2 sample_retained=1 wrap=1 destination_reads=0 duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated circular PORTR block complete",
    ),
    "shift-unowned": (
        "TMS320C54x shifted unowned low bus: PASS cases=122880 restore=122880 addresses=0002..0005 shifts=32 forms=DP,SP,absolute,postinc,offset flags=SXM,OVM accumulators=A,B patterns=4 pages=2 snapshots=3 extra_clock=0 source_reads=1 data_writes=0 literals_retained=1 pointer_retained=1 endpoint_identity_claim=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated shifted peripheral load complete",
    ),
    "shift-periph": (
        "TMS320C54x shifted peripheral load: PASS cases=245760 restore=245760 addresses=64 shifts=-16,-1,4,15 forms=DP,SP,absolute,postinc,offset flags=SXM,OVM accumulators=A,B patterns=4 pages=2 snapshots=3 extra_clock=1 source_reads=1 data_writes=0 literals_retained=1 pointer_retained=1 endpoint_identity_claim=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated shifted peripheral load complete",
    ),
    "shift-bsp": (
        "TMS320C54x shifted BSP receive: PASS cases=40960 restore=40960 shifts=32 forms=DP,SP,absolute,postinc,offset flags=SXM,OVM accumulators=A,B patterns=4 pages=2 snapshots=4 extra_clock=1 source_reads=1 data_writes=0 literals_retained=1 pointer_retained=1 original_word=1 next_frame_pending=1 ready_overrun_ack=1 debugger_peek=1 physical_clock_claim=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated shifted peripheral load complete",
    ),
    "stlm-st1-irq": (
        "TMS320C54x staged STLM ST1 IRQ: PASS cases=576 restore=576 sources=IRQ,NMI accumulator=B gap_nops=1 intm_requests=4 memory_cannot_set_intm=1 clear_policy=legacy_unverified imr_states=2 arrivals=4,6,8 pages=2 snapshots=3 followers=STLM,STM source_reads=0 status_writes=1 accepted_frame=1 masked_frame=0 foreground_once=1 rsbx_guard_inherited=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated STLM ST1 IRQ complete",
    ),
    "stlm-cpl": (
        "TMS320C54x staged STLM CPL: PASS cases=16 restore=16 directions=2 consumers=LD,ST gaps=0,1,2,3 latency=3 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated STLM addressing mode complete",
    ),
    "stlm-cmpt": (
        "TMS320C54x staged STLM CMPT: PASS cases=16 restore=16 directions=2 consumers=LD,ST gaps=0,1,2,3 latency=3 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated STLM addressing mode complete",
    ),
    "stlm-st1-edge": (
        "TMS320C54x staged STLM ST1 exclusions: PASS cases=64 restore=64 sources=A,B pages=2 snapshots=2 classes=delay,repeat,block,XC,adjacent_RPTB,adjacent_RPTBD page_wrap=1 data_reads=0 data_writes=0 atomic_fallback=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated STLM ST1 exclusions complete",
    ),
    "stlm-st1": (
        "TMS320C54x staged STLM ST1: PASS cases=18432 restore=18432 sources=A,B families=raw,SXM_LD,ASM_LD,long_SXM_LD,ASM_ADD,ASM_SUB gaps=0,1,2,3 pages=2 snapshots=3 execute_live=1 status_writes=1 destination_reads=0 short_latency=1 long_latency=0 clear_policy=legacy_unverified silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated STLM ST1 complete",
    ),
    "rpt-writa16": (
        "TMS320C54x repeated WRITA: PASS cases=19200 restore=19200 sources=DP,SP,plain,postdec,postinc count_sources=short,long,direct,absolute_RAM,absolute_AR registers=8 counts=0,1,7,255 targets=ordinary,halfspace_cross,wrap16 address_bits=16 ovly=ignored pages=1 snapshots=4 retained_first_source=1 retained_par=1 count_reads=once data_reads=count+1 program_writes=count+1 stream_clocks=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated repeated WRITA complete",
    ),
    "rpt-writa": (
        "TMS320C54x repeated WRITA: PASS cases=38400 restore=38400 sources=DP,SP,plain,postdec,postinc count_sources=short,long,direct,absolute_RAM,absolute_AR registers=8 counts=0,1,7,255 targets=ordinary,overlay_cross,wrap23 address_bits=23 ovly=2 pages=2 snapshots=4 retained_first_source=1 retained_par=1 count_reads=once data_reads=count+1 program_writes=count+1 stream_clocks=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated repeated WRITA complete",
    ),
    "tc-call": (
        "TMS320C54x TC conditional calls: PASS cases=64 restore=64 predicates=TC,NTC initial_tc=2 pages=2 snapshots=4 literal_fetches=1 taken_frame=1 balanced_return=1 untaken_stack_accesses=0 predicate_retained=1 target_retained=1 delayed_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated TC conditional calls complete",
    ),
    "signed-call": (
        "TMS320C54x signed conditional calls: PASS cases=768 restore=768 predicates=AGEQ,ALT,ANEQ,AEQ,AGT,ALEQ,BGEQ,BLT,BNEQ,BEQ,BGT,BLEQ patterns=8 pages=2 snapshots=4 literal_fetches=1 taken_frame=1 balanced_return=1 untaken_stack_accesses=0 predicate_retained=1 target_retained=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated signed conditional calls complete",
    ),
    "bcd-arith-edge": (
        "TMS320C54x BCD arithmetic exclusions: PASS cases=288 restore=288 exclusions=ANEQ,BNEQ,second_delay_word accumulators=2 branch_accumulators=2 outcomes=2 forms=3 pages=2 snapshots=2 data_accesses=0 ar_changes=0 accumulator_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated BCD arithmetic exclusions complete",
    ),
    "bcd-arithmetic": (
        "TMS320C54x BCD arithmetic: PASS cases=36864 restore=36864 operations=ADD,SUB,ADDS,SUBS accumulators=2 predicates=AEQ,BEQ outcomes=2 forms=absolute,long_offset,long_update operands=4 sources=4 sxm_ovm=4 pages=2 snapshots=3 predicate_retained=1 operand_retained=1 data_reads=1 data_writes=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated BCD arithmetic complete",
    ),
    "mar-circular": (
        "TMS320C54x circular MAR: PASS cases=49152 restore=49152 modifiers=minus1,minusAR0,plus1,plusAR0 registers=8 cmpt=2 bk=1,2,3,13,16,31,32,250 positions=4 strides=4 pages=2 snapshots=3 legal_domain=1 independent_residue_oracle=1 publication=Access data_accesses=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated circular MAR complete",
    ),
    "tc-return": (
        "TMS320C54x TC returns: PASS cases=384 restore=384 predicates=TC,NTC outcomes=2 producers=status,BITT_short,BITT_absolute gaps=0,1 selectors=0,15 pages=2 snapshots=4 predicate_retained=1 target_retained=1 untaken_stack_reads=0 data_writes=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated TC returns complete",
    ),
    "bitt-retd": (
        "TMS320C54x RETD BITT: PASS cases=1536 restore=1536 forms=absolute,long_offset selectors=16 t_high=2 values=2 tc_initial=2 pages=2 snapshots=3 stack_reads=1 operand_reads=1 extension_fetches=1 retained_memory_t=1 target_only=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated RETD BITT complete",
    ),
    "bitt": (
        "TMS320C54x staged BITT: PASS cases=6144 restore=6144 forms=DP,SP,plain,postdec,postinc,absolute,long_offset,long_update selectors=16 t_high=2 values=2 tc_initial=2 pages=2 snapshots=3 reads=1 writes=0 retained_memory_t=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated staged BITT complete",
    ),
    "rpt-store-edge": (
        "TMS320C54x repeated store exclusions: PASS cases=4096 restore=4096 forms=4 modes=4 registers=8 counts=4 destinations=0000,0010,0020,005f snapshots=2 reads=0 writes=0 ar_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated repeated store exclusions complete",
    ),
    "rpt-store": (
        "TMS320C54x repeated accumulator stores: PASS cases=49152 restore=49152 forms=STL_A,STL_B,STH_A,STH_B modes=plain,postdec,postinc,preinc registers=8 counts=0,1,7,255 setups=RPT,RPTZ patterns=2 sst_sxm=4 pages=2 snapshots=3 execute_live=1 reads=0 writes=count+1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated repeated accumulator stores complete",
    ),
    "rptz": (
        "TMS320C54x staged RPTZ: PASS cases=768 restore=768 accumulators=2 counts=0,1,3,17 c_tc=4 sxm_ovm=4 pages=2 snapshots=3 count_plus_one=1 literal_reads=1 status_preserved=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated staged RPTZ complete",
    ),
    "sat": (
        "TMS320C54x staged SAT: PASS cases=3072 restore=3072 patterns=16 accumulators=2 initial_overflow=4 sxm_ovm=4 pages=2 snapshots=3 adjacent=2 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated staged SAT complete",
    ),
    "rotate": (
        "TMS320C54x staged rotates: PASS cases=4608 restore=4608 forms=ROR_A,ROR_B,ROL_A,ROL_B,ROLTC_A,ROLTC_B patterns=8 c_tc=4 sxm_ovm=4 pages=2 snapshots=3 adjacent=2 guard_clear=1 status_preserved=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated staged rotates complete",
    ),
    "addm-ar": (
        "TMS320C54x mapped AR ADDM: PASS cases=24576 restore=24576 registers=8 values=4 immediates=4 sxm_ovm_c_ova=16 cmpt=2 pages=2 snapshots=3 read=7 write=8 reads=1 writes=1 extensions=2 accumulators_preserved=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated mapped AR ADDM complete",
    ),
    "addm-ar-load": (
        "TMS320C54x mapped AR ADDM consumers: PASS cases=768 restore=768 registers=8 patterns=4 consumers=short,long_offset legal_gaps=2_3,2_3 pages=2 snapshots=3 data_reads=2 ar_writes=1 forbidden_gap_claim=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated mapped AR ADDM consumers complete",
    ),
    "literal-guard": (
        "TMS320C54x opcode-shaped extensions: PASS cases=54 restore=54 literals=7cf8,7df8,7ff8 block_counts=0,1,2 pages=2 snapshots=3 instruction=ST_absolute writes=count+1 transfer_admission=0",
        "Fatal error: TMS320C54x isolated opcode-shaped extensions complete",
    ),
    "mvpd-controls": (
        (
            "TMS320C54x staged MVPD: PASS cases=128 addressing_modes=4 source_pages=2 source_regions=2 ovly_states=2 restore=128 duplicate_program_reads=0 duplicate_data_writes=0",
            "TMS320C54x staged RPT MVPD: PASS cases=128 counts=4 pages=2 ovly_states=2 destination_modes=2 restore=128 count_plus_one=1 duplicate_transfers=0",
            "TMS320C54x staged RPT interrupts: PASS cases=16 sources=IRQ,NMI counts=4 pages=2 restore=16 deferred_until_final_write=1 correct_continuation=1",
        ),
        "Fatal error: TMS320C54x isolated MVPD controls complete",
    ),
    "mvpd-ext-edge": (
        "TMS320C54x extended MVPD exclusions: PASS cases=512 restore=512 forms=4 registers=8 cmpt=2 pages=2 endpoints=CPU,peripheral snapshots=2 program_reads=0 data_writes=0 ar_status_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated extended MVPD exclusions complete",
    ),
    "mvpd-extended": (
        "TMS320C54x staged extended MVPD: PASS cases=6144 restore=6144 forms=absolute,offset,preincrement,circular registers=8 values=4 pages=2 ovly=2 source_regions=2 cmpt=2 snapshots=3 program_reads=1 data_writes=1 extensions=2 repeat_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated extended MVPD complete",
    ),
    "st1-rules": (
        "TMS320C54x ST1 memory rules: PASS cases=524288 word_values=65536 old_intm=2 masks=ffff,00ff,ff00,0800 memory_cannot_set_intm=1 reserved_bit10=0 debugger_seed=1 restore=1 xf_restored=1 clear_policy=legacy_unverified instruction_phase_claim=0",
        "Fatal error: TMS320C54x isolated ST1 memory rules complete",
    ),
    "st1-bits": (
        "TMS320C54x staged status bits: PASS cases=128 registers=2 bits=16 operations=2 initial_patterns=2 restore=128 xf_output=1 reserved_st1_bit10=0",
        "Fatal error: TMS320C54x isolated status bits complete",
    ),
    "reada-brc": (
        "TMS320C54x READA BRC: PASS cases=18432 restore=18432 forms=DP,SP,absolute,plain,postinc,postdec registers=8 standalone_values=0,1,8000,ffff loop_counts=0,1,2,7 gaps=1,2,3 source_pages=2 ovly=2 snapshots=4 address_bits=23 program_reads=1 brc_writes=1 data_reads=0 retained_par=1 retained_word=1 source_poison=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated READA BRC complete",
    ),
    "reada-brc16": (
        "TMS320C54x READA BRC: PASS cases=18432 restore=18432 forms=DP,SP,absolute,plain,postinc,postdec registers=8 standalone_values=0,1,8000,ffff loop_counts=0,1,2,7 gaps=1,2,3 source_pages=2 ovly=2 snapshots=4 address_bits=16 program_reads=1 brc_writes=1 data_reads=0 retained_par=1 retained_word=1 source_poison=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated READA BRC complete",
    ),
    "reada-brc-edge": (
        "TMS320C54x READA BRC exclusions: PASS cases=16 restore=16 forms=DP,absolute contexts=single_repeat,active_block source_pages=2 snapshots=2 program_reads=0 data_reads=0 data_writes=0 brc_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated READA BRC exclusions complete",
    ),
    "rptb-exit": (
        "TMS320C54x forward block exits: PASS cases=1152 restore=1152 branches=B,BC_AEQ,BC_BEQ counts=0,1,2,7 exits=first,middle,last,never endpoint_layouts=2 pages=2 snapshots=6 source_reads=ordered body_writes=selected braf_not_auto_cleared=1 outside_rsbx=1 spaced_endpoint_revisit=1 interrupt_composition=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated forward block exits complete",
    ),
    "stlm-bsp": (
        "TMS320C54x staged BSP STLM: PASS cases=3072 restore=3072 accumulators=2 patterns=8 flags=16 cpl=2 pages=2 snapshots=3 source_sample=Execute writes_each=1 data_reads=0 publish_clock=7 peripheral_extra_clock=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated BSP STLM complete",
    ),
    "stlm-bsp-edge": (
        "TMS320C54x BSP STLM exclusions: PASS cases=32 restore=32 compositions=delay,repeat,block,XC accumulators=2 pages=2 snapshots=2 data_reads=0 data_writes=0 accumulator_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated BSP STLM exclusions complete",
    ),
    "status-bitf": (
        "TMS320C54x staged status BITF: PASS cases=768 restore=768 registers=ST0,ST1 forms=direct,absolute masks=4 initial_tc=2 pages=2 snapshots=3 reads_each=1 data_writes=0 retained_source=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated status BITF complete",
    ),
    "serial-stm": (
        "TMS320C54x staged BSP STM: PASS cases=12 restore=12 literal_fetches=1 writes=1 destination_reads=0 publish_clock=8 peripheral_extra_clock=1 endpoint_semantics=external silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated staged BSP STM complete",
    ),
    "clock-stm": (
        "TMS320C54x staged CLKMD STM: PASS cases=12 restore=12 literal_fetches=1 writes=1 destination_reads=0 publish_clock=8 pll_reference=unconfigured physical_clock_claim=0",
        "Fatal error: TMS320C54x isolated staged CLKMD STM complete",
    ),
    "std-swwsr": (
        "TMS320C54x standard SWWSR: PASS encodings=65536 reserved_bit15=0 multiplier=0 separate_BSCR_SWCR=1 partial_writes=1 restore=1 reset=1 program_data_wait_claim=0 physical_mapping=unvalidated",
        "Fatal error: TMS320C54x isolated standard SWWSR complete",
    ),
    "stm-acc": (
        "TMS320C54x accumulator STM: PASS cases=3456 restore=3456 lanes=6 literals=6 patterns=4 sxm_ovm=4 pages=2 snapshots=3 literal_retained=1 live_siblings=1 guard_mask=8 publication=Execute adjacent_mvdk=1 writes=2 reads=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated accumulator STM complete",
    ),
    "std-stm": (
        "TMS320C54x standard SWWSR STM: PASS cases=12 restore=12 literal_fetches=1 writes=1 destination_reads=0 publish_clock=8 physical_mapping=unvalidated",
        "Fatal error: TMS320C54x isolated standard SWWSR STM complete",
    ),
    "std-io": (
        "TMS320C54x standard SWWSR I/O: PASS frontend=staged cases=288 restore=288 software_waits=0..7 ready_threshold=2 sources=none,IRQ,NMI timer_continues=1 duplicate_io=0 program_data_wait_claim=0 physical_mapping=unvalidated",
        "Fatal error: TMS320C54x isolated standard SWWSR I/O complete",
    ),
    "std-io-atomic": (
        "TMS320C54x standard SWWSR I/O: PASS frontend=atomic cases=1440 restore=1440 software_waits=0..7 ready_threshold=2 sources=none,IRQ,NMI timer_continues=1 duplicate_io=0 program_data_wait_claim=0 physical_mapping=unvalidated",
        "Fatal error: TMS320C54x isolated standard SWWSR I/O complete",
    ),
    "cold-pipeline": (
        "TMS320C54x ROM4 staged cold frontier: fetch=000f00 pc=0f01 sp=0000 pmst=ffa8 cycles=12",
        "Fatal error: TMS320C54x ROM4 staged cold frontier complete",
    ),
    "rpt-mvdp-irq": (
        "TMS320C54x staged repeated MVDP interrupts: PASS cases=288 restore=288 sources=IRQ,NMI counts=0,1,7,255 count_forms=2 pages=2 arrivals=3 snapshots=3 deferred_until_final_write=1 balanced_stack=1 duplicate_transfers=0",
        "Fatal error: TMS320C54x isolated repeated MVDP interrupts complete",
    ),
    "rpt-mvkd-edge": (
        "TMS320C54x repeated MVKD exclusions: PASS cases=32 restore=32 kinds=mapped_source,mapped_destination,absolute,long_offset count_forms=2 pages=2 snapshots=2 reads=0 writes=0 pointer_updates=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated repeated MVKD exclusions complete",
    ),
    "rpt-mvkd": (
        "TMS320C54x repeated MVKD: PASS cases=9216 restore=9216 forms=DP,SP,plain,postinc,postdec,preinc registers=8 counts=0,1,2,7 count_forms=2 layouts=distinct,equal,overlap pages=2 snapshots=4 source_increment=1 same_address_bypass=1 issue_interval=1 literal_fetches=once mapped_acceptance=0 interrupt_composition=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated repeated MVKD complete",
    ),
    "rpt-mvdp": (
        "TMS320C54x staged repeated MVDP: PASS cases=9600 restore=9600 forms=5 registers=8 counts=0,1,2,7,255 count_forms=2 pages=2 destinations=ordinary,wrap ovly=2 snapshots=3 source_reads=count+1 program_writes=count+1 stream_clocks=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated repeated MVDP complete",
    ),
    "mvdp-pair": (
        "TMS320C54x staged adjacent MVDP: PASS cases=12 restore=12 pages=2 snapshots=6 source_reads=2 program_writes=2",
        "Fatal error: TMS320C54x isolated adjacent MVDP complete",
    ),
    "mvdp": (
        "TMS320C54x staged MVDP: PASS cases=336 restore=336 forms=7 registers=8 pages=2 snapshots=3 source_reads=1 program_writes=1 repeat=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated MVDP complete",
    ),
    "parallel-sub": (
        "TMS320C54x atomic ST||SUB fractions: PASS cases=128 restore=128 source_dest_pairs=4 fractional_patterns=8 memory_patterns=4 ordinary_ram=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated ST||SUB fractions complete",
    ),
    "xc-barrier": (
        (
            "TMS320C54x staged XC return barriers: PASS cases=12 restore=12 forms=RET,RC_taken,RC_untaken sequential_target=1 snapshots=2 physical_phase_claim=0",
            "TMS320C54x atomic XC return barriers: PASS cases=12 restore=12 forms=RET,RC_taken,RC_untaken sequential_target=1 snapshots=2 physical_phase_claim=0",
        ),
        "Fatal error: TMS320C54x isolated XC return barriers complete",
    ),
    "xc-latency": (
        (
            "TMS320C54x staged XC conditions: PASS cases=384 restore=384 sources=TC,C,A,B producer_words=1,2 gaps=0,1,2,3 controlled_words=1,2 snapshots=2 physical_phase_claim=0",
            "TMS320C54x atomic XC conditions: PASS cases=384 restore=384 sources=TC,C,A,B producer_words=1,2 gaps=0,1,2,3 controlled_words=1,2 snapshots=2 physical_phase_claim=0",
        ),
        "Fatal error: TMS320C54x isolated XC conditions complete",
    ),
    "xc-status": (
        (
            "TMS320C54x staged XC/status: PASS cases=64 restore=64 producers=SSBX,RSBX gaps=0,1,2,3 old_condition_words=2 snapshots=2 physical_phase_claim=0",
            "TMS320C54x atomic XC/status: PASS cases=64 restore=64 producers=SSBX,RSBX gaps=0,1,2,3 old_condition_words=2 snapshots=2 physical_phase_claim=0",
        ),
        "Fatal error: TMS320C54x isolated XC/status complete",
    ),
    "xorm": (
        "TMS320C54x atomic XORM: PASS cases=1024 restore=1024 forms=8 values=4 literals=4 sxm_ovm=4 snapshots=2 reads=1 writes=1 flags_preserved=1 staged_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated XORM complete",
    ),
    "atomic-ret": (
        "TMS320C54x atomic conditional returns: PASS cases=144 restore=144 predicates=12 patterns=6 snapshots=2 taken_reads=1 untaken_reads=0 stack_writes=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated atomic conditional returns complete",
    ),
    "cond-reg-store": (
        (
            "TMS320C54x conditional-store disassembler: PASS opcodes=1024 supported=768 rejected=256 pages=2 bounded_extensions=1 pipeline_claim=0",
            "TMS320C54x atomic conditional register stores: PASS cases=18432 restore=18432 forms=STRCD,SRCCD predicates=12 patterns=6 xmem=16 source_values=4 snapshots=2 false_read_writeback=1 circular_update=1 silicon_phase_claim=0",
        ),
        "Fatal error: TMS320C54x isolated conditional register stores complete",
    ),
    "saccd": (
        (
            "TMS320C54x SACCD conformance: PASS variants=576",
            "TMS320C54x SACCD boundaries: PASS cases=96 asm=-16,-1,0,15 sxm=0,1 sst_ignored=1 sources=A,B false_store=1 pipeline_claim=0",
        ),
        "Fatal error: TMS320C54x isolated SACCD complete",
    ),
    "ov-branch": (
        "TMS320C54x atomic overflow branches: PASS cases=64 restore=64 forms=BC,BCD predicates=AOV,ANOV,BOV,BNOV flags=4 snapshots=2 selected_flag_clear=1 delay_condition_retained=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated overflow branches complete",
    ),
    "stack-mod3-edge": (
        (
            "TMS320C54x atomic prefix: PASS staged_suite_claim=0 silicon_phase_claim=0",
            "TMS320C54x MMR push MOD3 exclusions: PASS cases=512 restore=512 opcode_classes=4 arf=8 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 stack_changes=0 silicon_trap_claim=0",
        ),
        "Fatal error: TMS320C54x isolated MMR push MOD3 exclusions complete",
    ),
    "pshd-long": (
        "TMS320C54x extended PSHD: PASS cases=99696 restore=99696 ram_cases=96000 mapped_cases=3696 forms=25 offsets=-1,0,4,32767 circular_offsets=-1,0,4,8 values=4 bk=0,9 cmpt=2 cpl=2 pages=2 sources=none,IRQ,NMI arrivals=early,late snapshots=3 literal_fetches=1 source_reads=1 stack_writes=1 pointer_updates=once source_retained=1 balanced_frames=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated extended PSHD complete",
    ),
    "pshd-long-map": (
        "TMS320C54x extended PSHD mapped sources: PASS cases=3696 restore=3696 sources=26 forms=offset,preincrement,absolute self_modify_excluded=1 patterns=4 controls=safe pages=2 cmpt=2 snapshots=3 literal_fetches=1 source_reads=1 stack_writes=1 pointer_updates=once physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated extended PSHD mapped complete",
    ),
    "pshd-long-edge": (
        "TMS320C54x extended PSHD exclusions: PASS cases=64 restore=64 forms=SP,peripheral,self_AR,unadmitted_mode,delay,repeat,block,XC pages=2 cmpt=2 snapshots=2 data_reads=0 data_writes=0 stack_changes=0 pointer_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated extended PSHD exclusions complete",
    ),
    "stlm-pmst": (
        "TMS320C54x STLM PMST: PASS cases=38912 restore=38912 mapping_cases=4096 interrupt_cases=34816 sources=A,B patterns=8 old_sst_sxm=4 cpl=0,1 pages=2 entry_regions=low,high old_overlay=0,1 interrupt_sources=IRQ0..15,NMI arrivals=early,late snapshots=4 writes=1 source_sample=Execute prefetched_mapping=retained next_mapping=new vector_mapping=Prefetch balanced_frames=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated STLM PMST complete",
    ),
    "stlm-t": (
        (
            "TMS320C54x staged STLM T/TRN: PASS cases=12288 sources=2 patterns=8 sst_sxm_ovm_frct=16 gaps=0,1,2,3 consumers=MPY,MPYR,MPYU,LDM restore=12288 register_write=Execute consumer_sample=Read zero_gap_claim=projection_only duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0",
            "TMS320C54x staged STLM BRC: PASS cases=3072 sources=2 patterns=8 flags=16 gaps=0,1,2,3 restore=3072 register_write=Execute ldm_sample=Read block_repeat_claim=0 duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0",
        ),
        "Fatal error: TMS320C54x isolated STLM T/TRN complete",
    ),
    "stlm-pmst-edge": (
        "TMS320C54x STLM PMST exclusions: PASS cases=32 restore=32 forms=delay,repeat,block,RSA,REA,indirect,peripheral,SP pages=2 snapshots=2 data_reads=0 data_writes=0 control_changes=0 pointer_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated STLM PMST exclusions complete",
    ),
    "rpt-ld-block": (
        "TMS320C54x repeated accumulator LD in block: PASS cases=2048 restore=2048 outer_counts=0,2 inner_counts=0,1,7,255 selectors=4 shifts=-1,15 sxm_ovm=4 patterns=2 pages=2 snapshots=2 each_iteration=1 inner_continuations=outer+1 outer_counter_preserved=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated repeated accumulator LD block complete",
    ),
    "rpt-ld": (
        "TMS320C54x repeated accumulator LD: PASS cases=200704 restore=200704 arithmetic_cases=32768 composition_cases=165888 block_cases=2048 selectors=4 shifts=-16..15 shift_sources=opcode,ASM sxm_ovm=4 counts=0,1,7,255 count_sources=short,long,DP,SP,absolute_RAM,absolute_AR pages=2 sources=none,IRQ,NMI snapshots=3 each_iteration=1 retained_opcode=1 retained_count=1 fresh_continuation=1 balanced_frames=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated repeated accumulator LD complete",
    ),
    "rpt-ld-edge": (
        "TMS320C54x repeated accumulator LD exclusions: PASS cases=32 restore=32 other_families=8 pages=2 snapshots=2 data_reads=0 data_writes=0 arithmetic_changes=0 flag_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated repeated accumulator LD exclusions complete",
    ),
    "rpt-smem": (
        "TMS320C54x basic memory repeat: PASS cases=10376 restore=10376 forms=36 counts=0,1,7,255,32768,65535 pages=2 cpl=2 cmpt=2 sources=none,IRQ,NMI snapshots=3 count_reads=1 body_fetches=1 body_count=rc+1 pointer_effect=once retained_count=1 balanced_frames=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated basic memory repeat complete",
    ),
    "rpt-smem-edge": (
        "TMS320C54x basic memory repeat exclusions: PASS cases=100 restore=100 forms=control,peripheral,write_only_MOD3,long_offset,nested,delay,block mod3_cases=64 arf=8 cmpt=2 pages=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated basic memory repeat exclusions complete",
    ),
    "rptbd-shift-edge": (
        "TMS320C54x RPTBD shifted delay exclusions: PASS cases=16 restore=16 forms=spill,three_words,BRC,branch_owner pages=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 brc_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated RPTBD shifted exclusions complete",
    ),
    "rptbd-shift-st": (
        "TMS320C54x RPTBD shifted delay stores: PASS cases=115200 restore=115200 counts=0,1,2,7 forms=direct,plain,decrement,postincrement,mapped_encoding selectors=4 shifts=-16,-1,15 sst_sxm=4 patterns=2 pages=2 cpl=2 sources=none,IRQ,NMI arrivals=delay,body snapshots=3 delay_writes=1 pointer_effect=once body_count=brc+1 execute_live_source=1 retained_words=1 balanced_frames=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated RPTBD shifted stores complete",
    ),
    "mvkd-forward": (
        "TMS320C54x staged MVKD forwarding: PASS cases=6912 restore=6912 forms=direct,postincrement,absolute consumers=LD_A,ADD_A,LD_B same_and_different=1 gap_words=0,1 sxm_ovm=4 pages=2 cpl=2 source_reads=1 writes_each=1 retained_source=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated MVKD forwarding complete",
    ),
    "mvdk-acc": (
        "TMS320C54x MVDK accumulator destinations: PASS cases=8208 restore=8208 sources=18_mapped,1_RAM destinations=AL,AH,AG,BL,BH,BG forms=direct,indirect,absolute patterns=4 pages=2 snapshots=3 source_reads=1 destination_writes=1 source_sample=Access write=next_clock retained_literals=1 aliases=1 guard_width=8 other_fields_preserved=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated MVDK accumulator destinations complete",
    ),
    "mvdk-acc-prod": (
        "TMS320C54x MVDK accumulator producers: PASS cases=15552 restore=15552 sources=AL,AH,AG,BL,BH,BG destinations=AL,AH,AG,BL,BH,BG forms=3 producers=STL,POPM,LD gaps=0,1 patterns=4 pages=2 snapshots=3 source_reads=1 destination_writes=1 mapped_no_conflict=1 direct_ld_gap0=projection_only guard_width=8 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated MVDK accumulator producers complete",
    ),
    "st-shift-ar-edge": (
        "TMS320C54x shifted AR store exclusions: PASS cases=48 restore=48 excluded_destinations=12 pages=2 snapshots=2 source_reads=0 destination_writes=0 register_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated shifted AR exclusions complete",
    ),
    "st-shift-ar-bank": (
        "TMS320C54x shifted AR store bank conflicts: PASS cases=768 restore=768 destinations=8 writers=STM,MVDK,POPM,ST targets=same,different controls=RAM pages=2 snapshots=3 store_write=7 early_write=7,8 source_reads=once writes=once silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated shifted AR bank conflicts complete",
    ),
    "st-shift-ar": (
        "TMS320C54x shifted AR stores: PASS cases=122880 restore=122880 forms=absolute,offset,preincrement,circular destinations=8 selectors=4 patterns=4 sst_sxm=4 shifts=-16,-1,0,1,15 aliases=self,other pages=2 snapshots=3 publish_clock=8 destination_reads=0 writes=1 retained_extensions=1 execute_live_source=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated shifted AR stores complete",
    ),
    "st-shift-ar-use": (
        "TMS320C54x shifted AR store consumers: PASS cases=30720 restore=30720 destinations=8 selectors=4 patterns=4 sst_sxm=4 shifts=-16,-1,0,1,15 legal_gaps=2,3 pages=2 snapshots=3 publish_clock=8 consumer_reads=1 writes=1 retained_extensions=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated shifted AR stores complete",
    ),
    "rptb-page": (
        "TMS320C54x repeat page comparator: PASS cases=384 restore=384 frontends=atomic,staged counts=0,1,2,7 caller_pages=0,2,127 targets=foreign call_forms=FCALL,FCALLD hazard_controls=2 snapshots=4 rea_width=16 wrap_page=current balanced_frames=1 delay_effects=once physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated repeat page comparator complete",
    ),
    "instr-observer": (
        (
            "TMS320C54x instruction observer: PASS cases=32 restore=32 frontends=atomic,staged pages=2 ovly_states=2 snapshots=4 events_each=7 logical_address=1 captured_opcode=1 repeat_entries=3 ordinary_reads=0 annulled_fetches=0 observer_writes=0",
            "TMS320C54x observer XC annulment: PASS cases=64 restore=64 frontends=atomic,staged pages=2 ovly_states=2 snapshots=4 forms=XC1,XC2 events_each=5 rejected_slots=absent accepted_source_nops=present observer_writes=0",
        ),
        "Fatal error: TMS320C54x isolated instruction observer complete",
    ),
    "asm-delay": (
        "TMS320C54x delayed LD ASM: PASS cases=30720 restore=30720 controls=BD,BCD_taken,BCD_untaken,BD_store_prefix forms=DP,SP,postincrement,offset,absolute shifts=32 patterns=2 sxm_ovm=4 pages=2 snapshots=3 source_reads=1 extension_fetches=owned t_preserved=1 adjacent_consumer=LD_B_ASM_A target_observes_result=1 call_interrupt_repeat_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated delayed LD ASM complete",
    ),
    "asm-delay-edge": (
        "TMS320C54x delay ASM boundary: PASS cases=16 restore=16 forms=4 pages=2 snapshots=2 partial_instruction_rejected=1 data_reads=0 memory_writes=0 atomic_fallback=0",
        "Fatal error: TMS320C54x isolated delay ASM boundary complete",
    ),
    "st-dagen": (
        "TMS320C54x constant DAGEN ST: PASS cases=3840 excluded=80 restore=3920 forms=direct,absolute destinations=AR0..AR7,SP,BK patterns=4 older_stores=none,DAGEN,RAM gaps=0,1 pages=2 snapshots=4 consumers=indirect_LD,PSHM,circular_LD bk_min_gap=1_deferred_2 cpl=0 writes_each=1 extensions=retained repeated_publication=0 indirect_aliases=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated constant DAGEN ST complete",
    ),
    "dld-delay": (
        "TMS320C54x delayed DLD: PASS cases=147456 restore=147456 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD forms=offset,preincrement,circular,absolute registers=8 offsets=-1,2 parity=even,odd destinations=2 patterns=4 sxm_c16=4 compatibility_states=2 pages=2 snapshots=3 ordered_reads=2 literal_fetches=1 pointer_updates=once target_observes_result=1 interrupt_repeat_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated delayed DLD complete",
    ),
    "dld-delay-edge": (
        "TMS320C54x delay DLD boundary: PASS cases=16 restore=16 forms=4 pages=2 snapshots=2 partial_instruction_rejected=1 data_reads=0 memory_writes=0 atomic_fallback=0",
        "Fatal error: TMS320C54x isolated delay DLD boundary complete",
    ),
    "mac-delay": (
        "TMS320C54x staged delay MAC/MAS: PASS cases=147456 restore=147456 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD operations=MAC,MACR,MAS,MASR forms=short,absolute_RAM,absolute_AR5,offset destinations=2 operand_pairs=16 frct_ovm_smul=8 pages=2 snapshots=3 source_reads=1 t_retained=1 callee_observes_result=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated delay MAC/MAS complete",
    ),
    "mac-delay-edge": (
        "TMS320C54x delay MAC/MAS boundary: PASS cases=16 restore=16 forms=4 pages=2 snapshots=2 partial_instruction_rejected=1 data_reads=0 memory_writes=0 atomic_fallback=0",
        "Fatal error: TMS320C54x isolated delay MAC/MAS boundary complete",
    ),
    "mac-bk": (
        "TMS320C54x staged BK MAC/MAS: PASS cases=98304 restore=98304 operations=MAC,MACR,MAS,MASR destinations=A,B forms=direct,absolute pairs=64 flags=16 pages=2 snapshots=3 source_reads=1 data_writes=0 bk_retained=1 t_retained=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated BK MAC/MAS complete",
    ),
    "fcala-delay": (
        "TMS320C54x staged FCALAD: PASS cases=648 restore=648 accumulators=2 caller_pages=3 target_pages=3 delay_layouts=2 sources=none,IRQ,NMI snapshots=6 target_sample=Read return=PC_plus_3 frames=balanced delay_effects=once nominal_callee_execute=10 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated staged FCALAD complete",
    ),
    "imm-index": (
        "TMS320C54x staged indexed immediate ST: PASS cases=98304 ram_cases=96000 excluded_cases=2304 restore=98304 literals=4 directions=2 auxiliary_registers=8 compatibility_states=2 indices=8 accumulator_patterns=4 sst_sxm=4 pages=2 snapshots=3 address=Read literal=retained destination_reads=0 duplicate_writes=0 excluded_pointer_updates=0 producer_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated indexed immediate ST complete",
    ),
    "arith-ar": (
        "TMS320C54x staged mapped AR arithmetic: PASS cases=1024 registers=8 selectors=4 sxm_ovm=4 patterns=2 forms=direct,absolute restore=1024 duplicate_reads=0 data_writes=0",
        "Fatal error: TMS320C54x isolated mapped AR arithmetic complete",
    ),
    "arith-bk": (
        "TMS320C54x staged mapped BK arithmetic: PASS cases=1024 restore=1024 selectors=ADD_A,ADD_B,SUB_A,SUB_B sxm_ovm=4 patterns=8 forms=direct,absolute pages=2 snapshots=2 source=Read post_read_change=1 duplicate_reads=0 data_writes=0 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated mapped BK arithmetic complete",
    ),
    "add-delay": (
        "TMS320C54x delayed absolute ADD: PASS cases=18432 restore=18432 controls=BD,CALLD,FCALLD,RETD sources=RAM,AR3,AL,AH,BL,BH destinations=2 memory_values=4 initial_values=4 sxm_ovm=4 pages=2 snapshots=3 source_reads=1 literal_fetches=1 call_frames=owned target_observes_result=1 conditional_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated delayed absolute ADD complete",
    ),
    "rptb-banz": (
        "TMS320C54x long BANZ within block: PASS cases=768 restore=768 counts=0,1,2,7 forms=offset,preincrement,circular offset_pairs=8 pages=2 snapshots=4 ordered_sources=1 selected_writes=1 ar_updates_once=1 actual_iterations=1 compatibility_composition=unvalidated interrupt_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated long BANZ within block complete",
    ),
    "rptb-bd": (
        "TMS320C54x unconditional delayed block branches: PASS cases=512 restore=512 counts=0,1,2,7 patterns=4 accumulators=2 delay_layouts=2 pages=2 snapshots=4 ordered_sources=1 selected_writes=1 delay_words_once=1 actual_iterations=1 interrupt_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated unconditional delayed block branches complete",
    ),
    "squr": (
        "TMS320C54x staged accumulator SQUR: PASS cases=32768 restore=32768 destinations=2 a_patterns=16 b_patterns=16 flags=8 pages=2 snapshots=4 operand_bits=17 operand=Execute t_unchanged=1 sticky_overflow=1 data_accesses=0 producer_composition=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated accumulator SQUR complete",
    ),
    "repeat-body-isr": (
        "TMS320C54x direct IRQ repeat restoration: PASS cases=768 restore=768 outer_counts=1,2,7,56 inner_counts=0,1,3,63 sources=IRQ,NMI pages=2 returns=FRETE,FRETED snapshots=6 ordered_copies=1 balanced_frames=1 near_return=0 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated direct IRQ repeat restoration complete",
    ),
    "repeat-isr": (
        "TMS320C54x software-saved IRQ repeat: PASS cases=3840 restore=3840 outer_counts=0,1,2,7,56 inner_counts=0,1,3,63 sources=IRQ,NMI pages=2 stacks=2 returns=FRETE,FRETED snapshots=6 leaf_return=inside,REA_plus_1 ordered_copies=1 balanced_frames=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated software-saved IRQ repeat complete",
    ),
    "imm-forward": (
        "TMS320C54x staged immediate forwarding: PASS cases=6912 restore=6912 forms=direct,postincrement,absolute consumers=LD_A,ADD_A,LD_B same_and_different=1 gap_words=0,1 sxm_ovm=4 pages=2 cpl=2 writes_each=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated immediate forwarding complete",
    ),
    "rmw-forward": (
        "TMS320C54x staged RMW forwarding: PASS cases=768 restore=768 same_and_different=1 gap_words=0,1 flags=4 pages=2 cpl=2 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated RMW forwarding complete",
    ),
    "mvdd-source-edge": (
        "TMS320C54x mapped MVDD exclusions: PASS cases=12 restore=12 cpu_register_destination=4 dual_peripheral=4 repeated_peripheral_write=4 pages=2 snapshots=2 reads=0 writes=0 pointer_updates=0 source_opcode_fetches=1 atomic_fallback=0",
        "Fatal error: TMS320C54x isolated mapped MVDD exclusions complete",
    ),
    "mvdd-source": (
        "TMS320C54x mapped MVDD sources: PASS cases=49152 restore=49152 source_addresses=96 patterns=4 x_modifiers=4 y_modifiers=4 pages=2 snapshots=4 source_reads=1 guest_writes=1 source_opcode_fetches=1 peripheral_extra_clock=1 ram_extra_clock=0 register_aliases=Read_owned read_clear=retained endpoint_identities=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated mapped MVDD sources complete",
    ),
    "mvdd-source-rpt": (
        "TMS320C54x mapped MVDD repeat: PASS cases=400 restore=400 layouts=zero_page,enter_peripheral,leave_peripheral,AR_alias,read_clear lengths=128,4 patterns=4 prefixes=short,long pages=2 snapshots=5 reads=11520 writes=11520 source_opcode_fetches=1 peripheral_waits=once register_values=Read_owned endpoint_identities=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated mapped MVDD repeat complete",
    ),
    "ld-ts": (
        "TMS320C54x TS loads: PASS cases=208896 restore=208896 forms=17 registers=8 shift_codes=64 values=4 sxm_ovm=4 destinations=2 compatibility=2 snapshots=3 read_owned_t=1 source_reads=1 writes=0 status_preserved=1 mapped_sources=excluded silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated TS loads complete",
    ),
    "ld-ts-producer": (
        "TMS320C54x TS load producers: PASS cases=4608 restore=4608 producers=STM_T,LD_T,STLM_T shift_codes=8 values=4 sxm_ovm=4 destinations=2 pages=2 snapshots=3 read_owned_t=1 forbidden_stlm_gap_claim=0 duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated TS load producers complete",
    ),
    "st-extended-ar": (
        "TMS320C54x extended AR stores: PASS cases=24576 restore=24576 forms=absolute,offset,preincrement,circular destinations=8 stores=STL_A,STL_B,STH_A,STH_B patterns=4 sst_sxm=4 aliases=self,other pages=2 snapshots=3 publish_clock=7 destination_reads=0 writes=1 literal_fetches=1 execute_live_source=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated extended AR stores complete",
    ),
    "st-ar-consumer": (
        "TMS320C54x extended AR store consumer: PASS cases=6144 restore=6144 destinations=8 stores=STL_A,STL_B,STH_A,STH_B patterns=4 sst_sxm=4 legal_gaps=2,3 pages=2 snapshots=3 publish_clock=7 consumer_reads=1 ar_writes=1 literal_fetches=1 forbidden_gap_claim=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated extended AR consumer complete",
    ),
    "cmpm-smem": (
        "TMS320C54x staged Smem CMPM: PASS cases=52224 restore=52224 forms=17 registers=8 compare_pairs=8 initial_tc=2 sxm_ovm=4 compatibility=2 source_reads=1 literal_fetches=1,2 duplicate_pointer_updates=0 data_writes=0 mapped_sources=excluded silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated Smem CMPM complete",
    ),
    "cmpm-branch": (
        "TMS320C54x staged short CMPM branch: PASS cases=192 restore=192 pages=2 conditions=TC,NTC initial_tc=2 source_read=6 tc_publish=7 target_fetch=9 source_reads=1 literal_fetches=1 data_writes=0 adjacent_branch=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated short CMPM branch complete",
    ),
    "mpy-delay": (
        "TMS320C54x staged delay MPY: PASS cases=18432 restore=18432 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD forms=short,absolute_RAM,absolute_AR,offset destinations=2 operand_pairs=16 frct_ovm=4 pages=2 snapshots=3 source_reads=1 callee_observes_result=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated delay MPY complete",
    ),
    "mpy-delay-edge": (
        "TMS320C54x delay MPY boundary: PASS cases=16 restore=16 forms=4 pages=2 snapshots=2 partial_instruction_rejected=1 data_reads=0 memory_writes=0 atomic_fallback=0",
        "Fatal error: TMS320C54x isolated delay MPY boundary complete",
    ),
    "move-t": (
        "TMS320C54x staged T/TRN moves: PASS cases=1920 restore=1920 forms=MVMD_AR,MVDK_direct,MVDK_short,MVDK_offset,MVDK_absolute destinations=T,TRN values=4 frct_ovm=4 gaps=0,1 pages=2 snapshots=3 source_reads=1 destination_writes=1 consumers=MPY,LDM silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated T/TRN moves complete",
    ),
    "st-delay-edge": (
        "TMS320C54x delay store boundary: PASS cases=16 restore=16 forms=4 pages=2 snapshots=2 partial_instruction_rejected=1 data_reads=0 memory_writes=0 atomic_fallback=0",
        "Fatal error: TMS320C54x isolated delay store boundary complete",
    ),
    "store-delay": (
        "TMS320C54x staged long delay store: PASS cases=2880 restore=2880 controls=BD,BCD_taken,BCD_untaken,CALLD,RETD stores=STL_A,STL_B,STH_A,STH_B forms=offset,preincrement,circular,absolute literals=6 pages=2 snapshots=3 literal_fetches=1 destination_reads=0 store_writes=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated long delay store complete",
    ),
    "mvmd-bk": (
        "TMS320C54x staged MVMD BK: PASS cases=432 restore=432 contexts=isolated,STLM_NOP destinations=AR0_to_AR7,BK values=4 pages=2 snapshots=3 source_reads=1 destination_writes=1 source_retained=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated MVMD BK complete",
    ),
    "mvkd-sp": (
        (
            "TMS320C54x staged MVKD SP: PASS cases=384 restore=384 forms=DP,CPL,indirect,postincrement,postdecrement,preincrement,absolute,offset,MVDM values=4 cpl_states=2 pages=2 snapshots=3 source_reads=1 destination_writes=1 source_retained=1 address_retained=1 silicon_phase_claim=0",
            "TMS320C54x staged MVKD SP contexts: PASS cases=576 restore=576 producers=FRAME,PSHM,POPM,STM gaps=0,1,3 cpl_states=2 pages=2 accumulators=2 younger_frame=1 retained_source=1 silicon_phase_claim=0",
            "TMS320C54x staged MVKD SP exclusions: PASS cases=25 sources=10 source_cases=20 destination_cases=5 forms=MVKD,MVDM operand_reads=0 architectural_results=0",
        ),
        "Fatal error: TMS320C54x isolated MVKD SP complete",
    ),
    "add-sp": (
        (
            "TMS320C54x staged absolute ADD SP: PASS cases=4096 operands=4 initial_values=4 sxm_states=2 ovm_states=2 accumulators=2 sources=SP,RAM pages=2 cpl_states=2 restore=4096 duplicate_reads=0 independent_arithmetic=1 source_retained=1",
            "TMS320C54x staged absolute ADD SP contexts: PASS cases=576 restore=576 producers=FRAME,PSHM,POPM,STM gaps=0,1,3 cpl_states=2 pages=2 accumulators=2 younger_frame=1 retained_source=1 silicon_phase_claim=0",
            "TMS320C54x staged absolute ADD SP exclusions: PASS cases=20 sources=10 accumulators=2 operand_reads=0 architectural_results=0",
        ),
        "Fatal error: TMS320C54x isolated absolute ADD SP complete",
    ),
    "add-bk": (
        "TMS320C54x staged absolute ADD BK: PASS cases=2048 operands=4 initial_values=4 sxm_states=2 ovm_states=2 accumulators=2 sources=BK,RAM pages=2 restore=2048 duplicate_reads=0 independent_arithmetic=1 source_retained=1",
        "Fatal error: TMS320C54x isolated absolute ADD BK complete",
    ),
    "add-absolute": (
        "TMS320C54x staged absolute ADD: PASS cases=2048 operands=4 initial_values=4 sxm_states=2 ovm_states=2 accumulators=2 sources=AR,RAM pages=2 restore=2048 duplicate_reads=0 independent_arithmetic=1",
        "Fatal error: TMS320C54x isolated absolute ADD complete",
    ),
    "stlm-bk": (
        "TMS320C54x staged STLM BK: PASS cases=384 restore=384 sources=2 values=0,3,4,13 consumers=LDU,Xmem gaps=0,1,2,3 pages=2 snapshots=3 bk_write=Execute circular_bk=Decode post_decode_change=1 forbidden_gaps_projection=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated STLM BK complete",
    ),
    "store-bypass": (
        (
            "TMS320C54x staged store bypass: PASS cases=288 restore=288 producers=STL_A,STL_B,STH_A,STH_B,T,TRN destinations=2 gaps=0,1 aliases=0,1 pages=2 snapshots=3 alias_reads=0 writes=1 physical_banks=unvalidated",
            "TMS320C54x original nibble helper: PASS cases=128 restore=128 words=23 selectors=4 values=8 sxm=2 pages=2 count_reads=1 input_reads=1 count_writes=2 silicon_phase_claim=0",
        ),
        "Fatal error: TMS320C54x isolated store bypass complete",
    ),
    "ldu-periph": (
        "TMS320C54x staged peripheral LDU: PASS cases=9216 restore=9216 addresses=64 forms=direct,absolute,indirect values=4 accumulators=2 pages=2 snapshots=3 source_reads=1 writes=0 peripheral_extra_clock=1 physical_mapping=unvalidated",
        "Fatal error: TMS320C54x isolated peripheral LDU complete",
    ),
    "software-intr": (
        "TMS320C54x staged software INTR: PASS cases=256 restore=256 vectors=32 masks=2 pages=2 snapshots=2 frame_writes=1 return_reads=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated software INTR complete",
    ),
    "rptb-br-edges": (
        "TMS320C54x in-block branch exclusions: PASS cases=32 restore=32 kinds=8 pages=2 snapshots=2 below_block_targets=1 endpoint_overlap=1 delayed_control=1 conditional_call=1 nested_blocks=1 far_branch=1 data_writes=0 atomic_fallback=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated in-block branch exclusions complete",
    ),
    "signed-branch": (
        "TMS320C54x staged signed branches: PASS cases=12288 conditions=GE,LT,NE,EQ,GT,LE accumulators=A,B forms=BC,BCD patterns=8 sxm_ovm=4 pages=2 restore=12288 post_evaluate_change=0,1 duplicate_fetches=0",
        "Fatal error: TMS320C54x isolated signed branches complete",
    ),
    "st-t-edge": (
        "TMS320C54x mapped T store exclusions: PASS cases=64 restore=64 stores=4 other_destinations=4 pages=2 snapshots=2 data_reads=0 data_writes=0 register_publication=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated T store exclusions complete",
    ),
    "st-t": (
        "TMS320C54x mapped T stores: PASS cases=49152 restore=49152 stores=STL_A,STL_B,STH_A,STH_B forms=absolute,offset,preincrement,circular registers=8 patterns=8 sst_sxm=4 compatibility_states=2 pages=2 snapshots=3 address=Read data=Execute writes=1 destination_reads=0 pointer_updates=once physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated T stores complete",
    ),
    "st-t-consumer": (
        "TMS320C54x mapped T store consumers: PASS cases=15360 restore=15360 merge_cases=3072 stores=4 patterns=8 sst_sxm=4 memory_values=4 layouts=short_gap0,short_gap1,long_gap0,long_gap1,LD_T_merge pages=2 snapshots=3 short_gap0=projection long_gap0=legal younger_ld_t_wins=1 retirement_rewrite=0 writes=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated T store consumers complete",
    ),
    "ts-alu": (
        "TMS320C54x TS arithmetic: PASS address_cases=6528 arithmetic_cases=147456 restore=153984 operations=ADD,SUB selectors=4 address_forms=17 registers=8 compatibility_states=2 shifts=48 memory_values=4 accumulator_patterns=8 sxm_ovm=4 pages=2 snapshots=3 t_sample=Read duplicate_transactions=0 mapped_sources=excluded physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated TS arithmetic complete",
    ),
    "ts-alu-producer": (
        "TMS320C54x TS arithmetic producers: PASS cases=18432 restore=18432 producers=STM_T,LD_T,STLM_T gaps=0,1 shift_codes=8 memory_values=4 sxm_ovm=4 selectors=4 pages=2 snapshots=3 t_sample=Read projection_only_stlm_gap=0 duplicate_transactions=0 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated TS arithmetic producers complete",
    ),
    "ts-alu-edge": (
        "TMS320C54x TS arithmetic exclusions: PASS cases=320 restore=320 invalid_shift_cases=256 mapped_cases=64 selectors=4 invalid_codes=32..47 pages=2 snapshots=2 invalid_count_reads=1 mapped_reads=0 alu_publication=0 data_writes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated TS arithmetic exclusions complete",
    ),
    "t-load-control": (
        "TMS320C54x T load control: PASS load_cases=52224 pair_cases=768 restore=52992 t_publish=Read no_retirement_rewrite=1 mapped_and_RAM=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated T load control complete",
    ),
    "ld-ar0-index": (
        "TMS320C54x AR0 indexed LD: PASS cases=98304 restore=98304 ram_cases=96000 excluded_cases=2304 registers=8 index_directions=2 compatibility_states=2 destinations=2 index_values=8 memory_values=4 sxm_sst_ovm=8 pages=2 snapshots=3 address_index_sample=Access source_sample=Read result_publish=Execute self_index=1 pointer_updates=once data_writes=0 interrupt_delay_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated AR0 indexed LD complete",
    ),
    "ld-delay-edge": (
        "TMS320C54x delay LD exclusions: PASS cases=28 restore=28 spill_cases=16 other_family_cases=12 source_reads=0 data_writes=0 pointer_updates=0 register_publication=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated delay LD exclusions complete",
    ),
    "load-delay": (
        "TMS320C54x delayed LD: PASS cases=147456 restore=147456 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD forms=offset,preincrement,circular,absolute registers=8 offsets=2 destinations=2 memory_values=4 sxm_ovm=4 compatibility_states=2 pages=2 snapshots=3 source_reads=1 literal_fetches=1 pointer_updates=once target_observes_result=1 interrupt_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated delayed LD complete",
    ),
    "rptb-back": (
        "TMS320C54x backward in-block branches: PASS cases=1536 restore=1536 outer_counts=4 inner_counts=4 forms=conditional_back,unconditional_back accumulators=2 pages=2 sources=none,IRQ,NMI snapshots=4 ordered_copies=1 outer_counts_preserved=1 held_words_restored=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated backward in-block branches complete",
    ),
    "rptbd": (
        "TMS320C54x delayed block repeat: PASS cases=512 restore=512 counts=0,1,2,7 delay_layouts=4 patterns=4 pages=2 snapshots=4 delay_effects=once ordered_copies=1 retained_words=1 interrupt_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated delayed block repeat complete",
    ),
    "rptbd-edge": (
        "TMS320C54x delayed block repeat exclusions: PASS cases=28 restore=28 forms=STM_BRC,STLM_BRC,POPM_BRC,branch,spill,nested_body,nested_delay pages=2 snapshots=2 data_reads=0 data_writes=0 pointer_updates=0 counters_preserved=1 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated delayed block repeat exclusions complete",
    ),
    "rptbd-irq": (
        "TMS320C54x delayed block repeat interrupts: PASS cases=768 restore=768 counts=0,1,2,7 delay_layouts=4 pages=2 sources=IRQ,NMI arrivals=delay,first_write snapshots=6 delay_effects=once ordered_copies=1 balanced_frames=1 retained_words=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated delayed block repeat interrupts complete",
    ),
    "mvdk-mapped": (
        "TMS320C54x mapped MVDK: PASS cases=14256 restore=14256 sources=18 forms=direct,indirect,absolute destinations=11 patterns=4 pages=2 snapshots=3 source_reads=1 destination_writes=1 source_sample=Access mapped_aliases=1 producer_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated mapped MVDK complete",
    ),
    "mvdk-producer": (
        "TMS320C54x mapped MVDK producers: PASS cases=5184 restore=5184 sources=AL,AH,AG,BL,BH,BG forms=3 producers=STL,POPM,LD gaps=0,1 destinations=AR3,RAM patterns=4 pages=2 snapshots=3 source_reads=1 destination_writes=1 direct_alu_latency=retained direct_ld_gap0=projection_only mapped_no_conflict=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated mapped MVDK producers complete",
    ),
    "mvdk-map-edge": (
        "TMS320C54x mapped MVDK exclusions: PASS cases=48 restore=48 sources=12 pages=2 snapshots=2 data_reads=0 data_writes=0 destination_publication=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated mapped MVDK exclusions complete",
    ),
    "conditional-cd": (
        "TMS320C54x delayed conditional call: PASS cases=1920 restore=1920 conditions=12 patterns=8 delay_layouts=2 pages=2 snapshots=5 target_literals=1 delay_words=2 delay_effects=once predicate_retained=1 taken_frame=PC+4 untaken_stack_accesses=0 balanced_return=1 interrupt_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated delayed conditional call complete",
    ),
    "cond-cd-edge": (
        "TMS320C54x delayed conditional call exclusions: PASS cases=32 restore=32 conditions=8 pages=2 snapshots=2 stack_reads=0 stack_writes=0 stack_reservation=0 status_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated delayed conditional call exclusions complete",
    ),
    "rptb-ccd": (
        "TMS320C54x delayed conditional calls in block: PASS cases=1024 restore=1024 counts=0,1,2,7 conditions=AEQ,ANEQ,BEQ,BNEQ outcomes=2 positions=middle,endpoint delay_layouts=2 pages=2 snapshots=4 ordered_copies=1 balanced_frames=1 stack_return=PC+4 outer_count=Decode interrupt_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated delayed conditional calls in block complete",
    ),
    "addm-brc": (
        "TMS320C54x mapped BRC ADDM: PASS cases=3072 restore=3072 values=4 immediates=4 sxm_ovm_c_ova=16 cmpt=2 pages=2 snapshots=3 read=7 write=8 reads=1 writes=1 extensions=2 retained_source=1 active_loop_composition=excluded physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated mapped BRC ADDM complete",
    ),
    "addm-brc-loop": (
        "TMS320C54x ADDM BRC loop consumers: PASS cases=320 restore=320 counts=0,1,3,7 increments=1,-1 separators=NOP,CALL controls=RPTB,RPTBD pages=2 snapshots=5 ordered_publication=1 actual_iterations=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated ADDM BRC loop consumers complete",
    ),
    "rcd": (
        "TMS320C54x conditional delayed returns: PASS cases=4480 restore=4480 predicates=14 patterns=8 delay_layouts=2 stack_alignments=2 pages=2 snapshots=5 predicate_before_delay=1 taken_stack_reads=1 untaken_stack_reads=0 retained_target=1 delay_fetches=once interrupt_block_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated conditional delayed returns complete",
    ),
    "rptb-tc-delay": (
        "TMS320C54x delayed TC branches in block: PASS cases=512 restore=512 predicates=TC,NTC counts=0,1,2,7 patterns=4 delay_layouts=2 pages=2 snapshots=4 tc_producer=BITT ordered_sources=1 selected_writes=1 delay_effects_once=1 actual_iterations=1 interrupt_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated delayed TC branches in block complete",
    ),
    "poll-interrupt": (
        "TMS320C54x polling loop interrupts: PASS cases=384 restore=384 sources=IRQ,NMI arrivals=32 pages=2 snapshots=3 release=handler balanced_frames=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated polling loop interrupts complete",
    ),
    "cmpm-map": (
        "TMS320C54x mapped CMPM: PASS cases=69120 restore=69120 registers=T,TRN,AR0..7 forms=direct,indirect,absolute cpl=2 values=8 literals=6 conditions=TC,NTC initial_tc=2 pages=2 snapshots=3 reads=1 writes=0 literal_words=1,2 source=Read tc=Execute retained=1 indirect_pointer=separate physical_phase_claim=0 control_composition=unvalidated",
        "Fatal error: TMS320C54x isolated mapped CMPM complete",
    ),
    "cmpm-map-edge": (
        "TMS320C54x mapped CMPM exclusions: PASS cases=3840 restore=3840 addresses=80 forms=direct,indirect,absolute cpl=2 initial_tc=2 pages=2 snapshots=2 reads=0 writes=0 pointer_changes=0 status_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated mapped CMPM exclusions complete",
    ),
    "unsigned-map": (
        "TMS320C54x unsigned mapped arithmetic: PASS cases=221184 restore=221184 operations=ADDS,SUBS numeric_cases=147456 mapped_cases=73728 ram_forms=6 mapped_sources=16 accumulators=2 carry_inputs=ignored sxm_ovm=4 words=8 accumulator_patterns=8 late_accumulator=2 pages=2 snapshots=3 unsigned_smem=1 physical_phase_claim=0 control_composition=unvalidated",
        "Fatal error: TMS320C54x isolated unsigned mapped arithmetic complete",
    ),
    "unsigned-edge": (
        "TMS320C54x unsigned mapped exclusions: PASS cases=3840 restore=3840 operations=ADDS,SUBS addresses=80 forms=DP,absolute,indirect accumulators=2 pages=2 snapshots=2 reads=0 writes=0 pointer_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated unsigned mapped exclusions complete",
    ),
    "stld": (
        "TMS320C54x parallel store/load: PASS cases=405504 restore=405504 forms=6 encodings=1536 address_cases=110592 numeric_cases=294912 words=8 patterns=8 asm_modes=32 pages=2 snapshots=3 read_retained=1 store_source=Execute asm_source=Read t_publication=Read alias_priority=X carry_policy=preserve_unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated parallel store/load complete",
    ),
    "stld-edge": (
        "TMS320C54x parallel store/load exclusions: PASS cases=96 restore=96 forms=6 mapped_legs=X,Y pages=2 snapshots=2 reads=0 writes=0 pointer_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated parallel store/load exclusions complete",
    ),
    "stack-page0": (
        "TMS320C54x page-zero stack: PASS cases=73728 restore=73728 addresses=32 words=8 stacks=4 cpl=2 pages=2 snapshots=3 forms=push,pop,pair0,pair1,pair3,double_push source_retained=1 publications=once sp_updates=once status_preserved=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated page-zero stack complete",
    ),
    "stack-page0-edge": (
        "TMS320C54x page-zero stack exclusions: PASS cases=1536 restore=1536 forms=PSHM,POPM encodings=192 pages=2 snapshots=2 data_reads=0 data_writes=0 stack_allocation=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated page-zero stack exclusions complete",
    ),
    "mvdm-acc": (
        "TMS320C54x MVDM accumulator: PASS cases=110592 restore=110592 destinations=AL,AH,AG,BL,BH,BG sources=RAM,AR,BK words=8 patterns=8 status_modes=16 pages=2 snapshots=3 reads=1 writes=1 neighboring_slices=preserved guard_mask=8 source_mutation_after_read=1 consumer_gap=0 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated MVDM accumulator complete",
    ),
    "mvdm-acc-edge": (
        "TMS320C54x MVDM accumulator exclusions: PASS cases=2064 restore=2064 destinations=6 control_sources=86 pages=2 snapshots=2 data_reads=0 data_writes=0 state_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated MVDM accumulator exclusions complete",
    ),
    "repeat-stlm": (
        "TMS320C54x STLM-suspended IRQ repeat: PASS cases=3840 restore=3840 outer_counts=0,1,2,7,56 inner_counts=0,1,3,63 sources=IRQ,NMI pages=2 stacks=2 returns=FRETE,FRETED snapshots=6 status_clear=Execute balanced_frames=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated STLM-suspended IRQ repeat complete",
    ),
    "repeat-context": (
        "TMS320C54x active repeat context: PASS cases=384 restore=384 modes=same,replaced counts=0,1,2,7 layouts=2 pages=2 stack_alignments=2 snapshots=6 control_order=REA,RSA,BRC source_reads=once publications=once source_mutation_after_read=1 ordered_copies=1 balanced_frames=1 brc_gap_words=6 interrupt_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated active repeat context complete",
    ),
    "push-pop-same": (
        "TMS320C54x adjacent PSHM POPM: PASS cases=2048 restore=2048 sources=8 destinations=T,AR3,BK,SP patterns=4 pages=2 stack_alignments=2 snapshots=4 source_reads=1 stack_reads=1 stack_writes=1 register_writes=1 source_mutation_after_read=1 stack_mutation_after_read=1 logical_bypass=1 interrupt_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated adjacent PSHM POPM complete",
    ),
    "rptb-fcall-edge": (
        "TMS320C54x in-block far call exclusions: PASS cases=20 restore=20 forms=delayed_internal,endpoint,internal_target,near_internal,delay_spill pages=2 snapshots=2 data_reads=0 data_writes=0 frame_allocation=0 counters_preserved=1 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated in-block far call exclusions complete",
    ),
    "rptb-fcall": (
        "TMS320C54x in-block far calls: PASS cases=768 restore=768 counts=0,1,2,7 layouts=early,near_end caller_pages=2 callee_pages=same,cross depths=1,2 sources=none,IRQ,NMI snapshots=4 ordered_copy=1 balanced_stack=1 caller_loop_preserved=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated in-block far calls complete",
    ),
    "rptb-fcalld": (
        "TMS320C54x in-block delayed far calls: PASS cases=2304 restore=2304 counts=0,1,2,7 positions=middle,endpoint delay_layouts=two_single,one_double,original_LD_NOP caller_pages=2 callee_pages=same,cross depths=1,2 sources=none,IRQ,NMI snapshots=4 ordered_copy=1 balanced_stack=1 return_pc=call+4 caller_loop_preserved=1 delay_effects=once physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated in-block delayed far calls complete",
    ),
    "macsu-dual": (
        "TMS320C54x staged dual MACSU: PASS address_cases=18432 arithmetic_cases=196608 restore=215040 encodings=512 layouts=3 bk_modes=2 value_pairs=64 accumulator_patterns=8 arithmetic_flags=32 pages=2 snapshots=3 reads=2 data_writes=0 x_unsigned_y_signed=1 x_alias_priority=1 t_publish=Read accumulator_sample=Execute physical_banks=unvalidated interrupt_repeat_composition=unvalidated",
        "Fatal error: TMS320C54x isolated dual MACSU complete",
    ),
    "macsu-dual-edge": (
        "TMS320C54x dual MACSU exclusions: PASS cases=16 restore=16 operands=X,Y sources=cpu_register,peripheral pages=2 snapshots=2 reads=0 pointer_updates=0 register_publication=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated dual MACSU exclusions complete",
    ),
    "rptb-rpt-edge": (
        "TMS320C54x inner repeat exclusions: PASS cases=16 restore=16 forms=header_at_end,long_immediate,memory_count,nested_block pages=2 snapshots=2 data_reads=0 data_writes=0 counters_preserved=1 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated inner repeat exclusions complete",
    ),
    "rptb-single": (
        "TMS320C54x single repeat in block: PASS cases=768 restore=768 outer_counts=4 inner_counts=4 bodies=MVDD,ST,NOP positions=middle,end pages=2 snapshots=4 ordered_memory=1 outer_counts_preserved=1 duplicate_transactions=0 interrupt_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated single repeat in block complete",
    ),
    "asm-mapped": (
        "TMS320C54x mapped LD ASM: PASS cases=49152 restore=49152 sources=16 forms=DP,postincrement,offset,absolute shifts=32 status_states=4 pages=2 snapshots=3 source_sample=Read publish=Execute data_writes=0 pointer_updates=once physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated mapped LD ASM complete",
    ),
    "asm-load": (
        "TMS320C54x staged LD ASM: PASS cases=73728 restore=73728 forms=DP,SP,postincrement,offset,preincrement,absolute registers=8 shifts=32 status_states=4 compatibility_states=2 pages=2 snapshots=3 source_reads=1 publish=Execute t_unchanged=1 pointer_updates=once mapped_sources=excluded physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated LD ASM complete",
    ),
    "asm-consumer": (
        "TMS320C54x LD ASM consumers: PASS cases=9216 restore=9216 operations=LD,ADD,SUB shifts=32 patterns=2 sxm_ovm=4 gaps=0,1 pages=2 snapshots=3 latency=zero source_reads=1 data_writes=0 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated ASM consumers complete",
    ),
    "asm-edge": (
        "TMS320C54x LD ASM exclusions: PASS cases=16 restore=16 sources=control,SP,peripheral pages=2 snapshots=2 source_reads=0 data_writes=0 register_publication=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated ASM exclusions complete",
    ),
    "cmp-delay-edge": (
        "TMS320C54x delayed comparison exclusions: PASS cases=48 restore=48 controls=BD,BCD_TC,BCD_NTC operations=2 layouts=last_word,three_word pages=2 snapshots=2 operand_reads=0 data_writes=0 register_publication=0 literal_prefetch=unclaimed silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated delayed comparison exclusions complete",
    ),
    "compare-delay": (
        "TMS320C54x delayed CMPM BITF: PASS cases=27648 restore=27648 controls=BD,BCD_TC,BCD_NTC operations=2 forms=DP,SP,postincrement value_pairs=16 initial_tc=2 prefix=none,BITF sources=none,IRQ,NMI pages=2 snapshots=4 branch_decision=retained literal_reads=once operand_writes=0 interrupt_after_publication=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated delayed comparison complete",
    ),
    "store-circular": (
        "TMS320C54x staged circular STL STH: PASS cases=786432 restore=786432 modes=4 selectors=4 auxiliary_registers=8 compatibility_states=2 bk=4 indices=4 patterns=4 flags=8 pages=2 snapshots=3 destination_reads=0 duplicate_writes=0 excluded_pointer_updates=0 bk_zero=legacy_linear physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated circular STL STH complete",
    ),
    "store-index": (
        "TMS320C54x staged indexed STL STH: PASS cases=98304 ram_cases=96000 excluded_cases=2304 restore=98304 selectors=4 directions=2 auxiliary_registers=8 compatibility_states=2 indices=8 patterns=4 sst_sxm=4 pages=2 snapshots=3 address=Read data=Execute destination_reads=0 duplicate_writes=0 excluded_pointer_updates=0 producer_composition=unvalidated silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated indexed STL STH complete",
    ),
    "minmax": (
        "TMS320C54x staged MAX MIN: PASS cases=65536 restore=65536 selectors=4 value_pairs=256 status_states=8 pages=2 snapshots=4 operands=Execute signed_bits=40 equality=B carry_only=1 data_accesses=0 duplicate_fetches=0 producer_composition=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated MAX MIN complete",
    ),
    "mar-index": (
        "TMS320C54x staged indexed MAR: PASS cases=3072 restore=3072 directions=2 auxiliary_registers=8 compatibility_states=2 indices=8 starts=2 pages=2 snapshots=3 publication=Access data_accesses=0 alias_AR0=1 duplicate_updates=0 producer_latency=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated indexed MAR complete",
    ),
    "mac-t-imm": (
        "TMS320C54x staged immediate MAC: PASS isolated_cases=24576 stlm_cases=147456 value_pairs=64 selectors=4 flags=16 restore=172032 literal_fetches=1 t_retained=1 accumulator_latency=unvalidated data_reads=0 projection_only_stlm_gap=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated immediate MAC complete",
    ),
    "alu-dual": (
        "TMS320C54x staged dual ALU: PASS address_cases=36864 arithmetic_cases=49152 restore=86016 operations=ADD,SUB encodings=1024 layouts=3 bk_modes=2 value_pairs=64 arithmetic_flags=32 pages=2 snapshots=3 reads=2 data_writes=0 x_alias_priority=1 t_unchanged=1 result_publish=Execute physical_banks=unvalidated repeat_composition=unvalidated",
        "Fatal error: TMS320C54x isolated dual ALU complete",
    ),
    "alu-dual-edge": (
        "TMS320C54x staged dual ALU exclusions: PASS cases=32 restore=32 operations=ADD,SUB operands=X,Y sources=cpu_register,peripheral reads=0 pointer_updates=0 register_publication=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated dual ALU exclusions complete",
    ),
    "mpy-dual": (
        "TMS320C54x staged dual MPY: PASS address_cases=18432 arithmetic_cases=24576 restore=43008 encodings=512 layouts=3 bk_modes=2 value_pairs=64 arithmetic_flags=32 pages=2 snapshots=3 reads=2 data_writes=0 x_alias_priority=1 t_publish=Read result_publish=Execute physical_banks=unvalidated repeat_composition=unvalidated",
        "Fatal error: TMS320C54x isolated dual MPY complete",
    ),
    "mpy-dual-edge": (
        "TMS320C54x staged dual MPY exclusions: PASS cases=16 restore=16 operands=X,Y sources=cpu_register,peripheral pages=2 snapshots=2 reads=0 pointer_updates=0 register_publication=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated dual MPY exclusions complete",
    ),
    "xc-signed": (
        "TMS320C54x staged signed XC: PASS cases=2304 restore=2304 conditions=GE,LT,NE,EQ,GT,LE accumulators=A,B patterns=8 layouts=XC1_single,XC2_two_single,XC2_one_double pages=2 snapshots=4 predicate=Access rejected_writes=0 opcode_literal=1 signed_irq_composition=unvalidated",
        "Fatal error: TMS320C54x isolated signed XC complete",
    ),
    "xc-controls": (
        "TMS320C54x staged XC controls: PASS unconditional_irq=4 arithmetic_irq=32 tc=128 eq_ne=48 restore=212",
        "Fatal error: TMS320C54x isolated XC controls complete",
    ),
    "norm": (
        "TMS320C54x staged NORM: PASS cases=98304 restore=98304 selectors=4 shifts=-16..31 patterns=8 status_states=8 pages=2 snapshots=4 ts=Read result=Execute carry_preserved=1 data_accesses=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated NORM complete",
    ),
    "norm-edge": (
        "TMS320C54x staged NORM aliases: PASS cases=16384 restore=16384 selectors=4 ts_codes=32..47 shifts=-16..-1 patterns=8 status_states=8 pages=2 snapshots=2 data_accesses=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated NORM aliases complete",
    ),
    "exp-norm": (
        "TMS320C54x staged EXP NORM: PASS cases=6144 restore=6144 selectors=4 patterns=16 gaps=0,1,2 sxm_ovm=4 pages=2 snapshots=4 ts=Read zero_gap=projection_only data_accesses=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated EXP NORM complete",
    ),
    "drsub": (
        (
            "TMS320C54x staged DRSUB: PASS cases=6144 restore=6144 address_forms=8 destinations=A,B patterns=8 c16=0,1 sxm=0,1 ovm=0,1 parity=0,1 pair_reads=2 memory_writes=0 retained_pair=1 silicon_phase_claim=0",
            "TMS320C54x paired arithmetic MOD3 exclusions: PASS cases=3584 restore=3584 opcode_classes=14 arf=8 cmpt=2 c16=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0",
        ),
        "Fatal error: TMS320C54x isolated DRSUB complete",
    ),
    "long-pair": (
        (
            "TMS320C54x staged DSUB: PASS cases=6144 address_forms=8 destinations=A,B patterns=8 c16=0,1 sxm=0,1 ovm=0,1 parity=0,1 restore=6144 duplicate_reads=0 memory_writes=0 silicon_phase_claim=0",
            "TMS320C54x staged DADD: PASS cases=12288 address_forms=8 selectors=4 patterns=8 c16=0,1 sxm=0,1 ovm=0,1 parity=0,1 restore=12288 duplicate_reads=0 memory_writes=0 silicon_phase_claim=0",
            "TMS320C54x staged long-pair controls: PASS dsub=6144 dadd=12288 restore=18432 pair_reads=2 memory_writes=0",
            "TMS320C54x paired arithmetic MOD3 exclusions: PASS cases=3584 restore=3584 opcode_classes=14 arf=8 cmpt=2 c16=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0",
        ),
        "Fatal error: TMS320C54x isolated long-pair controls complete",
    ),
    "block-xc": (
        "TMS320C54x block XC: PASS cases=5760 restore=5760 predicates=15 layouts=XC1_single,XC2_two_single,XC2_one_double counts=0,1,2,7 positions=interior,endpoint pages=2 snapshots=4 source_rotations=2 ordered_reads=1 selected_writes=1 rejected_writes=0 opcode_literal=1 counter_braf_owned=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated block XC complete",
    ),
    "block-xc-edge": (
        "TMS320C54x block XC exclusions: PASS cases=24 restore=24 forms=XC1,XC2 cross_end_positions=2 pages=2 snapshots=3 data_reads=0 data_writes=0 pointer_effects=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated block XC exclusions complete",
    ),
    "block-xc-irq": (
        "TMS320C54x short block XC protection: PASS cases=384 restore=384 counts=0,1,3,7 forms=XC1,XC2 loop_words=3,4,5 sources=IRQ,NMI pages=2 snapshots=4 all_stores_before_vector=1 vector_fetches=1 stack_writes=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated short block XC protection complete",
    ),
    "ld-circular": (
        "TMS320C54x circular LD: PASS cases=393216 restore=393216 ram_cases=368640 excluded_cases=24576 modifiers=4 registers=8 compatibility_states=2 destinations=2 bk=0,1,7,16,8192 index_patterns=4 valid_nonzero_steps=1 memory_values=4 sxm_ovm_sst=8 pages=2 snapshots=3 bk_sample=Decode address_index_sample=Access source_sample=Read result_publish=Execute self_index=1 pointer_updates=once data_writes=0 interrupt_delay_repeat_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated circular LD complete",
    ),
    "move-brc-edge": (
        "TMS320C54x staged memory move BRC exclusions: PASS cases=24 restore=24 writers=MVDK,MVMD contexts=block_body,RPTBD_delay pages=2 snapshots=3 source_reads=0 counter_writes=0 pointer_effects=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated memory move BRC exclusions complete",
    ),
    "move-brc": (
        "TMS320C54x staged memory move BRC: PASS cases=960 restore=960 source_forms=8 counts=0,1,2,7,127 gaps=0,1 rptb_rptbd=2 pages=2 snapshots=3 source_reads=1 counter_reads=0 counter_writes=1 latency=0 source_retained=1 count_plus_one=1 active_repeat_write_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated memory move BRC complete",
    ),
    "asm-store-bp": (
        "TMS320C54x staged ASM store bypass: PASS cases=24576 restore=24576 selectors=4 shifts=32 sxm_sst=4 destinations=2 gaps=0,1 aliases=0,1 pages=2 snapshots=3 retained_shift=1 alias_reads=0 writes=1 physical_banks=unvalidated",
        "Fatal error: TMS320C54x isolated ASM store bypass complete",
    ),
    "xmem-store-bp": (
        "TMS320C54x staged Xmem store bypass: PASS cases=288 restore=288 producers=6 destinations=2 gaps=0,1 aliases=0,1 pages=2 snapshots=3 alias_reads=0 writes=1 physical_banks=unvalidated",
        "Fatal error: TMS320C54x isolated Xmem store bypass complete",
    ),
    "xmem-store": (
        "TMS320C54x staged Xmem stores: PASS cases=786432 restore=786432 encodings=1024 operations=STL,STH xmem_modes=16 shifts=0..15 values=8 accumulators=2 status_states=16 pages=2 snapshots=3 eab=Read eb=Execute destination_reads=0 data_writes=1 sst_sxm=1 ovm_independent=1 bk_zero_and_wrap=1 mapped_acceptance=0 repeat_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated Xmem stores complete",
    ),
    "xmem-alu": (
        "TMS320C54x staged Xmem arithmetic: PASS cases=393216 restore=393216 encodings=1024 operations=ADD,SUB xmem_modes=16 shifts=0..15 values=8 accumulators=2 status_states=8 pages=2 snapshots=3 read=5 publish=6 pointer_access=4 captured_source=1 bk_zero_and_wrap=1 data_writes=0 mapped_acceptance=0 repeat_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated Xmem arithmetic complete",
    ),
    "write-a-edge": (
        "TMS320C54x staged WRITA exclusions: PASS cases=12 restore=12 classes=cpu_source,peripheral_source,repeat_extended,control_delay,block_repeat,XC source_reads=0 program_writes=0 atomic_fallback=0",
        "Fatal error: TMS320C54x isolated WRITA exclusions complete",
    ),
    "write-a": (
        "TMS320C54x staged WRITA: PASS cases=3584 restore=3584 forms=7 registers=8 values=4 pages=2 mapping=2 snapshots=4 source_reads=1 program_writes=1 destination_reads=0 repeat=unvalidated mapped_source=excluded silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated WRITA complete",
    ),
    "banzd-long": (
        (
            "TMS320C54x staged long-offset BANZ: PASS cases=2304 forms=offset,preincrement,circular registers=8 offset_pairs=8 compatibility=0,1 pages=2 restore=2304 offset_fetches=1 target_fetches=1 data_accesses=0 silicon_phase_claim=0",
            "TMS320C54x staged long-offset BANZD: PASS cases=4608 restore=4608 forms=offset,preincrement,circular registers=8 offset_pairs=8 compatibility=0,1 pages=2 delay_forms=two_single,one_double offset_fetches=1 target_fetches=1 data_accesses=0 silicon_phase_claim=0",
        ),
        "Fatal error: TMS320C54x isolated long BANZD complete",
    ),
    "carry-branch": (
        "TMS320C54x staged carry branches: PASS cases=384 forms=BC,BCD conditions=C,NC producer=status_bit gaps=0,1,2 pages=2 restore=384 post_evaluate_change=0,1 duplicate_fetches=0 arithmetic_producers=unvalidated",
        "Fatal error: TMS320C54x isolated carry branches complete",
    ),
    "tc-branch": (
        "TMS320C54x staged TC branches: PASS cases=1536 forms=BC,BCD conditions=TC,NTC producers=BIT,CMPR_EQ,status_bit,CMPR_LT gaps=0,1,2 pages=2 restore=1536 post_evaluate_change=0,1 duplicate_fetches=0",
        "Fatal error: TMS320C54x isolated TC branches complete",
    ),
    "bio-branch": (
        "TMS320C54x staged BIO branches: PASS cases=256 forms=BC,BCD conditions=BIO,NBIO levels=0,1 transitions=4 pages=2 restore=256 line_sample=Read duplicate_callbacks=0 duplicate_fetches=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated BIO branches complete",
    ),
    "plain-branch": (
        "TMS320C54x staged plain conditional branches: PASS cases=48 conditions=AEQ,ANEQ,BEQ,BNEQ pages=2 outcomes=2 restore=48 duplicate_fetches=0 taken_data_reads=0",
        "Fatal error: TMS320C54x isolated plain conditional branches complete",
    ),
    "rptb": (
        "TMS320C54x staged RPTB: PASS cases=6144 irq_nmi_cases=24 restore=6168 counts=0,1,2,7 zero_overhead_body=5 counter=Decode final_braf=Decode silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated RPTB complete",
    ),
    "rptb-branch": (
        "TMS320C54x in-block branches: PASS cases=12288 restore=12288 layouts=diamond,endpoint counts=0,1,2,7 patterns=4 accumulators=2 pages=2 snapshots=8 arrivals=9,10,12,14 sources=none,IRQ,NMI source_reads_each=1 selected_writes_each=1 brc_decode=1 braf_final_decode=1 held_tags_restored=1 interrupt_return=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated in-block branches complete",
    ),
    "mul-mod3-edge": (
        "TMS320C54x multiply load MOD3 exclusions: PASS cases=4352 restore=4352 opcode_classes=34 arf=8 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 asm_changes=0 rc_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated multiply load MOD3 exclusions complete",
    ),
    "mul-t-sources": (
        (
            "TMS320C54x staged memory multiply: PASS ram_cases=27648 mapped_cases=24576 operations=MPYA,MASA,MACA,MACAR patterns=8 flag_states=16 restore=52224 t_read_owned=1 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
            "TMS320C54x staged MPYU: PASS ram_cases=27648 mapped_cases=24576 accumulators=2 t_values=2 patterns=8 flag_states=16 restore=52224 t_retained=1 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
            "TMS320C54x staged MPY and MPYR: PASS cases=52224 accumulators=2 rounding=0,1 t_retained=1 restore=52224 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
            "TMS320C54x staged memory LD T: PASS cases=52224 ram_and_mapped=1 restore=52224 read_publication=1 retirement_rewrite=0 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
        ),
        "Fatal error: TMS320C54x isolated multiply and T sources complete",
    ),
    "arith-mod3-edge": (
        "TMS320C54x arithmetic MOD3 exclusions: PASS cases=35840 restore=35840 scalar_classes=24 variable_selectors=8 shifts=32 arf=8 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 alu_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated arithmetic MOD3 exclusions complete",
    ),
    "memory-alu": (
        "TMS320C54x staged memory ADD/SUB: PASS cases=104448 forms=17 registers=8 selectors=4 patterns=8 sxm_ovm=4 compatibility=2 restore=104448 duplicate_reads=0 data_writes=0",
        "Fatal error: TMS320C54x isolated memory ADD SUB complete",
    ),
    "shifted-alu": (
        "TMS320C54x staged shifted memory ADD/SUB: PASS ram_cases=147456 mapped_ar_cases=98304 selectors=8 shifts=32 sxm_ovm=4 patterns=4 restore=245760 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated shifted memory ADD SUB complete",
    ),
    "bitf-smem": (
        "TMS320C54x staged BITF: PASS cases=52224 forms=17 registers=8 mask_pairs=8 initial_tc=2 sxm_ovm=4 compatibility=2 restore=52224 duplicate_reads=0 data_writes=0",
        "Fatal error: TMS320C54x isolated Smem BITF complete",
    ),
    "mapped-bitf": (
        "TMS320C54x staged mapped BITF: PASS cases=13824 registers=AL,AH,AG,BL,BH,BG,T,TRN masks=4 initial_tc=2 producer_cases=1536 restore=13824 reads_each=1 data_writes=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated mapped BITF complete",
    ),
    "bitf-mod3-edge": (
        "TMS320C54x bitfield MOD3 exclusions: PASS cases=512 restore=512 operations=CMPM,BITF initial_tc=2 arf=8 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 tc_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated bitfield MOD3 exclusions complete",
    ),
    "indirect-dld": (
        (
            "TMS320C54x paired circular fixture guard: PASS cases=14 nonzero_bk_domain=SPRU131G_5.5.3.4 cpu_execution=0",
            "TMS320C54x staged extended DST: PASS cases=3072 forms=offset,preincrement,circular,absolute registers=8 accumulators=2 offsets=4 compatibility=0,1 sst=0,1 restore=3072 duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0",
            "TMS320C54x staged short-indirect DST: PASS cases=9216 modes=12 registers=8 accumulators=2 parity=even,odd compatibility=0,1 sst=0,1 restore=9216 duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0",
            "TMS320C54x store/DLD address composition: PASS cases=1152 restore=1152 registers=8 patterns=4 gaps=0,1,2 forms=ordinary,shifted pages=2 snapshots=3 writes=2 reads=2 pointer_updates=once silicon_phase_claim=0",
            "TMS320C54x staged indirect DLD: PASS cases=23040 forms=15 registers=8 accumulators=2 patterns=4 sxm=0,1 c16=0,1 compatibility=0,1 restore=23040 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
        ),
        "Fatal error: TMS320C54x isolated indirect double loads complete",
    ),
    "load-variable": (
        "TMS320C54x staged variable memory LD: PASS cases=104448 address_forms=17 registers=2 accumulators=2 values=4 shifts=32 sxm_ovm=4 restore=104448 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated variable memory loads complete",
    ),
    "ldu-shift16": (
        (
            "TMS320C54x staged LDU: PASS ram_cases=52224 mapped_ar_cases=2304 address_forms=17 registers=8 accumulators=2 values=8 sxm_ovm=4 compatibility=2 restore=54528 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
            "TMS320C54x staged memory LD shift16: PASS ram_cases=52224 mapped_ar_cases=2304 address_forms=17 registers=8 accumulators=2 values=8 sxm_ovm=4 compatibility=2 restore=54528 duplicate_reads=0 data_writes=0 silicon_phase_claim=0",
        ),
        "Fatal error: TMS320C54x isolated LDU and shift16 loads complete",
    ),
    "load-mod3-edge": (
        "TMS320C54x load MOD3 exclusions: PASS cases=9728 restore=9728 families=7 variable_shifts=32 arf=8 accumulators=2 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated load MOD3 exclusions complete",
    ),
    "indirect-load": (
        "TMS320C54x staged indirect loads: PASS cases=6 ar_update=Access read=Read execute=Execute captured_address_restore=6",
        "Fatal error: TMS320C54x isolated indirect loads complete",
    ),
    "rpt-subc-st": (
        (
            "TMS320C54x staged repeat SUBC: PASS cases=38400 forms=5 rpt=short,long counts=0,1,15,32,255 sources=none,IRQ,NMI pages=2 restore=38400 count_plus_one=1 divisor_reads_each_iteration=1 duplicate_body_fetches=0 silicon_phase_claim=0",
            "TMS320C54x staged repeat immediate ST: PASS cases=38400 forms=5 rpt=short,long counts=0,1,15,32,255 values=4 sst_sxm_ovm=8 sources=none,IRQ,NMI pages=2 restore=38400 count_plus_one=1 literal_fetches=1 body_fetches=1 destination_reads=0 cycles_each=2 silicon_phase_claim=0",
        ),
        "Fatal error: TMS320C54x isolated repeated SUBC and ST complete",
    ),
    "subc-mod3-edge": (
        "TMS320C54x SUBC MOD3 exclusions: PASS cases=512 restore=512 arf=8 accumulators=2 cmpt=2 pages=2 frontends=2 snapshots=2 repeat=none,short data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated SUBC MOD3 exclusions complete",
    ),
    "mpy-mod3-edge": (
        "TMS320C54x memory MPY MOD3 exclusions: PASS cases=256 restore=256 arf=8 accumulators=2 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated memory MPY MOD3 exclusions complete",
    ),
    "mpy-mem-imm": (
        (
            "TMS320C54x memory immediate MPY: PASS cases=98304 restore=98304 forms=12 values=8 literals=8 flags=8 accumulators=2 pages=2 snapshots=4 source_reads=1 data_writes=0 t_publication=Read address_literal_retained=1 repeat_acceptance=0 irq_acceptance=0 silicon_phase_claim=0",
            "TMS320C54x memory immediate MAC: PASS cases=262144 restore=262144 ram_forms=8 values=8 literals=8 flags=16 selectors=4 pages=2 snapshots=4 source_reads=1 data_writes=0 t_publication=Read repeat_acceptance=0 irq_acceptance=0 silicon_phase_claim=0",
            "TMS320C54x memory immediate MAC exclusions: PASS cases=1664 restore=1664 mapped_sources=96 mod3_forms=8 selectors=4 pages=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0",
        ),
        "Fatal error: TMS320C54x isolated memory immediate MPY complete",
    ),
    "mpy-t-imm": (
        "TMS320C54x staged immediate MPY: PASS isolated_cases=12288 stlm_cases=73728 value_pairs=64 selectors=4 flags=16 restore=86016 literal_fetches=1 t_retained=1 data_reads=0 projection_only_stlm_gap=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated immediate MPY complete",
    ),
    "mpy-mapped": (
        (
            "TMS320C54x staged MPY mapped consumer: PASS cases=3072 restore=3072 sources=AL,AH,AG,BL,BH,BG gaps=0,1 reads_each=1 literal_fetches=1 data_writes=0 silicon_phase_claim=0",
            *IMMEDIATE_MAPPED_REQUIRED,
            *DST_BYPASS_REQUIRED,
        ),
        "Fatal error: TMS320C54x isolated MPY mapped consumer complete",
    ),
    "absolute": (
        "TMS320C54x staged ABS: PASS cases=13824 selectors=4 patterns=16 status_states=32 pages=2 producer_cases=1536 restore=13824 data_accesses=0 irq_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated ABS complete",
    ),
    "negate": (
        "TMS320C54x staged NEG: PASS cases=13824 selectors=4 patterns=16 status_states=32 pages=2 producer_cases=1536 restore=13824 data_accesses=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated NEG complete",
    ),
    "logic-edges": (
        "TMS320C54x long logic exclusions: PASS cases=24 restore=24 mapped_peripheral=12 repeats=6 control_delays=6 data_reads=0 data_writes=0 pointer_updates=0 atomic_fallback=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated long logic exclusions complete",
    ),
    "logic-short": (
        (
            "TMS320C54x staged memory logic: PASS cases=576 operations=3 accumulators=2 address_forms=4 value_pairs=4 sxm_states=2 restore=576 duplicate_reads=0",
            "TMS320C54x logic transfer MOD3 exclusions: PASS cases=2816 restore=2816 opcode_classes=11 arf=8 cmpt=2 initial_tc=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 program_writes=0 io_writes=0 pointer_changes=0 silicon_trap_claim=0",
        ),
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
        (
            "TMS320C54x POPD edges: PASS cases=31 restore=31 exclusions=15 dagen_conflicts=16 conflict_read=6 conflict_write=7 same_and_different_registers=1 active_controls_rejected=4 memory_count_rejected=1 source_capture=1 destination_capture=1 silicon_phase_claim=0",
            "TMS320C54x extended POPD exclusions: PASS cases=64 restore=64 forms=mapped,SP,peripheral,unadmitted_mode,delay,repeat,block,XC pages=2 cmpt=2 snapshots=2 data_reads=0 data_writes=0 stack_changes=0 pointer_changes=0 silicon_trap_claim=0",
        ),
        "Fatal error: TMS320C54x isolated POPD edges complete",
    ),
    "popd-ram": (
        (
            "TMS320C54x staged POPD: PASS cases=50 address_forms=4 compiler_latency=6 original_copy_words=6 copy_cases=12 copy_sxm_states=2 restore=50 duplicate_stack_reads=0 duplicate_writes=0",
            "TMS320C54x extended POPD: PASS cases=16128 restore=16128 forms=25 values=4 cmpt=2 cpl=2 pages=2 snapshots=3 circular_bk=9 circular_steps_bounded=1 literal_fetches=1 stack_reads=1 destination_writes=1 pointer_updates=once source_retained=1 mapped_acceptance=0 irq_acceptance=0 physical_phase_claim=0",
        ),
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
        "TMS320C54x staged ADDS: PASS cases=52224 restore=52224 forms=17 registers=8 accumulators=2 patterns=8 sxm_ovm=4 compatibility=2 source_unsigned=1 reads=1 data_writes=0 mapped_acceptance=0 silicon_phase_claim=0",
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
    "cmpm-acc": (
        "TMS320C54x accumulator CMPM: PASS cases=6912 restore=6912 lanes=6 forms=direct,indirect,absolute cpl=2 values=4 literals=2 conditions=TC,NTC initial_tc=2 pages=2 snapshots=3 reads=1 writes=0 source=Read tc=Execute retained=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated accumulator CMPM complete",
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
    "pmst-popd": (
        "TMS320C54x staged POPD PMST: PASS cases=64 restore=64 values=4 source_pages=2 target_regions=2 stack_reads=1 writes=1 source_retained=1 active_repeat_claim=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated POPD PMST complete",
    ),
    "repeat-pmst": (
        "TMS320C54x active repeat POPD PMST: PASS cases=384 restore=384 contexts=same,replaced counts=0,1,2,7 layouts=2 pages=2 stack_alignments=2 snapshots=6 pmst_unchanged=1 stack_retained=1 publications=once actual_iterations=1 interrupt_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated active repeat POPD PMST complete",
    ),
    "repeat-popd": (
        "TMS320C54x active repeat POPD context: PASS cases=384 restore=384 contexts=same,replaced counts=0,1,2,7 layouts=2 pages=2 stack_alignments=2 snapshots=6 control_order=REA,RSA,BRC source_retained=1 publications=once actual_iterations=1 interrupt_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated active repeat POPD context complete",
    ),
    "mvdk-circular": (
        "TMS320C54x circular MVDK: PASS cases=294336 restore=294336 excluded=8064 encodings=32 destination_kinds=21 mapped_sources=18 native_bk=82 snapshots=2,3 raw_words=1 alias_order=sample,modify,publish physical_phase_claim=0 interrupt_repeat_composition=unvalidated",
        "Fatal error: TMS320C54x isolated circular MVDK complete",
    ),
    "mvdk-repeat": (
        "TMS320C54x repeated MVDK: PASS cases=105696 restore=105696 source_geometries_skipped=288 encodings=184 counts=4 placements=6 compatibility_states=2 cpl_states=2 pages=2 snapshots=3 mapped_access=excluded physical_phase_claim=0 interrupt_composition=unvalidated",
        "Fatal error: TMS320C54x isolated repeated MVDK complete",
    ),
    "mvdk-rpt-edge": (
        "TMS320C54x repeated MVDK exclusions: PASS cases=480 restore=480 sources_destinations=10 counts=2 encodings=2 compatibility_states=2 pages=2 snapshots=3 reads=0 writes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated repeated MVDK exclusions complete",
    ),
    "rptb-call": (
        "TMS320C54x in-block near calls: PASS cases=3456 restore=3456 counts=0,1,5,15 layouts=middle,endpoint,whole_block pages=3 depths=1,2 sources=none,IRQ,NMI calls=CALL,CALLD returns=RET,RETD snapshots=4 ordered_copy=1 balanced_stack=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated in-block near calls complete",
    ),
    "rptb-call-edge": (
        "TMS320C54x in-block near call exclusions: PASS cases=72 restore=72 forms=internal_target,delay_spill,literal_spill pages=3 counts=2 snapshots=4 reads=0 writes=0 stack_allocation=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated in-block near call exclusions complete",
    ),
    "rpt-mem-copy": (
        "TMS320C54x memory-count copies: PASS cases=27648 restore=27648 count_sources=24 owned_mapped_sources=18 counts=0,1,7,31 bodies=MVDD,MVKD,MVDK flag_states=2 pages=2 sources=none,IRQ,NMI snapshots=4 patterns=2 count_reads=once mutation_after_read=1 body_fetches=once physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated memory-count copies complete",
    ),
    "rpt-mem-wrap": (
        "TMS320C54x memory-count wrap: PASS cases=192 restore=192 count_sources=AL,RAM counts=0,1,7,31 bodies=MVDD,MVKD,MVDK pages=2 snapshots=4 header=fffe,ffff body=0000 count_reads=once physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated memory-count wrap complete",
    ),
    "rpt-mem-chain": (
        "TMS320C54x memory-count chaining: PASS cases=384 restore=384 count_sources=AL,RAM bodies=MVDD,MVKD,MVDK then=RPT_NOP first_counts=0,1,7,31 second_count=3 flag_states=2 pages=2 snapshots=4 stale_body=excluded physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated memory-count chaining complete",
    ),
    "addc": (
        "TMS320C54x ADDC: PASS cases=110592 restore=110592 numeric_cases=73728 mapped_cases=36864 ram_forms=6 mapped_sources=16 accumulators=2 carry_states=2 sxm_ovm=4 words=8 accumulator_patterns=8 late_accumulator_carry=2 pages=2 snapshots=3 unsigned_smem=1 physical_phase_claim=0 control_composition=unvalidated",
        "Fatal error: TMS320C54x isolated ADDC complete",
    ),
    "addc-edge": (
        "TMS320C54x ADDC exclusions: PASS cases=1920 restore=1920 addresses=80 forms=DP,absolute,indirect accumulators=2 pages=2 snapshots=2 reads=0 writes=0 pointer_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated ADDC exclusions complete",
    ),
    "addc-chain": (
        "TMS320C54x ADDC chains: PASS cases=4096 restore=4096 destinations=AA,BA,AB,BB accumulator_pairs=4 word_pairs=4 carry_states=2 sxm_ovm=4 pages=2 snapshots=4 issue_gap=0 read_retention=1 carry_sample=Execute physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated ADDC chains complete",
    ),
    "mvdk-circ-edge": (
        "TMS320C54x circular MVDK exclusions: PASS cases=19712 restore=19712 encodings=32 destinations=77 compatibility_states=2 pages=2 snapshots=2 reads=0 writes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated circular MVDK exclusions complete",
    ),
    "portr-bypass": (
        "TMS320C54x PORTR consumer bypass: PASS cases=6656 restore=6656 generic_cases=6144 dispatcher_cases=512 consumers=LD,AND,OR,XOR gaps=0,1,2 alias_controls=2 accumulators=2 sxm=2 patterns=8 pages=2 snapshots=4 volatile_io=1 real_writes=1 peripheral_bypass_claim=0 physical_banks=unvalidated",
        "Fatal error: TMS320C54x isolated PORTR consumer bypass complete",
    ),
    "sqdst": (
        "TMS320C54x staged SQDST: PASS address_cases=9216 arithmetic_cases=393216 restore=402432 encodings=256 layouts=3 bk_modes=2 value_pairs=64 old_a_patterns=8 old_b_patterns=8 arithmetic_flags=32 snapshots=3 reads=2 writes=0 t_unchanged=1 old_accumulators=Execute physical_banks=unvalidated interrupt_repeat_composition=unvalidated",
        "Fatal error: TMS320C54x isolated SQDST complete",
    ),
    "sqdst-edge": (
        "TMS320C54x SQDST exclusions: PASS cases=16 restore=16 reads=0 pointer_updates=0 register_publication=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated SQDST exclusions complete",
    ),
    "stmac": (
        "TMS320C54x parallel MAC: PASS cases=1720320 restore=1720320 operations=MAC,MACR,MAS,MASR encodings=4096 source_destination_pairs=4 shifts=32 reads=1 writes=1 retained=T,X,ASM old_source_store=1 carry=unvalidated physical_banks=unvalidated interrupt_repeat_composition=unvalidated",
        "Fatal error: TMS320C54x isolated parallel MAC complete",
    ),
    "stmac-edge": (
        "TMS320C54x parallel MAC exclusions: PASS cases=128 restore=128 reads=0 writes=0 pointer_updates=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated parallel MAC exclusions complete",
    ),
    "mac-family": (
        "TMS320C54x dual MAC family: PASS cases=1720320 restore=1720320 operations=MAC,MACR,MAS,MASR encodings=4096 source_destination_pairs=4 snapshots=3 reads=2 t_publish=Read accumulator_sample=Execute physical_banks=unvalidated interrupt_repeat_composition=unvalidated",
        "Fatal error: TMS320C54x isolated dual MAC family complete",
    ),
    "mac-family-edge": (
        "TMS320C54x dual MAC family exclusions: PASS cases=128 restore=128 reads=0 pointer_updates=0 register_publication=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated dual MAC family exclusions complete",
    ),
    "mac-dual": (
        "TMS320C54x staged dual MAC: PASS address_cases=18432 arithmetic_cases=196608 restore=215040 encodings=512 layouts=3 bk_modes=2 value_pairs=64 accumulator_patterns=8 arithmetic_flags=32 pages=2 snapshots=3 reads=2 data_writes=0 signed_operands=1 source_equals_destination=1 t_publish=Read accumulator_sample=Execute physical_banks=unvalidated interrupt_repeat_composition=unvalidated",
        "Fatal error: TMS320C54x isolated dual MAC complete",
    ),
    "mac-dual-edge": (
        "TMS320C54x dual MAC exclusions: PASS cases=16 restore=16 reads=0 pointer_updates=0 register_publication=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated dual MAC exclusions complete",
    ),
    "repeat-rearm": (
        "TMS320C54x adjacent block rearm: PASS cases=64 restore=64 first_counts=0,1,3,7 second_counts=0,2 pages=2 snapshots=4 brc_publications=1 actual_iterations=1 write_owner=Access interrupt_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated adjacent block rearm complete",
    ),
    "rearm-edge": (
        "TMS320C54x in-body STM BRC exclusion: PASS cases=8 restore=8 pages=2 snapshots=4 write_owner=Access publications=0 body_effects=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated in-body STM BRC exclusion complete",
    ),
    "mar-reverse": (
        "TMS320C54x staged reverse-carry MAR: PASS cases=3072 restore=3072 directions=2 auxiliary_registers=8 compatibility_states=2 power_of_two_indices=8 starts=2 pages=2 snapshots=3 publication=Access data_accesses=0 alias_AR0=1 duplicate_updates=0 independent_bit_serial_oracle=1 producer_latency=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated reverse-carry MAR complete",
    ),
    "rptb-bcd": (
        "TMS320C54x delayed internal block branches: PASS cases=3072 restore=3072 predicates=12 counts=0,1,2,7 patterns=4 delay_layouts=2 pages=2 snapshots=4 ordered_sources=1 selected_writes=1 delay_decision_retained=1 actual_iterations=1 interrupt_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated delayed internal block branches complete",
    ),
    "asm-immediate": (
        "TMS320C54x immediate ASM consumers: PASS cases=9216 restore=9216 operations=LD,ADD,SUB shifts=32 patterns=2 sxm_ovm=4 gaps=0,1 pages=2 snapshots=3 latency=zero data_reads=0 data_writes=0 opcode_mutation_after_fetch=1 publication=Execute physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated immediate ASM consumers complete",
    ),
    "asm-store": (
        "TMS320C54x short ASM stores: PASS cases=122880 restore=122880 selectors=4 shifts=32 patterns=8 sxm_sst=4 forms=DP,SP,postincrement,indexed_sub,indexed_add pages=2 snapshots=3 source=Execute address_and_shift=Read writes=1 data_reads=0 asm_load_prefix=0 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated short ASM stores complete",
    ),
    "asm-store-ld": (
        "TMS320C54x short ASM stores: PASS cases=122880 restore=122880 selectors=4 shifts=32 patterns=8 sxm_sst=4 forms=DP,SP,postincrement,indexed_sub,indexed_add pages=2 snapshots=3 source=Execute address_and_shift=Read writes=1 data_reads=0 asm_load_prefix=1 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated short ASM stores complete",
    ),
    "shift-delay": (
        "TMS320C54x delayed shifted LD: PASS cases=110592 restore=110592 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD forms=DP,indirect,postincrement destinations=2 shifts=32 memory_values=4 sxm_ovm=4 pages=2 snapshots=3 source_reads=1 extension_fetches=1 pointer_updates=once target_observes_result=1 interrupt_composition=unvalidated physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated delayed shifted LD complete",
    ),
    "shift-delay-edge": (
        "TMS320C54x shifted-delay exclusions: PASS cases=16 restore=16 forms=spill,store_extension,arithmetic_extension,long_source pages=2 snapshots=2 data_reads=0 data_writes=0 pointer_updates=0 atomic_fallback=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated shifted-delay exclusions complete",
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
    "long-mvkd": (
        (
            "TMS320C54x staged long immediate ST: PASS cases=6144 forms=offset,preincrement,circular registers=8 offsets=4 values=4 compatibility=0,1 pages=2 restore=6144 duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0",
            "TMS320C54x staged long STL/STH: PASS cases=38400 forms=25 offsets=4 selectors=4 sst_sxm=4 patterns=4 compatibility=2 restore=38400 duplicate_writes=0 destination_reads=0",
            "TMS320C54x staged shifted STL/STH: PASS cases=172032 forms=7 offsets=4 selectors=4 shifts=32 sst_sxm=4 patterns=4 restore=172032 duplicate_writes=0 destination_reads=0 silicon_phase_claim=0",
            "TMS320C54x staged long MVDK: PASS cases=38400 forms=25 offsets=4 destinations=12 values=4 compatibility=2 restore=38400 source_alias=1 zero_gap_cmpr=1 duplicate_reads=0 duplicate_writes=0",
            "TMS320C54x staged original long MVDK pair: PASS cases=8 restore=8 stl_write=7 source_read=8 ar2_write=8 zero_gap_cmpr=1 duplicate_writes=0",
            "TMS320C54x staged long RMW: PASS cases=153600 operations=ADDM,ANDM,ORM forms=25 offsets=4 value_pairs=8 status_states=8 compatibility=2 restore=153600 duplicate_reads=0 duplicate_writes=0",
            "TMS320C54x staged long MVKD: PASS cases=19200 forms=25 offsets=4 sources=selected_AR,AR2,RAM,alias values=4 compatibility=2 pages=2 restore=19200 reads_each=1 writes_each=1 extensions_each=2 silicon_phase_claim=0",
        ),
        "Fatal error: TMS320C54x isolated long MVKD complete",
    ),
    "mapped-load-age": (
        "TMS320C54x staged mapped load age: PASS cases=18432 restore=18432 loads=LD,LDU forms=DP,indirect,absolute,offset registers=8 accumulators=2 values=4 younger=MAR_inc,MAR_dec,MVMM sxm=2 pages=2 snapshots=3 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated mapped load age complete",
    ),
    "ldm-ar-age": (
        "TMS320C54x staged LDM AR age: PASS cases=2304 restore=2304 registers=8 accumulators=2 values=4 younger=MAR_inc,MAR_dec,MVMM sxm=2 pages=2 snapshots=3 read=5 execute=6 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated LDM AR age complete",
    ),
    "st-mvmm": (
        "TMS320C54x staged store MVMM: PASS cases=1536 restore=1536 stores=1,2 source_registers=8 destination_registers=8 gap_nops=0,1 pages=2 snapshots=3 physical_phase_claim=0",
        "Fatal error: TMS320C54x isolated store MVMM complete",
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
        "TMS320C54x staged memory shift16: PASS cases=208896 restore=208896 forms=17 registers=8 selectors=8 patterns=8 sxm_ovm=4 compatibility=2 reads=1 data_writes=0 partial_carry=1 mapped_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated memory shift16 complete",
    ),
    "mvmd-status-irq": (
        "TMS320C54x staged MVMD status IRQ: PASS cases=576 restore=576 sources=IRQ,NMI intm_requests=4 memory_cannot_set_intm=1 clear_policy=legacy_unverified imr_states=2 arrivals=4,6,8 pages=2 snapshots=3 followers=STLM,STM source_reads=1 status_writes=1 accepted_frame=1 masked_frame=0 foreground_once=1 rsbx_guard_inherited=0 silicon_phase_claim=0",
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
    "mvdd-periph": (
        "TMS320C54x peripheral MVDD: PASS cases=1536 restore=1536 destinations=3 values=4 dual_modifiers=16 pages=2 snapshots=4 source_reads=1 peripheral_writes=1 extra_cycles=1 retained_address=1 retained_source=1 repeat_acceptance=0 irq_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated peripheral MVDD complete",
    ),
    "mvmd-t": (
        "TMS320C54x staged MVMD T/TRN: PASS cases=432 restore=432 sources=2 destinations=AR0_to_AR7,BK values=4 pages=2 snapshots=3 source_reads=1 destination_writes=1 producer_composition=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated MVMD T/TRN complete",
    ),
    "mvmd-acc": (
        "TMS320C54x staged MVMD accumulator: PASS cases=288 restore=288 sources=AL,BL producers=POPM,STL gaps=0,1,2 pages=2 values=4 source_reads=1 ram_writes=1 mapped_no_conflict=1 status_destination=unvalidated silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated MVMD accumulator complete",
    ),
    "subs": (
        "TMS320C54x staged SUBS: PASS cases=52224 restore=52224 forms=17 registers=8 accumulators=2 patterns=8 sxm_ovm=4 compatibility=2 source_unsigned=1 reads=1 data_writes=0 mapped_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated staged SUBS complete",
    ),
    "ret-load": (
        "TMS320C54x staged load return: PASS cases=64 restore=64 accumulators=2 values=2 gaps=2 pages=2 snapshots=4 producer_reads=1 conditional_stack_reads=1 data_writes=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated load return complete",
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
    "st-t-branch": (
        "TMS320C54x BC T/TRN stores: PASS cases=32 restore=32 predicates=TC,NTC initial_tc=2 sources=T,TRN pages=2 snapshots=2 writes=1 reads=0 delayed_acceptance=0 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated BC T/TRN stores complete",
    ),
    "st-t-long": (
        "TMS320C54x extended T/TRN stores: PASS cases=6144 restore=6144 forms=absolute,offset,premod,circular registers=8 compatibility=2 sources=T,TRN values=4 endpoints=RAM,peripheral pages=2 snapshots=3 destination=Read source=Execute literal_retained=1 pointer_updates=once destination_reads=0 writes=1 peripheral_extra_clock=1 silicon_phase_claim=0",
        "Fatal error: TMS320C54x isolated extended T/TRN stores complete",
    ),
    "st-t-long-edge": (
        "TMS320C54x extended T/TRN exclusions: PASS cases=6144 restore=6144 forms=4 registers=8 compatibility=2 sources=2 pages=2 snapshots=3 reads=0 writes=0 pointer_updates=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated extended T/TRN exclusions complete",
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
    "rpt-xfer-edge": (
        "TMS320C54x repeated transfer exclusions: PASS cases=9600 restore=9600 opcode_classes=6 forbidden_modes=25 repeat_counts=0,2 count_forms=short,long cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 stack_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated repeated transfer exclusions complete",
    ),
    "mmr-long-edge": (
        "TMS320C54x MMR long exclusions: PASS cases=4608 restore=4608 opcode_classes=9 forbidden_modes=25 reserved_modes=7 cmpt=2 pages=2 frontends=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 stack_changes=0 silicon_trap_claim=0",
        "Fatal error: TMS320C54x isolated MMR long exclusions complete",
    ),
    "mac-smem": (
        (
        "TMS320C54x staged MAC/MAS: PASS cases=73728 restore=73728 operations=MAC,MACR,MAS,MASR destinations=A,B forms=indirect_RAM,absolute_RAM,mapped_AR1 pairs=64 flags=16 source_reads=1 data_writes=0 t_retained=1 silicon_phase_claim=0",
        "TMS320C54x staged SQURA/SQURS: PASS cases=36864 restore=36864 operations=add,subtract destinations=A,B forms=indirect_RAM,absolute_RAM,mapped_AR1 pairs=64 flags=16 snapshots=3 source_reads=1 data_writes=0 t_publish=Read t_republish=0 repeat_acceptance=0 irq_acceptance=0 physical_phase_claim=0",
        "TMS320C54x staged POLY: PASS cases=4608 restore=4608 forms=direct,indirect,postincrement patterns=8 flags=8 repeat=none,0,2 pages=2 snapshots=4 coefficient_reads=iterations data_writes=0 mapped_acceptance=0 extended_acceptance=0 irq_acceptance=0 physical_phase_claim=0",
        "TMS320C54x staged accumulator T products: PASS cases=69120 restore=69120 opcodes=18 patterns=8 flags=16 layouts=none,STLM_zero_gap,STLM_one_gap,A_producer,T_successor pages=2 snapshots=3 t_sample=Read accumulator_sample=Execute overflow_initial=clear,sticky data_reads=0 producer_writes=0,1 opcode_fetches=1 repeat_acceptance=0 irq_acceptance=0 physical_phase_claim=0",
        "TMS320C54x staged Smem T dependencies: PASS cases=107520 restore=107520 opcodes=14 patterns=8 flags=16 layouts=5 sources=RAM,mapped_T pages=2 snapshots=3 source_reads=1 t_sample=Read repeat_acceptance=0 irq_acceptance=0 physical_phase_claim=0",
        "TMS320C54x staged remaining T dependencies: PASS cases=129792 restore=129792 POLY=2304 immediate=27648 absolute_Smem=64512 EXP_products=35328 patterns=8 layouts=6 pages=2 snapshots=3 poly_smul=excluded source_reads=0,1 literal_fetches=0,1 t_sample=Read repeat_acceptance=0 irq_acceptance=0 physical_phase_claim=0",
        ),
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


def check_result(text, returncode, *, bus_registers=False, bus_io=False, atomic_io=False, compose=False, bus_stm=False, mapped_read_a=False, dp_absolute=False, profile=None, verbose_teardown=False):
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
    machine = "tms54rom4" if args.profile == "cold-pipeline" else "tms54xtra" if args.profile in ("addm-ar", "addm-ar-load", "literal-guard", "mvpd-controls", "mvpd-ext-edge", "mvpd-extended", "st1-rules", "st1-bits", "reada-brc", "reada-brc16", "reada-brc-edge", "rptb-exit", "stlm-bsp", "stlm-bsp-edge", "status-bitf", "serial-stm", "clock-stm", "std-swwsr", "std-stm", "std-io", "std-io-atomic", "rpt-mvdp-irq", "rpt-mvdp", "mvdp-pair", "mvdp", "stlm-t", "ov-branch", "cond-reg-store", "saccd", "atomic-ret", "xorm", "xc-status", "xc-latency", "xc-barrier", "parallel-sub", "mmr-long-edge", "rpt-xfer-edge") else "tms54test"
    if args.profile in ("portr-circular", "portr-block", "shift-unowned", "st-t-branch", "st-t-long", "st-t-long-edge", "stm-acc", "rpt-mvkd-edge", "rpt-mvkd", "cmpm-acc", "tc-call", "store-circular", "sat", "shift-periph", "shift-bsp", "shift-per-edge", "stlm-st1", "stlm-st1-edge", "rotate", "rptz", "rpt-store", "rpt-store-edge", "bitt", "bitt-retd", "tc-return", "mar-circular", "bcd-arithmetic", "bcd-arith-edge", "signed-call", "rpt-writa", "rpt-writa16"):
        machine = "tms54xtra"
    if args.profile in ("mac-family", "mac-family-edge", "stmac", "stmac-edge", "sqdst", "sqdst-edge", "portr-bypass", "mvdk-circular", "mvdk-circ-edge", "mvdk-repeat", "mvdk-rpt-edge", "rptb-call", "rptb-call-edge", "rpt-mem-copy", "rpt-mem-wrap", "rpt-mem-chain", "addc", "addc-edge", "addc-chain", "repeat-stlm", "mvdm-acc", "mvdm-acc-edge", "stack-page0", "stack-page0-edge", "stld", "stld-edge", "unsigned-map", "unsigned-edge", "cmpm-map", "cmpm-map-edge"):
        machine = "tms54xtra"
    if args.profile in ("poll-interrupt", "rptb-tc-delay", "rcd", "addm-brc", "addm-brc-loop", "st-mvmm", "ldm-ar-age", "mapped-load-age", "mvmd-repeat", "rptbd-t-load", "rptb-exit-delay", "mvmd-rptz", "squr-smem"):
        machine = "tms54xtra"
    command = [str(args.mame.resolve()), machine, "-rompath", str(args.rompath.resolve()),
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
