"""Two-process CCONT counter retention, not handset clock provisioning."""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess


def verify(text, stored):
    if stored != bytes((3, 1, 12, 1, 1, 12, 0, 0x50, 0)):
        raise ValueError('mapped RTC fixture did not persist its exact retained domain')
    if '[LUA ERROR]' in text or 'LUA error' in text:
        raise ValueError('runtime Lua failure')
    writes = re.findall(r'ccont_rtc: event=counter_write reg=([0-9a-f]+) data=([0-9a-f]+)', text)
    if writes != [('07', '00')]:
        raise ValueError('cold boot rewrote retained counters beyond its seconds reset')
    ticks = re.findall(r'ccont_rtc: event=second time=(\d+):(\d+):(\d+) day=(\d+)', text)
    if ticks != [('12', '01', '01', '1'), ('12', '01', '02', '1'), ('12', '01', '03', '1')]:
        raise ValueError('cold process did not resume the retained minute/day')
    for register, value in (('0b', '01'), ('0c', '0c')):
        if not re.search(rf'ccont_rtc: event=read reg={register} data={value}\b', text):
            raise ValueError('alarm latches did not survive cold restart')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    try:
        run.mkdir(parents=True, exist_ok=False)
        seed = run / 'seed_rtc'
        with (run / 'seed-console.log').open('w') as console:
            subprocess.run(['make', 'verify-ccont-rtc', f'RUN_DIR={run / "seed"}'],
                           cwd=root, stdout=console, stderr=subprocess.STDOUT,
                           check=True, timeout=300)
        stored = (seed / 'nvram/noki3210/ccont').read_bytes()
        cold = run / 'cold'
        cold.mkdir()
        shutil.copytree(seed / 'nvram', cold / 'nvram')
        command = [str(root / 'mame/mame'), 'noki3210', '-rompath', str(root / 'roms'),
                   '-nvram_directory', 'nvram', '-cfg_directory', 'cfg', '-noreadconfig',
                   '-verbose', '-log', '-video', 'none', '-sound', 'none', '-nothrottle',
                   '-seconds_to_run', '3']
        with (cold / 'console.log').open('w') as console:
            subprocess.run(command, cwd=cold, stdout=console, stderr=subprocess.STDOUT,
                           check=True, timeout=90)
        verify((cold / 'error.log').read_text(errors='replace'), stored)
        # Exercise the actual MAME loader, not a Python imitation of its parser.
        compatibility = {'legacy': stored[:4], 'truncated': stored[:6],
                         'invalid_armed': stored[:-1] + b'\x02',
                         'invalid_mask': stored[:7] + b'\x51\x00'}
        for name, payload in compatibility.items():
            directory = run / name
            shutil.copytree(seed / 'nvram', directory / 'nvram')
            (directory / 'nvram/noki3210/ccont').write_bytes(payload)
            with (directory / 'console.log').open('w') as console:
                subprocess.run(command, cwd=directory, stdout=console,
                               stderr=subprocess.STDOUT, check=True, timeout=90)
            text = (directory / 'error.log').read_text(errors='replace')
            console = (directory / 'console.log').read_text(errors='replace')
            minute = '01' if name == 'legacy' else '00'
            ticks = re.findall(r'ccont_rtc: event=second time=(\d+):(\d+):(\d+) day=(\d+)', text)
            if ticks != [('12', minute, f'0{s}', '1') for s in range(1, 4)]:
                raise ValueError(f'{name}: non-atomic load or lost legacy counters')
            rejected = 'Error reading NVRAM file' in console
            if rejected != (name != 'legacy'):
                raise ValueError(f'{name}: unexpected NVRAM loader acceptance')
            for register in ('0b', '0c'):
                if not re.search(rf'ccont_rtc: event=read reg={register} data=00\b', text):
                    raise ValueError(f'{name}: stale alarm latch')
        (run / 'acceptance.json').write_text(json.dumps({
            'result': 'pass', 'machine': 'noki3210',
            'fixture_class': 'MMIO conformance', 'retained_registers': '07..0d, upper IRQ mask, alarm armed',
            'stored_counter_bytes': stored.hex(), 'host_time_advance': False,
            'handset_clock_provisioning_proven': False,
            'legacy_and_atomic_failure_cases': list(compatibility),
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'CCONT counter retention FAIL: {error}; inspect {run}\n')
    print('CCONT two-process counter retention PASS; handset clock provisioning unproved')


if __name__ == '__main__':
    main()
