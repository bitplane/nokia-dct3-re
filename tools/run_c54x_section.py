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
    counts['signed_conditions'] = 384
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
    selected_source = ROOT / ('tests/firmware/c54x_' + args.section + '.h') if args.section in ('descriptor_copy', 'byte_swap', 'block_swap', 'signed_branch', 'store_compare', 'conditional_store', 'stack_alignment', 'store_move_compare') else sections / (args.section + '.h')
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
