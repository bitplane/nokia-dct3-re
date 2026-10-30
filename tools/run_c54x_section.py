"""Build only the selected replacement section against the cached C54x core.

Overlays the isolated c54xisa target, not the legacy suite or handset build.
Each invocation records build/run wall times. Section names are explicit.
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


def copy_changed(source, target):
    if not target.exists() or source.read_bytes() != target.read_bytes():
        shutil.copyfile(source, target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    counts = dict(immediate_store=8, basic_memory=18, stack=141, dagen=292, paired_store=64, register_load=80, memory_dp=48, data_stack=76, descriptor_copy=12, register_store=27, absolute_load=64, stack_control=108, stm_ram=1024, program_transfer=832, repeat_program=48, memory_repeat=96, absolute_add=512, immediate_context=16, stlm_address=256)
    counts['stlm_peripheral'] = 64
    counts['stlm_register'] = 896
    counts['byte_swap'] = 512
    counts['block_swap'] = 1048
    counts['block_edges'] = 16
    counts['delayed_block'] = 128
    counts['delayed_square'] = 4992
    counts['count_branch'] = 576
    counts['signed_branch'] = 12
    counts['offset_count_branch'] = 1152
    counts['signed_conditions'] = 389
    counts['compare_register'] = 140
    counts['store_compare'] = 3
    counts['modify_address'] = 96
    counts['conditional_store'] = 6
    counts['interrupt_mask'] = 12
    counts['status_bits'] = 128
    counts['paired_load'] = 64
    counts['immediate_add'] = 128
    counts['data_move'] = 40
    counts['compiler_stack'] = 40
    counts['stack_alignment'] = 32
    counts['address_pointer'] = 64
    counts['accumulator_program_read'] = 24
    counts['mapped_stack'] = 144
    counts['mapped_operand_limits'] = 45
    counts['direct_memory_modify'] = 96
    counts['register_copy'] = 330
    counts['offset_data_move'] = 9600
    counts['store_move_compare'] = 8
    counts['flag_branch'] = 240
    counts['ram_forwarding'] = 2432
    counts['opcode_literals'] = 9
    counts['interrupt_rmw'] = 13
    counts['fast_return'] = 16
    counts['branch_delay'] = 16
    counts['delayed_asm'] = 5120
    counts['far_return'] = 24
    counts['far_return_irq'] = 8
    counts['xc_irq'] = 4
    counts['xc_arithmetic'] = 16
    counts['xc_condition'] = 44
    counts['plain_branch'] = 16
    counts['conditional_delay'] = 32
    counts['plain_return'] = 16
    counts['call'] = 44
    counts['delayed_return'] = 8
    counts['far_branch'] = 10
    counts['far_call'] = 10
    counts['tc_return'] = 96
    counts['delayed_program_read'] = 128
    counts['signed_xc'] = 288
    counts['return_barrier'] = 12
    counts['xc_status'] = 64
    counts['xc_latency'] = 384
    counts['xc_context'] = 4
    counts['conditional_delayed_return'] = 224
    counts['atomic_conditional_return'] = 96
    counts['atomic_overflow_branch'] = 32
    counts['carry_branch'] = 8
    counts['repeat_status'] = 1
    counts['atomic_delayed_call'] = 9
    counts['conditional_control'] = 196
    counts['conditional_register_store'] = 2304
    counts['circular_mar'] = 8192
    counts['zero_bk'] = 208
    counts['indirect_popm'] = 4
    counts['copy_dependency'] = 192
    counts['repeated_store'] = 1024
    counts['repeat_zero'] = 128
    counts['repeat_nop'] = 30
    counts['repeat_rearm'] = 10
    counts['block_repeat_control'] = 8
    counts['clock_control'] = 17
    counts['accumulator_call'] = 2
    counts['instruction_examples'] = 50
    counts['shifted_arithmetic'] = 104
    counts['rounding'] = 64
    counts['square_difference'] = 40
    counts['firs_sum'] = 32
    counts['arithmetic_boundaries'] = 80
    counts['conditional_accumulator_store'] = 168
    counts['rounded_multiply'] = 104
    counts['store_shift'] = 120
    counts['smul_control'] = 192
    counts['delayed_signed_branch'] = 60
    counts['branch_arithmetic'] = 1536
    counts['bit_test_t'] = 320
    counts['delayed_bit_test'] = 2048
    counts['mapped_address_dependency'] = 128
    counts['mapped_addm'] = 1728
    counts['mapped_repeat_dependency'] = 32
    counts['multiword_decode'] = 5
    counts['logical_immediate'] = 480
    counts['memory_add_immediate'] = 256
    counts['memory_logic'] = 192
    counts['logic_composition'] = 5
    counts['bit_immediate'] = 512
    counts['parallel_load_multiply'] = 64
    counts['rounded_load'] = 64
    counts['accumulator_field_store'] = 144
    counts['store_double_load'] = 192
    counts['shifted_high_store'] = 128
    counts['direct_memory_compare'] = 64
    counts['sticky_overflow_branch'] = 576
    counts['bitfield_compare_branch'] = 160
    counts['offset_memory_test'] = 8
    counts['mapped_word_arithmetic'] = 768
    counts['software_interrupt'] = 128
    counts['compact_load'] = 28
    counts['stack_address_dependency'] = 24
    counts['transition_register'] = 20
    counts['compact_store'] = 640
    counts['asm_store_capture'] = 5120
    counts['asm_load'] = 6144
    counts['reverse_carry'] = 2690
    counts['frontend_drain'] = 6
    counts['interrupt_entry'] = 12
    counts['indexed_memory'] = 7680
    counts['smem_test_compare'] = 4352
    counts['smem_arithmetic'] = 4864
    counts['t_shift_load'] = 2336
    counts['shifted_ar_conflict'] = 128
    counts['atomic_xorm'] = 256
    counts['scalar_boundaries'] = 34
    counts['atomic_repeat_irq'] = 2
    counts['store_forwarding'] = 4192
    counts['stlm_bk_dependency'] = 64
    counts['asm_mapped'] = 2048
    counts['t_shift_arithmetic'] = 10304
    counts['t_store'] = 2944
    counts['mvdm_accumulator'] = 1152
    counts['mvdm_t_trn'] = 96
    counts['mvdm_ar'] = 192
    counts['move_delay_irq'] = 768
    counts['mvmd_ar'] = 96
    counts['reada_accumulator'] = 48
    counts['dual_memory_copy'] = 15360
    counts['branch_word_store'] = 8
    counts['mvmd_word_register'] = 144
    counts['push_sp_forwarding'] = 384
    counts['word_move_consumer'] = 320
    counts['delay_store'] = 480
    counts['move_brc'] = 460
    counts['indexed_mar'] = 1024
    counts['move_sp'] = 64
    counts['move_data'] = 104
    counts['load_status'] = 48
    counts['status_consumer'] = 128
    counts['repeat_prefix'] = 14
    counts['push_pop_forwarding'] = 256
    counts['page_zero_stack'] = 6528
    counts['load_return'] = 8
    counts['atomic_subtract'] = 384
    counts['long_memory_logic'] = 9408
    counts['mapped_compare'] = 6816
    counts['addc_chain'] = 512
    counts['load_t_multiply'] = 256
    counts['square_literals'] = 6
    counts['atomic_near_control'] = 10
    counts['long_literals'] = 33
    counts['atomic_mac_literals'] = 39
    counts['atomic_dual_address'] = 11
    counts['atomic_parallel_literals'] = 10
    counts['atomic_shift_literals'] = 25
    counts['atomic_register_shift'] = 36
    counts['atomic_register_logic'] = 20
    counts['atomic_product_literals'] = 21
    counts['atomic_conditional_control'] = 12
    counts['atomic_far_edges'] = 23
    counts['atomic_extended_interactions'] = 3
    counts['atomic_fast_return'] = 1
    counts['atomic_pending_far'] = 2
    counts['atomic_program_mapping'] = 13
    counts['coefficient_mac_literals'] = 6
    counts['atomic_memory_literals'] = 24
    counts['atomic_double_stores'] = 8
    counts['atomic_address_literals'] = 4
    counts['atomic_shifted_loads'] = 7
    counts['mapped_pshd'] = 48
    counts['repeated_pshd'] = 98
    counts['reada_brc'] = 3456
    counts['mvmm_producer'] = 8
    counts['xmem_bit'] = 3072
    counts['preincrement_store'] = 1280
    counts['status_store_consumers'] = 5376
    counts['repeated_mvkd'] = 816
    counts['delay_copy'] = 7424
    counts['mapped_load_age'] = 8064
    counts['atomic_stack_literals'] = 9
    counts['atomic_mapped_moves'] = 4
    counts['atomic_memory_tests'] = 10
    counts['atomic_zero_repeat'] = 3
    counts['atomic_program_transfers'] = 8
    counts['atomic_conditional_repeat'] = 4
    counts['atomic_transfer_literals'] = 34
    counts['atomic_mmr_burst'] = 2
    counts['atomic_memory_sequences'] = 3
    counts['extended_pop'] = 2704
    counts['extended_push'] = 2416
    counts['long_immediate_store'] = 1536
    counts['imr_store'] = 2304
    counts['atomic_absolute_literals'] = 30
    counts['atomic_condition_edges'] = 6
    counts['pmst_store'] = 4608
    counts['mvdp'] = 41
    counts['repeated_mvdp'] = 1040
    counts['write_a'] = 280
    counts['xmem_store'] = 32768
    counts['xmem_load_alu'] = 40960
    counts['atomic_alu_store_literals'] = 8
    counts['atomic_high_store_literals'] = 5
    counts['atomic_parallel_load_literals'] = 8
    counts['atomic_move_modify_literals'] = 9
    counts['atomic_logic_pointer_literals'] = 18
    counts['atomic_logic_program_literals'] = 5
    counts['atomic_accumulator_add_literals'] = 7
    counts['atomic_multiply_move_literals'] = 7
    counts['atomic_conditional_slot_literals'] = 10
    counts['atomic_mapped_load_sequences'] = 4
    counts['atomic_register_copy_sequences'] = 4
    counts['atomic_guard_conditions'] = 18
    counts['atomic_memory_alu_literals'] = 9
    counts['atomic_mmr_transfer_literals'] = 4
    counts['atomic_transfer_repeat_literals'] = 265
    counts['atomic_status_xf'] = 8
    counts['atomic_single_slot_conditions'] = 8
    counts['atomic_memory_pairs'] = 13
    counts['atomic_memory_address_literals'] = 28
    counts['atomic_memory_arithmetic'] = 45
    counts['atomic_dual_arithmetic_literals'] = 3
    counts['atomic_coefficient_fir_literals'] = 5
    counts['atomic_dual_add_sub_literals'] = 8
    counts['atomic_immediate_product_literals'] = 11
    counts['atomic_asm_arithmetic'] = 523
    counts['atomic_basic_repeat_sequences'] = 5
    counts['atomic_basic_control_sequences'] = 13
    counts['atomic_basic_alu_control'] = 10
    counts['atomic_basic_parallel_block'] = 9
    counts['atomic_accumulator_calls'] = 3
    counts['atomic_bus_timing'] = 3
    counts['atomic_port_timing'] = 15
    counts['atomic_mac_saturation'] = 7
    counts['atomic_port_marked_operations'] = 22
    counts['atomic_compatibility_memory'] = 9
    counts['atomic_port_marked_memory'] = 11
    counts['atomic_port_loads'] = 6
    counts['atomic_port_marked_transfers'] = 9
    counts['atomic_port_marked_counter'] = 8
    counts['atomic_absolute_arithmetic'] = 16
    counts['atomic_long_offset_literals'] = 14
    counts['atomic_memory_route_literals'] = 26
    counts['atomic_short_immediate'] = 520
    counts['atomic_port_delayed_branch'] = 6
    counts['atomic_port_register_examples'] = 6
    counts['atomic_port_low_store'] = 6
    counts['atomic_port_indirect_output'] = 5
    counts['atomic_port_control_examples'] = 6
    counts['atomic_offset_transports'] = 6
    counts['atomic_shift_peripheral_literals'] = 10
    counts['atomic_xc_word_boundary'] = 3
    counts['bio_atomic'] = 29
    counts['bio_pipeline'] = 32
    counts['port_read_destinations'] = 52
    counts['port_read_forwarding'] = 832
    counts['port_write_sources'] = 40
    counts['peripheral_logic'] = 108
    counts['peripheral_waits'] = 9
    counts['peripheral_cost_classes'] = 20
    counts['mapped_accumulator_move'] = 96
    counts['status_bitfield'] = 128
    counts['mapped_accumulator_arithmetic'] = 1152
    counts['mapped_accumulator_bitfield'] = 576
    counts['immediate_product'] = 4224
    counts['product_mapped_consumer'] = 1536
    counts['immediate_mapped_consumer'] = 480
    counts['rounded_byte_count'] = 8
    counts['accumulator_branch_targets'] = 324
    counts['nibble_composition'] = 64
    counts['long_accumulator_store'] = 3408
    counts['long_mvkd'] = 2504
    counts['long_memory_rmw'] = 2264
    counts['pmst_restore'] = 368
    counts['atomic_operand_order'] = 46
    counts['xc_return'] = 16
    counts['delayed_far_call'] = 81
    counts['software_intr_pipeline'] = 64
    counts['atomic_intm_follower'] = 3
    counts['stack_interrupt_order'] = 16
    counts['ifr_ack_pair'] = 32
    counts['poll_interrupt'] = 64
    counts['block_xc_interrupt'] = 56
    counts['atomic_xc_interrupt'] = 8
    counts['unsigned_imr_load'] = 64
    counts['accumulator_call_frames'] = 216
    parser.add_argument('section', choices=[*counts, 'all'])
    parser.add_argument('--jobs', type=int, default=8)
    parser.add_argument('--negative-control', action='store_true', help='require failure of an intentionally wrong expectation')
    args = parser.parse_args()
    if args.section == 'all':
        if args.negative_control:
            parser.error('select one section for the negative control')
        for section in counts:
            subprocess.run([sys.executable, __file__, section, '--jobs', str(args.jobs)], check=True)
        return
    sections = ROOT / 'tests/c54x/sections'
    overlay = ROOT / 'mame/src/mame/nokia'
    output = ROOT / 'tmp/c54x-section-trial' / (args.section + ('-negative' if args.negative_control else ''))
    output.mkdir(parents=True, exist_ok=True)
    for source in (ROOT / 'tests/c54x').glob('*.h'):
        copy_changed(source, overlay / source.name)
    copy_changed(sections / 'runner.cpp', overlay / 'tms320c54x_test_isa.cpp')
    copy_changed(sections / 'common.h', overlay / 'c54x_section_common.h')
    selected_source = ROOT / ('tests/firmware/c54x_' + args.section + '.h') if args.section in ('descriptor_copy', 'byte_swap', 'block_swap', 'signed_branch', 'store_compare', 'conditional_store', 'stack_alignment', 'store_move_compare', 'xc_context', 'logic_composition', 'nibble_composition') else sections / (args.section + '.h')
    copy_changed(selected_source, overlay / 'c54x_section_selected.h')
    if args.negative_control:
        selected = overlay / 'c54x_section_selected.h'
        text = selected.read_text()
        if text.count('return cases;') != 1:
            parser.error('negative control requires one section return')
        selected.write_text(text.replace('return cases;',
            'cases.front().states.back().observation.expected.push_back({r::STATE_ILLEGAL,1}); return cases;'))
    start = time.monotonic()
    with (output / 'build.log').open('w') as log:
        build = subprocess.run(['make', '-C', str(ROOT / 'mame'), 'REGENIE=1', 'SUBTARGET=c54xisa',
                        'SOURCES=src/mame/nokia/tms320c54x_test_isa.cpp', 'USE_QTDEBUG=0',
                        '-j' + str(args.jobs)], stdout=log, stderr=subprocess.STDOUT)
    if build.returncode:
        print('\n'.join((output / 'build.log').read_text().splitlines()[-30:]), file=sys.stderr)
        raise SystemExit('section build failed')
    built = time.monotonic()
    command = [str(ROOT / 'mame/c54xisa'), 'tms54isa', '-video', 'none', '-sound', 'none',
               '-joystickprovider', 'none', '-midiprovider', 'none', '-nothrottle',
               '-noreadconfig', '-seconds_to_run', '1']
    run = subprocess.run(command, cwd=output, capture_output=True, text=True, timeout=30)
    text = run.stdout + run.stderr
    (output / 'console.log').write_text(text)
    result = dict(section=args.section, build_seconds=round(built-start,3),
                  run_seconds=round(time.monotonic()-built,3), exit_code=run.returncode)
    (output / 'timing.json').write_text(json.dumps(result, indent=2) + '\n')
    print(text, end='')
    print(json.dumps(result))
    if args.negative_control:
        if run.returncode != 3 or 'expected=1' not in text or 'C54x section: PASS' in text:
            raise SystemExit('negative control did not detect the wrong expectation')
        print('C54x negative control: PASS (wrong expectation rejected)')
    elif run.returncode != 3 or f'C54x section: PASS cases={counts[args.section]} restores={counts[args.section]}' not in text or 'C54x section complete' not in text:
        raise SystemExit('section failed')


if __name__ == '__main__':
    main()
