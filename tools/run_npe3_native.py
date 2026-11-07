"""Bounded native 6210 compatibility checks, not fitted-mask qualification."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
if __package__ in (None, ''):
    sys.path.insert(0, str(ROOT))


def check_execution(text, loader, program, flash):
    """Reject incomplete initialization, restarts and unacquired low ROM reads."""
    required = {
        'npe3_native_composition: hle=0 staged=0 clock_policy=13000000 fitted_mask_claim=0 observer_writes=0': 1,
        'npe3_native_control_contract: status=10 release=04 execution=01 readback=05 observer_writes=0': 1,
        'npe3_native_verifier: result0=0000 result1=0006': 1,
        'npe3_native_entry: kind=reset': 2,
        'npe3_native_entry: kind=loader': 2,
        'npe3_native_entry: kind=resident': 1,
        'npe3_native_selftest: command=0d faults=00 flag=c4': 1,
        'npe3_native_coverage: missing_program_reads=0 observer_writes=0': 1,
    }
    for marker, count in required.items():
        if text.count(marker) != count:
            raise ValueError(f'native 6210 marker count changed: {marker}')
    terminals = re.findall(r'npe3_native_terminal: t=([\d.]+) arm=[\da-f]+ dsp=[\da-f]+ illegal=(\d+) idle=[01] pmst=[\da-f]+ observer_writes=0', text)
    if (len(terminals) != 1 or terminals[0][1] != '0' or 'Lua error' in text
            or re.search(r'staged.*unsupported|unsupported.*staged', text)):
        raise ValueError('native 6210 terminal execution is incomplete or illegal')
    if (loader != flash[0x24b7c:0x24b7c + 1258]
            or len(program) != 0x20000
            or program[0x590 * 2:0x736 * 2] != flash[0x1eac4:0x1eac4 + 844]):
        raise ValueError('native uploads do not match own 6210 MCU bytes')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=int, default=30)
    parser.add_argument('--scenario', choices=('menu', 'phonebook'), default='menu')
    parser.add_argument('--reset-policy', action='store_true',
                        help='add passive reset-policy and native ring observations')
    args = parser.parse_args()
    if not 24 <= args.seconds <= 60:
        parser.error('Menu check duration must be 24..60 seconds')
    flash = (ROOT / 'roms/noki6210/6210_556c.fls').read_bytes()
    pmm = (ROOT / 'roms/noki6210/6210 virgin eeprom 005fa000.fls').read_bytes()
    if (hashlib.sha1(flash).hexdigest() != '3d9ea319503e78ec69b60d72cda23e461e118ea9'
            or hashlib.sha1(pmm).hexdigest() != 'b3a527ede1be87bd715fb3741a81eef5bd422efa'):
        raise ValueError('NPE-3 scout requires pinned own MCU/PMM inputs')
    run = Path(tempfile.mkdtemp(prefix='npe3-native-', dir=ROOT / 'tmp'))
    print(f'Native6210 evidence: {run}', flush=True)
    storage = run / 'nvram/npe3r6'
    storage.mkdir(parents=True)
    (storage / 'flash').write_bytes(flash.ljust(0x3fa000, b'\xff') + pmm)
    roms = run / 'roms'
    roms.mkdir()
    (roms / 'npe3r6').symlink_to(ROOT / 'roms/nsm3r6', target_is_directory=True)
    observer = 'rom6_reset_policy_observe.lua' if args.reset_policy else 'npe3_native_observe.lua'
    command=[str(ROOT/'mame/mame'),'npe3r6','-rompath',str(roms)+';'+str(ROOT/'roms'),
        '-nvram_directory',str(run/'nvram'),'-cfg_directory','cfg','-snapshot_directory','snap',
        '-noreadconfig','-joystickprovider','none','-midiprovider','none','-keyboardprovider','none',
        '-mouseprovider','none','-lightgunprovider','none','-video','none','-sound','none','-nothrottle',
        '-debug','-debugger','none','-log','-autoboot_delay','0',
        '-autoboot_script',str(ROOT/'tools'/observer),'-seconds_to_run',str(args.seconds)]
    (run/'command.json').write_text(json.dumps(command,indent=2)+'\n')
    def execute(phase, scenario, seconds, persist=False):
        directory = run / phase
        directory.mkdir()
        cmd = command.copy()
        cmd[-1] = str(seconds)
        if not persist:
            cmd.append('-nonvram_save')
        if scenario != 'menu':
            cmd.append('-verbose')
        env = {k: v for k, v in os.environ.items() if not k.startswith('NOKIA_DCT3_')}
        env['NPE3_NATIVE_SCENARIO'] = scenario
        with (directory / 'console.log').open('w') as output:
            subprocess.run(cmd, cwd=directory, env=env, stdout=output,
                           stderr=subprocess.STDOUT, check=True, timeout=600)
        text = (directory / 'error.log').read_text(errors='replace')
        check_execution(text, (directory/'npe3_loader2.be16').read_bytes(),
                        (directory/'npe3_terminal_program.be16').read_bytes(), flash)
        for line in text.splitlines():
            if 'npe3_native_' in line:
                print(line)
        return directory, text
    # Save is at 28 s. This bounded gate ends before the separately observed
    # ~32 s DSP restart; --seconds 34 deliberately tests and rejects that restart.
    first, text = execute('primary', args.scenario,
                          30 if args.scenario == 'phonebook' else args.seconds,
                          args.scenario == 'phonebook')
    if args.scenario == 'phonebook':
        from PIL import Image
        from tools.run_noki6210_acceptance import check_phonebook
        card = storage / 'sim_card'
        before = card.read_bytes()
        cold, read_text = execute('cold', 'read', 27)
        with Image.open(cold / 'snap/6210_phonebook_read_contact.png') as frame:
            check_phonebook(text, read_text, before, frame)
        if before != card.read_bytes():
            raise ValueError('cold read changed retained SIM bytes')
        print('Native6210 phonebook: PASS bounded own uploads, physical save, cold readback')
    else:
        from PIL import Image
        from tools.run_noki6210_acceptance import check_frame, MENU_SHA256
        if ('npe3_native_physical: key=Menu' not in text
                or 'npe3_native_key: value=19' not in text):
            raise ValueError('native 6210 physical Menu was not decoded')
        with Image.open(first/'snap/npe3_after_menu.png') as frame:
            check_frame(frame, MENU_SHA256, 'native 6210 Messages menu')
        print('Native6210 Menu: PASS bounded native boot and physical input')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        raise SystemExit(f'Native6210 scout failed: {error}')
