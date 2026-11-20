#!/usr/bin/env python3
"""Bounded native 8850 compatibility trial, not fitted-mask qualification.

nsm2r6 runs the 8850's own v5.31 MCU, PMM and DSP uploads against the
acquired 8210 ROM6 program/data with no DSP HLE or staged takeover. The gate
requires the own uploads (613-word second loader, 422-word resident prefix),
native verifier 0000/0006, the own class-74 self-test 0d with no faults, no
illegal DSP instruction or unacquired low-ROM read, and the 8850's reviewed
physical security-code, Messages, Inbox and Names frames.

It ends at 30 s: the firmware's packet-window policy (2f614a selector 1,
NV 0607=ff, reason 68) resets the phone at 31.96 s because the native DSP
publishes no radio packets without a cell. --seconds 34 deliberately shows
that restart.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
if __package__ in (None, ''):
    sys.path.insert(0, str(ROOT))
FLASH_SHA1 = '9f966787403b68a09530680ad911302403eb1521'
PMM_SHA1 = 'b09455302d98fbedf35072c9ecfd7721a04924b0'
LOADER2 = (0x11ae80, 613)        # flash offset, words (nsm2stage's source)
RESIDENT = (0x590, 0x736, 0x115368)  # program words installed before 2c75, flash offset


def check_execution(text, loader, program, flash):
    """Reject incomplete initialization, restarts, faults and illegal execution."""
    required = {
        'nsm2_native_composition: hle=0 staged=0 clock_policy=45500000 fitted_mask_claim=0 observer_writes=0': 1,
        'nsm2_native_verifier: result0=0000 result1=0006 observer_writes=0': 1,
        'nsm2_native_entry: kind=reset observer_writes=0': 2,
        'nsm2_native_entry: kind=loader observer_writes=0': 2,
        'nsm2_native_entry: kind=resident observer_writes=0': 1,
        '8850_service_reply class=74 command=0d status=00': 1,
        'nsm2_native_coverage: missing_program_reads=0 observer_writes=0': 1,
    }
    for marker, count in required.items():
        if text.count(marker) != count:
            raise ValueError(f'native 8850 marker count changed: {marker}')
    if 'nsm2_native_mcu_reset:' in text:
        raise ValueError('native 8850 firmware requested a software reset')
    terminals = re.findall(r'nsm2_native_terminal: t=[\d.]+ arm=[\da-f]+ dsp=[\da-f]+ illegal=(\d+) '
                           r'idle=[01] pmst=[\da-f]+ observer_writes=0', text)
    if terminals != ['0'] or '[LUA ERROR]' in text or 'Lua error' in text:
        raise ValueError('native 8850 terminal execution is incomplete or illegal')
    words = LOADER2[1] * 2
    if loader != flash[LOADER2[0]:LOADER2[0] + words]:
        raise ValueError('native second loader does not match own 8850 MCU bytes')
    resident = program[RESIDENT[0] * 2:RESIDENT[1] * 2]
    if len(program) != 0x20000 or resident != flash[RESIDENT[2]:RESIDENT[2] + 844]:
        raise ValueError('native resident prefix is not own 8850 MCU bytes')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--seconds', type=int, default=30)
    parser.add_argument('--mame', type=Path, default=ROOT / 'mame/mame')
    parser.add_argument('--keep', action='store_true')
    args = parser.parse_args()
    if not 29 <= args.seconds <= 60:
        parser.error('menu check duration must be 29..60 seconds')
    args.mame = args.mame.resolve()
    flash = (ROOT / 'roms/noki8850/8850v531.fls').read_bytes()
    pmm = (ROOT / 'roms/noki8850/8850 virgin eeprom 003d0000.fls').read_bytes()
    if hashlib.sha1(flash).hexdigest() != FLASH_SHA1 or hashlib.sha1(pmm).hexdigest() != PMM_SHA1:
        raise ValueError('nsm2r6 requires the pinned own 8850 MCU/PMM inputs')
    (ROOT / 'tmp').mkdir(exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='nsm2-native.', dir=ROOT / 'tmp'))
    roms = run / 'roms'
    roms.mkdir()
    (roms / 'nsm2r6').symlink_to(ROOT / 'roms/nsm3r6', target_is_directory=True)
    command = [str(args.mame), 'nsm2r6', '-rompath', f'{roms};{ROOT / "roms"}',
               '-nvram_directory', 'nvram', '-cfg_directory', 'cfg', '-snapshot_directory', 'snap',
               '-noreadconfig', '-video', 'none', '-sound', 'none', '-nothrottle',
               '-debug', '-debugger', 'none', '-log', '-autoboot_delay', '0',
               '-autoboot_script', str(ROOT / 'tools/nsm2_native_observe.lua'),
               '-seconds_to_run', str(args.seconds), '-nonvram_save']
    environment = {k: v for k, v in os.environ.items() if not k.startswith(('NOKIA_DCT3_', 'NSM2_NATIVE_'))}
    environment['NSM2_NATIVE_SCENARIO'] = 'menu'
    with (run / 'console.log').open('w') as output:
        subprocess.run(command, cwd=run, env=environment, stdout=output, stderr=subprocess.STDOUT,
                       check=True, timeout=1800)
    text = (run / 'error.log').read_text(errors='replace')
    check_execution(text, (run / 'nsm2_loader2.be16').read_bytes(),
                             (run / 'nsm2_terminal_program.be16').read_bytes(), flash)
    from tools.noki8850_staged_trace_check import check_navigation_frames, check_physical_inputs
    errors = check_physical_inputs(text) + check_navigation_frames(run / 'snap')
    if errors:
        raise ValueError('; '.join(errors))
    print(f'native 8850: verifier 0000/0006, self-test 0d/00, loader2 {LOADER2[1]} words, '
          f'resident prefix 422 words at flash {RESIDENT[2]:06x}')
    print('Native 8850 Menu: PASS own uploads on the acquired ROM6, physical security code, '
          'Messages/Inbox/Names reviewed frames')
    if args.keep:
        print(f'evidence retained: {run}')
    else:
        shutil.rmtree(run)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        raise SystemExit(f'Native 8850 trial failed: {error}')
