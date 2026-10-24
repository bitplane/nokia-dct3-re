"""Qualify fixed physical-PIN delays, not a timing sweep or native DSP claim."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

STARTS = (7, 8, 10, 12, 20)
KEY = re.compile(r'8210_pin_physical: key=(Keypad [1-4]|Menu) t=([0-9.]+)')
REGISTERED = re.compile(r'Location Updating Accept acknowledged nr=1 t=([0-9.]+)')


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def check_leg(directory, start):
    acceptance = directory / 'acceptance.json'
    report = json.loads(acceptance.read_text())
    if (report.get('passed') is not True or report.get('machine') != 'nsm3hle' or
            report.get('scenario') != 'registration' or report.get('laboratory_carrier') != 823 or
            report.get('pin_start_seconds') != start or report.get('native_dsp_complete') is not False):
        raise ValueError('child acceptance does not qualify the requested own-model PIN delay')
    keys, registered, failures = [], [], 0
    with (directory / 'error.log').open() as stream:
        for line in stream:
            if '8210_pin_physical:' in line:
                match = KEY.search(line)
                if not match:
                    raise ValueError('malformed physical PIN observation')
                keys.append((match[1], float(match[2])))
            match = REGISTERED.search(line)
            if match:
                registered.append(float(match[1]))
            failures += 'directed timing failure published ' in line
    expected = [('Keypad ' + str(i + 1), start + i) for i in range(4)] + [('Menu', start + 4)]
    if keys != expected:
        raise ValueError('physical PIN keys or requested input times differ')
    if len(registered) != 1 or not start + 4 < registered[0] < 46:
        raise ValueError('registration is absent or precedes the physical PIN result')
    if failures != int(start != 7):
        raise ValueError('early/delayed negative-completion path differs')
    return dict(pin_start_seconds=start, pin_submit_seconds=start + 4,
                registered_seconds=registered[0], timing_failures=failures,
                acceptance_sha256=digest(acceptance), log_sha256=digest(directory / 'error.log'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    mame = (args.mame or root / 'mame/mame').resolve()
    sources = [mame, Path(__file__).resolve(), root / 'tools/run_noki8210_acceptance.py',
               root / 'tools/noki8210_security_input.lua', root / 'tools/noki8210_registration_input.lua']
    try:
        run.mkdir(parents=True, exist_ok=False)
        identities = {str(path): digest(path) for path in sources}
        commands, results = [], []
        for start in STARTS:
            leg = run / f'pin_{start}'
            command = [sys.executable, str(root / 'tools/run_noki8210_acceptance.py'), str(leg),
                       '--mame', str(mame), '--scenario', 'registration', '--pin-enabled',
                       '--pin-start', str(start), '--dcs1800']
            commands.append(command)
            with (run / f'pin_{start}.log').open('w') as log:
                subprocess.run(command, cwd=root, stdout=log, stderr=subprocess.STDOUT, check=True)
            results.append(check_leg(leg, start))
        if any(digest(path) != identities[str(path)] for path in sources):
            raise ValueError('binary or fixture source changed during the timing matrix')
        (run / 'acceptance.json').write_text(json.dumps(dict(
            machine='nsm3hle', passed=True, physical_pin_starts=list(STARTS),
            laboratory_carrier=823, commands=commands, source_sha256=identities,
            legs=results, native_dsp_complete=False, speech_tested=False,
            all_input_timings_claim=False), indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'8210 PIN-delay matrix FAIL: {error}\n')
    print('8210 physical PIN delays 7/8/10/12/20 and own DCS registration PASS; native DSP unproved')


if __name__ == '__main__':
    main()
