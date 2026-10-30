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
    selected_source = ROOT / ('tests/firmware/c54x_' + args.section + '.h') if args.section in ('descriptor_copy', 'byte_swap', 'block_swap', 'signed_branch', 'store_compare', 'conditional_store', 'stack_alignment', 'store_move_compare', 'xc_context', 'logic_composition') else sections / (args.section + '.h')
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
