"""Build only the selected replacement section against the cached C54x core.

Overlays the isolated c54xisa target, not the legacy suite or handset build.
Each invocation records build/run wall times. Section names are explicit.
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def copy_changed(source, target):
    if not target.exists() or source.read_bytes() != target.read_bytes():
        shutil.copyfile(source, target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('section', choices=['immediate_store', 'basic_memory', 'stack', 'dagen'])
    parser.add_argument('--jobs', type=int, default=8)
    args = parser.parse_args()
    sections = ROOT / 'tests/c54x/sections'
    overlay = ROOT / 'mame/src/mame/nokia'
    output = ROOT / 'tmp/c54x-section-trial' / args.section
    output.mkdir(parents=True, exist_ok=True)
    for source in (ROOT / 'tests/c54x').glob('*.h'):
        copy_changed(source, overlay / source.name)
    copy_changed(sections / 'runner.cpp', overlay / 'tms320c54x_test_isa.cpp')
    copy_changed(sections / 'common.h', overlay / 'c54x_section_common.h')
    copy_changed(sections / (args.section + '.h'), overlay / 'c54x_section_selected.h')
    start = time.monotonic()
    with (output / 'build.log').open('w') as log:
        subprocess.run(['make', '-C', str(ROOT / 'mame'), 'REGENIE=1', 'SUBTARGET=c54xisa',
                        'SOURCES=src/mame/nokia/tms320c54x_test_isa.cpp', 'USE_QTDEBUG=0',
                        '-j' + str(args.jobs)], stdout=log, stderr=subprocess.STDOUT, check=True)
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
    if run.returncode != 3 or 'C54x section: PASS' not in text or 'C54x section complete' not in text:
        raise SystemExit('section failed')


if __name__ == '__main__':
    main()
