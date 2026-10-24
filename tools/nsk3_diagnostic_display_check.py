"""Own NSK-3 physical Net monitor exit and independent cold idle acceptance."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_current_profile
from tools.nsk3_cold_input import prepare
from tools.nsk3_static_check import inspect
from tools.nsk3_pin_check import check_trace as check_pin
from tools.nsk3_security_check import check_trace as check_phone
from tools.nsk3_boot_check import check_upload_handoffs
from tools.nsk1_phonebook_roundtrip import frame_check

SEED_EEPROM = '7554f3abb2f69da09454f36042e7a8d4d91195c72d5da7d63045649f1d5cc25d'
SELECTED_EEPROM = '26b7dd4abc3ea4d9c712c470ebaf03eaf77d3c6b9e1077351ff8daa0f8503ae2'
IDLE = '192aec2de3cd98478979cc093ad09b2f7ad825bcfc29ac2162f64ba7f37ea462'
EARLY_IDLE = '9b62da95e67601d1d4124ebeef9c1f3a956cd96506a7de0d097f2fd79e806dc2'
KEYS = ('menu','up','menu','menu','zero','zero','menu','back','back')
FRAMES = (
    '2ab2adf527d11235a1ce8e0dfbc5b640e0f1011d70c7aadff363cd573b61ff9f',
    '3f97af4479f62c3d521f67a21e884851af35ff1bbd8c4f8eadebf92269ed194d',
    '9fcfcd4fe5f66d9d435d00fe87bd9a062047b0306469187f49bb4bc58c73e5a0',
    'a91940797ace29f2eab2c08c564a059ac958a3734ce6b11694da20c2fe34740d',
    'bfa1e8ad2eb20dbe8c07c7271330e56a5029e2c4a705a3274e349c6de3c2e2f5',
    'dc7b6b47ec4e129182ba13f556218a992b9cba097d998627ea8b2bfa02680d62',
    IDLE,
    '39e85cbd738c073fb2a7624a61fb2f09d0f14660c911ab2d7730c35f464ffeee',
    IDLE,
)
DESCRIPTORS = (
    (0x2c1778, '0000060403a90001'),
    (0x2c18e8, '0000072a19660021'), (0x2c18f0, '0000072b1ab00021'),
    (0x2c18f8, '0000072c1bfa0021'), (0x2c1908, '0000072e19660021'),
    (0x2c1910, '0000072f1ab00021'), (0x2c1918, '000007301bfa0021'),
)


def contract(image):
    for address, encoded in DESCRIPTORS:
        expected = bytes.fromhex(encoded)
        if image[address-0x200000:address-0x200000+len(expected)] != expected:
            raise ValueError(f'own NSK-3 descriptor differs at {address:#x}')
    if image[0xc7a30:0xc7a49] != bytes((62,62,62,62,62,17,25,1,2,3,14,23,4,5,6,15,24,7,8,9,16,26,12,10,11)):
        raise ValueError('own logical-key table differs')


def check_actions(text):
    rows = re.findall(r'nsk3_diagnostic_key: key=(\w+) down=([01]) t=[0-9.]+ physical_only=1', text)
    if (rows != [(key, state) for key in KEYS for state in ('1', '0')]
            or text.count('nsk3_diagnostic_key:') != len(rows)):
        raise ValueError('physical notice/diagnostic sequence differs')


def check_native(text, phase, count):
    rows = re.findall(r'nsk3_diagnostic_native: phase=(\w+) sample=(\d+) pc=([0-9a-f]{6}) stopped=([01]) idle=([01]) observer_writes=0', text)
    if ([(row[0],int(row[1]),row[3]) for row in rows] != [(phase,i,'0') for i in range(1,count+1)]
            or text.count('nsk3_diagnostic_native:') != count):
        raise ValueError('native compatibility observations missing or stopped on unsupported instruction')


def input_signature(path):
    system = ET.parse(path).find('system')
    if system is None or system.get('name') != 'nsk3r4t' or system.find('input') is None:
        raise ValueError('own input configuration missing')
    return [(node.tag, sorted(node.attrib.items()), (node.text or '').strip())
            for node in system.find('input').iter()]


def check_bank(bank, selected=False):
    digest = SELECTED_EEPROM if selected else SEED_EEPROM
    if len(bank) != 8192 or bank[0x3a9] != (0x3f if selected else 0xff) or hashlib.sha256(bank).hexdigest() != digest:
        raise ValueError('complete own EEPROM bank differs')


def check_link(seed, run):
    metadata = json.loads((run / 'cold_input.json').read_text())
    hashes = {name: hashlib.sha256((seed / 'nvram/nsk3r4t' / name).read_bytes()).hexdigest()
              for name in ('flash','eeprom','sim_card')}
    if (metadata.get('source') != str(seed.resolve()) or metadata.get('source_sha256') != hashes
            or metadata.get('copied_directories') != ['nvram','cfg']
            or metadata.get('ram_or_save_state_copied') is not False
            or metadata.get('fdn_fixture') is not False or metadata.get('fdn_adn_access') is not False):
        raise ValueError('own cold-storage link differs')
    for name in ('eeprom','sim_card'):
        if (run / f'initial_{name}.bin').read_bytes() != (seed / 'nvram/nsk3r4t' / name).read_bytes():
            raise ValueError('initial saved-device snapshot differs')


def check(source, seed, run, cold):
    _, image = inspect(source, 'boot4')
    contract(image)
    card = make_current_profile(True)
    seed_storage = seed / 'nvram/nsk3r4t'
    if (seed_storage / 'flash').read_bytes() != image or (seed_storage / 'sim_card').read_bytes() != card:
        raise ValueError('requires own flash and original PIN-enabled laboratory card')
    check_bank((seed_storage / 'eeprom').read_bytes())
    config = input_signature(seed / 'cfg/nsk3r4t.cfg')
    for directory, predecessor in ((run, seed), (cold, run)):
        check_link(predecessor, directory)
        if input_signature(directory / 'cfg/nsk3r4t.cfg') != config:
            raise ValueError('external hardware inputs changed')
        storage = directory / 'nvram/nsk3r4t'
        if (storage / 'flash').read_bytes() != image or (storage / 'sim_card').read_bytes() != card:
            raise ValueError('own executable or complete card changed')
        check_bank((storage / 'eeprom').read_bytes(), True)
        text = (directory / 'error.log').read_text()
        check_pin(text, True, tx_limit=4096)
        check_phone(text, True)
        check_upload_handoffs(text, 1)
        if 'nsk3_reset_request:' in text:
            raise ValueError('unexpected native reset')
        if json.loads((directory / 'exit_status.json').read_text())['returncode'] != 0:
            raise ValueError('MAME did not complete')
        if directory == run:
            check_actions(text)
            check_native(text, 'physical', 9)
        elif 'nsk3_diagnostic_key:' in text:
            raise ValueError('cold boot repeated the diagnostic selection')
        else:
            check_native(text, 'cold', 5)
    for index, digest in enumerate(FRAMES, 1):
        frame_check(run / f'snap/nsk3_diagnostic_{index:02}.png', digest)
    for time in (25,29,31,36,42):
        frame_check(cold / f'snap/nsk3_idle_{time}.png', EARLY_IDLE if time < 36 else IDLE)


def execute(binary, rompath, run, script, seconds=54):
    path = Path(__file__).with_name(script).resolve()
    command = [str(binary), 'nsk3r4t', '-rompath', str(rompath), '-noreadconfig',
               '-video','none','-sound','none','-joystickprovider','none','-midiprovider','none',
               '-nothrottle','-debug','-debugger','none','-log','-verbose',
               '-seconds_to_run',str(seconds),'-autoboot_delay','0','-autoboot_script',str(path)]
    (run / 'idle_invocation.json').write_text(json.dumps({
        'command': command, 'mame_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
        'script_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
    }, indent=2)+'\n')
    with (run / 'console.log').open('w') as output:
        result = subprocess.run(command, cwd=run, stdout=output, stderr=subprocess.STDOUT, timeout=180)
    (run / 'exit_status.json').write_text(json.dumps({'returncode': result.returncode})+'\n')
    if result.returncode:
        raise ValueError('MAME execution failed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source','seed','run','cold'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--rompath', type=Path, default=Path(__file__).resolve().parents[1] / 'roms')
    args = parser.parse_args()
    source, seed, run, cold = (getattr(args,name).resolve() for name in ('source','seed','run','cold'))
    try:
        if args.mame:
            if run.exists() or cold.exists():
                raise ValueError('both output directories must be fresh')
            check_bank((seed / 'nvram/nsk3r4t/eeprom').read_bytes())
            for target, predecessor, script in (
                    (run,seed,'nsk3_diagnostic_display_input.lua'), (cold,run,'nsk3_idle_input.lua')):
                prepare(source,predecessor,target)
                execute(args.mame.resolve(),args.rompath.resolve(),target,script)
        check(source,seed,run,cold)
    except (OSError,ValueError,KeyError,ET.ParseError,subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSK-3 diagnostic display FAIL: {error}\n')
    print('NSK-3 physical diagnostic exit/cold idle PASS; complete banks pinned; native radio/speech unproved')


if __name__ == '__main__':
    main()
