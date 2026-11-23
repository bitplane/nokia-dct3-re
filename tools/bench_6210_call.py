#!/usr/bin/env python3
"""How fast this host runs the native 6210 call, as a fraction of real time.

Runs the ROM6 lab cell's early incoming call (page at 18 s, Send at 20 s, End
at 30 s) unthrottled, with no research observers and no debugger: the work a
real SIP call needs. It runs once to 20 s and once to 30 s; the difference is
the connected call, ten emulated seconds of TCH/F speech both ways. A speed
of 1.0 or more means a live call can keep up with real time on this host.

CPU time is the figure to compare; wall time also counts other load.
"""
import argparse
import hashlib
import os
from pathlib import Path
import platform
import resource
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.make_npe3_record_profile import make_profile  # noqa: E402
from tools.run_npe3_native import configure_cell_power  # noqa: E402

MODEL = 'npe3ap5'
CONNECTED = 'npe3_tch_facch: kind=connect_acknowledge'


INPUTS = {  # paths under roms/, with SHA-1s; see docs/6210_sip_quickstart.md
    'noki6210/6210_556c.fls': '3d9ea319503e78ec69b60d72cda23e461e118ea9',
    'noki6210/6210 virgin eeprom 005fa000.fls': 'b3a527ede1be87bd715fb3741a81eef5bd422efa',
    'nsm3r6/nsm3_rom6_program.be16': 'd6b1f40eaa4b2c8a9f7adcb6cb5864d9cc1354fc',
    'nsm3r6/nsm3_rom6_program_page1.be16': '51178fc69b3662a8a05291149832f04a1a2c6032',
    'nsm3r6/nsm3_rom6_data.be16': '845133638256ed1f07e014437fdee3ececc4508f',
}


def input_errors(roms):
    """What is missing or different among the files the 6210 call needs."""
    errors = []
    for name, sha1 in INPUTS.items():
        path = roms / name
        if not path.is_file():
            errors.append(f'missing {path}')
        elif hashlib.sha1(path.read_bytes()).hexdigest() != sha1:
            errors.append(f'{path} is not the expected file (SHA-1 {sha1})')
    return errors


def host_summary():
    """Compiler aside, what makes two hosts' figures comparable."""
    cpu = platform.machine()
    try:
        for line in Path('/proc/cpuinfo').read_text().splitlines():
            if line.startswith('model name'):
                cpu = line.split(':', 1)[1].strip()
                break
    except OSError:
        pass
    revision = subprocess.run(['git', 'describe', '--always', '--dirty'], cwd=ROOT,
                              capture_output=True, text=True).stdout.strip() or 'unknown'
    return f'host: {cpu}, {os.cpu_count()} threads, {platform.system()} {platform.machine()}; source {revision}'


def prepare(base):
    errors = input_errors(ROOT / 'roms')
    if errors:
        raise SystemExit('the 6210 call needs its own inputs (docs/6210_sip_quickstart.md):\n  ' +
                         '\n  '.join(errors))
    flash = (ROOT / 'roms/noki6210/6210_556c.fls').read_bytes()
    pmm = (ROOT / 'roms/noki6210/6210 virgin eeprom 005fa000.fls').read_bytes()
    pmm = make_profile(flash, pmm, (ROOT / 'roms/nsm3r6/nsm3_rom6_data.be16').read_bytes())
    (base / 'nvram' / MODEL).mkdir(parents=True)
    (base / 'nvram' / MODEL / 'flash').write_bytes(flash.ljust(0x3fa000, b'\xff') + pmm)
    (base / 'roms').mkdir()
    (base / 'roms' / MODEL).symlink_to(ROOT / 'roms/nsm3r6', target_is_directory=True)


def run(mame, base, seconds):
    directory = base / f'run{seconds}'
    directory.mkdir()
    configure_cell_power(directory, MODEL, 'handset33', incoming_call=True, traffic_assignment=True,
                         call_ack=True, clear_call=True, sacch=True, speech=True)
    command = [str(mame), MODEL, '-rompath', f'{base / "roms"};{ROOT / "roms"}',
               '-nvram_directory', str(base / 'nvram'), '-cfg_directory', 'cfg', '-snapshot_directory', 'snap',
               '-noreadconfig', '-joystickprovider', 'none', '-midiprovider', 'none',
               '-keyboardprovider', 'none', '-mouseprovider', 'none', '-lightgunprovider', 'none',
               '-video', 'none', '-sound', 'none', '-nothrottle', '-log', '-autoboot_delay', '0',
               '-autoboot_script', str(ROOT / 'tools/npe3_call_bench.lua'),
               '-seconds_to_run', str(seconds), '-nonvram_save']
    environment = {k: v for k, v in os.environ.items() if not k.startswith('NOKIA_DCT3_')}
    before = resource.getrusage(resource.RUSAGE_CHILDREN).ru_utime
    start = time.monotonic()
    with (directory / 'console.log').open('w') as output:
        subprocess.run(command, cwd=directory, env=environment, stdout=output,
                       stderr=subprocess.STDOUT, check=True)
    wall = time.monotonic() - start
    cpu = resource.getrusage(resource.RUSAGE_CHILDREN).ru_utime - before
    return cpu, wall, (directory / 'error.log').read_text(errors='replace')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mame', type=Path, default=ROOT / 'mame/mame')
    parser.add_argument('--keep', action='store_true', help='keep the run directory')
    args = parser.parse_args()
    args.mame = args.mame.resolve()  # MAME runs from inside the run directory
    (ROOT / 'tmp').mkdir(exist_ok=True)
    base = Path(tempfile.mkdtemp(prefix='bench6210-call.', dir=ROOT / 'tmp'))
    try:
        prepare(base)
        cpu20, wall20, _ = run(args.mame, base, 20)
        cpu30, wall30, log = run(args.mame, base, 30)
        if CONNECTED not in log:
            raise SystemExit(f'the call did not connect; evidence in {base}')
        cpu, wall = cpu30 - cpu20, wall30 - wall20
        print(host_summary())
        print(f'boot to answer (20 emulated s): {cpu20:.2f} CPU s')
        print(f'connected call (10 emulated s): {cpu:.2f} CPU s, {wall:.2f} wall s')
        print(f'6210 call speed: {10 / cpu:.2f}x real time (CPU), {10 / wall:.2f}x (wall)')
    finally:
        if args.keep:
            print(f'run directory: {base}')
        else:
            shutil.rmtree(base, ignore_errors=True)


if __name__ == '__main__':
    main()
