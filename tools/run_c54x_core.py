#!/usr/bin/env python3
"""Run retained copied/acquired firmware fixtures, not generic CPU acceptance."""
import argparse
from pathlib import Path
import re
import sys
import tempfile

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.process_runner import run_emulator
from tools.c54x_profiles import machine_for_profile
from tools.c54x_test_oracles import EXTRA_PROFILES


def check_result(text, returncode, *, profile='rom4-context', verbose_teardown=False):
    if profile not in EXTRA_PROFILES:
        raise ValueError(f'unknown firmware profile: {profile}')
    if returncode != 3:
        raise ValueError(f'unexpected MAME exit status {returncode}; expected completion exit 3')
    if re.search(r'^c54x_failure:|^Driver .+: [1-9]\d* errors?,|invalid BIOS|reverting to default',
                 text, re.MULTILINE | re.IGNORECASE):
        raise ValueError('firmware assertion, validity or BIOS-selection failure')
    lines = text.splitlines()
    if verbose_teardown and lines[-2:] == ['Enter sdlwindow_exit', 'Leave sdlwindow_exit']:
        lines = lines[:-2]
    marker, completion = EXTRA_PROFILES[profile]
    if lines.count(marker) != 1:
        raise ValueError('missing or duplicated firmware result')
    if not lines or lines[-1] != completion:
        raise ValueError('missing terminal firmware completion')
    if 'Errors:' in lines or any(line.startswith('Fatal error:') and line != completion for line in lines):
        raise ValueError('unexpected firmware fatal/validity error')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mame', type=Path, required=True)
    parser.add_argument('--rompath', type=Path, required=True)
    parser.add_argument('--output-root', type=Path, default=Path('tmp'))
    parser.add_argument('--verbose', action='store_true')
    parser.add_argument('--profile', choices=sorted(EXTRA_PROFILES), default='rom4-context',
                        help='explicit firmware scope; generic CPU sections use run_c54x_section.py')
    args = parser.parse_args()
    try:
        machine = machine_for_profile(args.profile)
    except ValueError as error:
        parser.error(str(error))
    args.output_root.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='run_c54x_firmware_', dir=args.output_root.resolve()))
    command = [str(args.mame.resolve()), machine, '-bios', args.profile,
               '-rompath', str(args.rompath.resolve()), '-video', 'none', '-sound', 'none',
               '-joystickprovider', 'none', '-midiprovider', 'none', '-nothrottle',
               '-noreadconfig', '-log', '-seconds_to_run', '5']
    if args.verbose:
        command.append('-verbose')
    print(f'C54x firmware evidence: {run}', flush=True)
    with (run / 'console.log').open('w') as output:
        returncode = run_emulator(command, run, output)
    text = (run / 'console.log').read_text()
    print(text, end='')
    try:
        check_result(text, returncode, profile=args.profile, verbose_teardown=args.verbose)
    except ValueError as error:
        print(f'C54x firmware: FAIL: {error}', file=sys.stderr)
        return 1
    print(f'C54x firmware {args.profile}: PASS (not generic CPU or handset acceptance)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
