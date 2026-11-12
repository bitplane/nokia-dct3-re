"""Build only the selected replacement section against the cached C54x core.

Refreshes CPU sources and the isolated c54xisa fixture, not handset fixtures.
Each invocation records build/run wall times. Section names are explicit.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
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


def refresh_cpu_overlay(root=ROOT):
    target = root / 'mame/src/devices/cpu/tms320c54x'
    for source in sorted((root / 'cpu/tms320c54x').iterdir()):
        if source.suffix in ('.cpp', '.h'):
            copy_changed(source, target / source.name)


def batch_compatible(section):
    """Only standalone base-machine sections can share a translation unit."""
    if section.get('scope') != 'cpu' or section['cases'] > 64:
        return False
    source = section['source'].read_text()
    directives = re.findall(r'^\s*#(.*)$', source, re.M)
    return (directives == ['include "c54x_section_common.h"']
            and source.count('inline ') == 1
            and source.count('section_cases()') == 1)


def plan_batches(selected, sections, size):
    if not 1 <= size <= 8:
        raise ValueError('batch size must be between 1 and 8')
    batches, pending = [], []
    for name in selected:
        if size > 1 and batch_compatible(sections[name]):
            pending.append(name)
            if len(pending) == size:
                batches.append(pending)
                pending = []
        else:
            batches.append([name])
    if pending:
        batches.append(pending)
    order = {name: i for i, name in enumerate(selected)}
    return sorted(batches, key=lambda batch: order[batch[0]])


def batch_header(names):
    lines = ['#include <cstdlib>', '#include <stdexcept>', '#include <string_view>']
    for i, name in enumerate(names):
        lines += [f'#define section_cases section_cases_{name}',
                  f'#include "c54x_batch_{i}.h"', '#undef section_cases']
    lines += ['namespace c54x_test {',
              'inline std::vector<clocked_register_case> section_cases() {',
              'auto const *name = std::getenv("C54X_TEST_SECTION");',
              'if (!name) throw std::invalid_argument("Missing C54x batch section");']
    for name in names:
        lines.append(f'if (std::string_view(name) == "{name}") return section_cases_{name}();')
    lines += ['throw std::invalid_argument("Unknown C54x batch section");', '}', '}']
    return '\n'.join(lines) + '\n'


def run_suite(selected, sections, jobs, scope, root=ROOT, batch_size=8, coverage=False):
    output = root / 'tmp/c54x-section-trial' / (scope + '-summary.json')
    output.parent.mkdir(parents=True, exist_ok=True)
    started = time.monotonic()
    report = dict(scope=scope, selected=len(selected), completed=[], complete=False, passed_cases=0)
    try:
        for batch in plan_batches(selected, sections, batch_size):
            current = time.monotonic()
            report['active_section'] = ','.join(batch)
            output.write_text(json.dumps(report, indent=2) + '\n')
            arguments = batch if len(batch) == 1 else ['--batch', *batch]
            subprocess.run([sys.executable, __file__, *arguments, '--jobs', str(jobs),
                            *(['--opcode-coverage'] if coverage else [])], check=True)
            elapsed = time.monotonic() - current
            for name in batch:
                report['completed'].append(dict(section=name, cases=sections[name]['cases'],
                    wall_seconds=round(elapsed / len(batch), 3), build_group=batch))
                report['passed_cases'] += sections[name]['cases']
        report.pop('active_section', None)
        report['complete'] = True
    finally:
        report['passed_cases'] = sum(item['cases'] for item in report['completed'])
        report['wall_seconds'] = round(time.monotonic()-started, 3)
        output.write_text(json.dumps(report, indent=2) + '\n')
        print(f'C54x suite report: {output}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sections = load_sections()
    counts = {name: item['cases'] for name, item in sections.items()}
    parser.add_argument('section', nargs='?')
    parser.add_argument('--fixture-header',type=Path,help='explicit isolated firmware fixture, excluded from suite discovery')
    parser.add_argument('--fixture-cases',type=int,default=1,help='expected case count for --fixture-header')
    parser.add_argument('--batch', nargs='+', choices=list(counts), help='build up to eight compatible sections once; execute each separately')
    parser.add_argument('--batch-size', type=int, choices=range(1, 9), default=8, help='maximum small-section batch for all (1 disables batching)')
    parser.add_argument('--list', action='store_true', help='list sections and expected counts without building')
    parser.add_argument('--scope', choices=('cpu', 'firmware', 'all'), default='cpu', help='scope for --list or all; private/copied firmware is opt-in')
    parser.add_argument('--jobs', type=int, default=8)
    parser.add_argument('--build-only', action='store_true', help='prepare and compile the selected section without executing it')
    parser.add_argument('--negative-control', action='store_true', help='require failure of an intentionally wrong expectation')
    parser.add_argument('--opcode-coverage', action='store_true',
                        help='keep each section error.log with the core [opcov] records (-log -verbose)')
    args = parser.parse_args()
    if args.fixture_header:
        if (not args.section or args.section=='all' or not args.section.replace('_','').isalnum() or
                args.batch or args.list or args.fixture_cases<1 or not args.fixture_header.is_file()):
            parser.error('explicit fixture requires one named section, an existing header and positive case count')
        sections={args.section:dict(source=args.fixture_header.resolve(),cases=args.fixture_cases,scope='firmware')}
        counts={args.section:args.fixture_cases}
    elif args.section is not None and args.section not in (*counts,'all'):
        parser.error('unknown section: '+args.section)
    if args.batch and (args.section or args.build_only or args.negative_control or len(args.batch) > 8
                       or len(set(args.batch)) != len(args.batch)
                       or not all(batch_compatible(sections[name]) for name in args.batch)):
        parser.error('batch requires up to eight distinct compatible sections and no other execution mode')
    if args.build_only and args.negative_control:
        parser.error('--negative-control must execute the section')
    selected = [name for name, item in sections.items() if args.scope == 'all' or item['scope'] == args.scope]
    if args.list:
        print(json.dumps({name: dict(cases=counts[name], source=str(sections[name]['source'].relative_to(ROOT)), scope=sections[name]['scope']) for name in selected}, indent=2))
        return
    if args.section is None and not args.batch:
        parser.error('select a section, all, or --list')
    if args.section == 'all':
        if args.build_only:
            parser.error('--build-only requires one section')
        if args.negative_control:
            parser.error('select one section for the negative control')
        run_suite(selected, sections, args.jobs, args.scope, batch_size=args.batch_size,
                  coverage=args.opcode_coverage)
        return
    names = args.batch or [args.section]
    group = 'batch-' + hashlib.sha256(','.join(names).encode()).hexdigest()[:12] if args.batch else args.section
    selected_source = sections[names[0]]['source']
    sections = ROOT / 'tests/c54x/sections'
    overlay = ROOT / 'mame/src/mame/nokia'
    output = ROOT / 'tmp/c54x-section-trial' / (group + ('-negative' if args.negative_control else ''))
    output.mkdir(parents=True, exist_ok=True)
    refresh_cpu_overlay()
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
    if args.batch:
        for i, name in enumerate(names):
            copy_changed(ROOT / 'tests/c54x/sections' / (name + '.h'), overlay / f'c54x_batch_{i}.h')
        (overlay / 'c54x_section_selected.h').write_text(batch_header(names))
    else:
        copy_changed(selected_source, overlay / 'c54x_section_selected.h')
    if args.negative_control:
        selected = overlay / 'c54x_section_selected.h'
        selected.write_text('#define C54X_SECTION_NEGATIVE_CONTROL\n' + selected.read_text())
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
    if args.batch:
        (output / 'timing.json').write_text(json.dumps(dict(sections=names,
            build_seconds=round(built-start, 3)), indent=2) + '\n')
    if args.build_only:
        result = dict(section=args.section, build_seconds=round(built-start,3), executed=False)
        (output / 'timing.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result))
        return
    command = [str(ROOT / 'mame/c54xisa'), 'tms54isa', '-video', 'none', '-sound', 'none',
               '-joystickprovider', 'none', '-midiprovider', 'none', '-nothrottle',
               '-noreadconfig', '-seconds_to_run', '1']
    if args.opcode_coverage:
        command += ['-log', '-verbose']
    for name in names:
        run_section(command, output, name, counts[name], built-start, args.negative_control,
                    group if args.batch else None)


def run_section(command, output, name, count, build_seconds, negative=False, group=None):
    if group:
        output = ROOT / 'tmp/c54x-section-trial' / name
        output.mkdir(parents=True, exist_ok=True)
    started = time.monotonic()
    (output / 'error.log').unlink(missing_ok=True)
    env = os.environ.copy()
    env['C54X_TEST_SECTION'] = name
    run = subprocess.run(command, cwd=output, env=env, capture_output=True, text=True, timeout=30)
    text = run.stdout + run.stderr
    (output / 'console.log').write_text(text)
    result = dict(section=name, build_seconds=None if group else round(build_seconds,3), build_group=group,
                  run_seconds=round(time.monotonic()-started,3), exit_code=run.returncode)
    (output / 'timing.json').write_text(json.dumps(result, indent=2) + '\n')
    print(text, end='')
    print(json.dumps(result))
    if negative:
        if run.returncode != 3 or 'expected=1' not in text or 'C54x section: PASS' in text:
            raise SystemExit('negative control did not detect the wrong expectation')
        print('C54x negative control: PASS (wrong expectation rejected)')
    elif run.returncode != 3 or f'C54x section: PASS cases={count} restores={count}' not in text.splitlines() or 'C54x section complete' not in text:
        raise SystemExit('section failed')


if __name__ == '__main__':
    main()
