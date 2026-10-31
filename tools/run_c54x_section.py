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


def load_sections(root=ROOT):
    def unique(pairs):
        result = {}
        for name, count in pairs:
            if name in result:
                raise ValueError(f'duplicate C54x section: {name}')
            result[name] = count
        return result

    counts = json.loads((root / 'fixtures/c54x_sections.json').read_text(), object_pairs_hook=unique)
    result = {}
    for name, count in counts.items():
        if not name.replace('_', '').isalnum() or not isinstance(count, int) or isinstance(count, bool) or count < 1:
            raise ValueError(f'invalid C54x section/count: {name}')
        candidates = [root / 'tests/c54x/sections' / (name + '.h'), root / 'tests/firmware' / ('c54x_' + name + '.h')]
        sources = [path for path in candidates if path.is_file()]
        if len(sources) != 1:
            raise ValueError(f'C54x section needs exactly one source: {name}')
        result[name] = dict(cases=count, source=sources[0], scope='firmware' if sources[0].parent.name == 'firmware' else 'cpu')
    discovered = {path for path in (root / 'tests/c54x/sections').glob('*.h') if path.name != 'common.h'}
    discovered.update((root / 'tests/firmware').glob('c54x_*.h'))
    missing = discovered - {item['source'] for item in result.values()}
    if missing:
        raise ValueError('unregistered C54x sections: ' + ', '.join(str(path.relative_to(root)) for path in sorted(missing)))
    return result


def copy_changed(source, target):
    if not target.exists() or source.read_bytes() != target.read_bytes():
        shutil.copyfile(source, target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sections = load_sections()
    counts = {name: item['cases'] for name, item in sections.items()}
    parser.add_argument('section', nargs='?', choices=[*counts, 'all'])
    parser.add_argument('--list', action='store_true', help='list sections and expected counts without building')
    parser.add_argument('--scope', choices=('cpu', 'firmware', 'all'), default='cpu', help='scope for --list or all; private/copied firmware is opt-in')
    parser.add_argument('--jobs', type=int, default=8)
    parser.add_argument('--build-only', action='store_true', help='prepare and compile the selected section without executing it')
    parser.add_argument('--negative-control', action='store_true', help='require failure of an intentionally wrong expectation')
    args = parser.parse_args()
    if args.build_only and args.negative_control:
        parser.error('--negative-control must execute the section')
    selected = [name for name, item in sections.items() if args.scope == 'all' or item['scope'] == args.scope]
    if args.list:
        print(json.dumps({name: dict(cases=counts[name], source=str(sections[name]['source'].relative_to(ROOT)), scope=sections[name]['scope']) for name in selected}, indent=2))
        return
    if args.section is None:
        parser.error('select a section, all, or --list')
    if args.section == 'all':
        if args.build_only:
            parser.error('--build-only requires one section')
        if args.negative_control:
            parser.error('select one section for the negative control')
        for section in selected:
            subprocess.run([sys.executable, __file__, section, '--jobs', str(args.jobs)], check=True)
        return
    selected_source = sections[args.section]['source']
    sections = ROOT / 'tests/c54x/sections'
    overlay = ROOT / 'mame/src/mame/nokia'
    output = ROOT / 'tmp/c54x-section-trial' / (args.section + ('-negative' if args.negative_control else ''))
    output.mkdir(parents=True, exist_ok=True)
    for source in (ROOT / 'tests/c54x').glob('*.h'):
        copy_changed(source, overlay / source.name)
    copy_changed(sections / 'runner.cpp', overlay / 'tms320c54x_test_isa.cpp')
    copy_changed(sections / 'common.h', overlay / 'c54x_section_common.h')
    copy_changed(ROOT / 'driver/tms320c54x_bsp_state.h', overlay / 'tms320c54x_bsp_state.h')
    if args.section in ('mcbsp_control', 'deep_idle_clocks', 'clock_rates'):
        for name in ('tms320c54x_mcbsp.h', 'tms320c54x_mcbsp.cpp'):
            copy_changed(ROOT / 'driver' / name, overlay / name)
    if args.section in ('idle_dma', 'clock_rates'):
        for name in ('tms320c54x_dma.h', 'tms320c54x_dma.cpp'):
            copy_changed(ROOT / 'driver' / name, overlay / name)
    copy_changed(selected_source, overlay / 'c54x_section_selected.h')
    if args.negative_control:
        selected = overlay / 'c54x_section_selected.h'
        text = selected.read_text()
        if text.count('return cases;') != 1:
            parser.error('negative control requires one section return')
        selected.write_text(text.replace('return cases;',
            'cases.front().states.back().observation.expected.push_back({r::STATE_ILLEGAL,1}); return cases;'))
    start = time.monotonic()
    sources = 'src/mame/nokia/tms320c54x_test_isa.cpp'
    if args.section in ('mcbsp_control', 'deep_idle_clocks', 'clock_rates'):
        sources += ',src/mame/nokia/tms320c54x_mcbsp.cpp'
    if args.section in ('idle_dma', 'clock_rates'):
        sources += ',src/mame/nokia/tms320c54x_dma.cpp'
    with (output / 'build.log').open('w') as log:
        build = subprocess.run(['make', '-C', str(ROOT / 'mame'), 'REGENIE=1', 'SUBTARGET=c54xisa',
                        'SOURCES=' + sources, 'USE_QTDEBUG=0',
                        '-j' + str(args.jobs)], stdout=log, stderr=subprocess.STDOUT)
    if build.returncode:
        print('\n'.join((output / 'build.log').read_text().splitlines()[-30:]), file=sys.stderr)
        raise SystemExit('section build failed')
    built = time.monotonic()
    if args.build_only:
        result = dict(section=args.section, build_seconds=round(built-start,3), executed=False)
        (output / 'timing.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result))
        return
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
