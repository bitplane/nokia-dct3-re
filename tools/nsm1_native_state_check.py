"""Own NSM-1 native idle save/load with full backed research RAM observation."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_current_profile
from tools.nse2_native_state_check import check_state
from tools.nsm1_v523_static_check import SHA1
from tools.nsm1_security_check import check_trace as check_phone, MENU
from tools.nsk3_pin_check import check_trace as check_pin
from tools.nsm1_published_record_check import records

SEED_EEPROM = '44493d3a303c99a1bc87d3320bc01bf292b7e18b581a2f17078159a12096987c'


def signature(path, *, machine='nsm1r4t'):
    if machine not in ('nsm1r4t','nsm1pcm'):
        raise ValueError('undeclared NSM-1 machine')
    system = ET.parse(path).find('system')
    if system is None or system.get('name') != machine or system.find('input') is None:
        raise ValueError('own NSM-1 external inputs missing')
    return [(node.tag, sorted(node.attrib.items()), (node.text or '').strip())
            for node in system.find('input').iter()]


def check_storage(directory, *, eeprom_sha256=None, machine='nsm1r4t'):
    if machine not in ('nsm1r4t','nsm1pcm'):
        raise ValueError('undeclared NSM-1 machine')
    if eeprom_sha256 is None:
        eeprom_sha256=SEED_EEPROM
    storage = directory / 'nvram' / machine
    files = {name: (storage / name).read_bytes() for name in ('flash', 'eeprom', 'sim_card')}
    if hashlib.sha1(files['flash']).hexdigest() != SHA1:
        raise ValueError('own executable differs')
    if (len(files['eeprom']) != 0x4000
            or hashlib.sha256(files['eeprom']).hexdigest() != eeprom_sha256):
        raise ValueError('complete qualified EEPROM differs')
    if files['sim_card'] != make_current_profile(True):
        raise ValueError('complete original PIN-enabled laboratory card differs')
    return files


def prepare(seed, run):
    if run.exists():
        raise ValueError('fresh output required')
    files = check_storage(seed)
    signature(seed / 'cfg/nsm1r4t.cfg')
    for name in ('nvram', 'cfg'):
        shutil.copytree(seed / name, run / name)
    for name in ('eeprom', 'sim_card'):
        (run / f'initial_{name}.bin').write_bytes(files[name])
    (run / 'cold_input.json').write_text(json.dumps(dict(
        source=str(seed.resolve()), source_sha256={
            name: hashlib.sha256(data).hexdigest() for name, data in files.items()},
        copied_directories=['nvram', 'cfg'], ram_or_save_state_copied=False), indent=2)+'\n')


def check_trace(text, before, after, reference, restored):
    if text.count('nsm1_state: phase=') != 2:
        raise ValueError('native snapshot count differs')
    result = check_state(text, before, after, reference, restored,
                         prefix='nsm1', checkpoint='40.000000000',
                         card_size=3570, menu_frame=MENU, ram_limit=0x120000)
    # Architecture is validated first; its ILLEGAL register name is data,
    # not an execution-error message. Every other line remains checked.
    protocol = re.sub(r'^(?:\[luaengine\] )?nsm1_state: phase=(?:saved|restored) t=[^\n]+\n?',
                      '', text, flags=re.M)
    check_pin(protocol, True, prefix='nsm1', tx_limit=4096)
    check_phone(protocol, True, ())
    records(protocol, True)
    markers = ('SIM status ins=20 sw=9000', 'nsm1_security_submit:',
               'nsm1_security_accepted:', 'nsm1_state: phase=saved',
               'nsm1_state_contact: phase=reference down=1')
    positions = [text.index(marker) for marker in markers]
    if positions != sorted(positions):
        raise ValueError('authentication/save/physical continuation order differs')
    return result


def check(seed, run):
    files = check_storage(seed)
    check_storage(run)
    metadata = json.loads((run / 'cold_input.json').read_text())
    if metadata != dict(source=str(seed.resolve()), source_sha256={
            name: hashlib.sha256(data).hexdigest() for name, data in files.items()},
            copied_directories=['nvram', 'cfg'], ram_or_save_state_copied=False):
        raise ValueError('complete cold-input provenance differs')
    for name in ('eeprom', 'sim_card'):
        if (run / f'initial_{name}.bin').read_bytes() != files[name]:
            raise ValueError('initial persistent bytes differ')
    if signature(seed / 'cfg/nsm1r4t.cfg') != signature(run / 'cfg/nsm1r4t.cfg'):
        raise ValueError('external inputs changed')
    if json.loads((run / 'exit_status.json').read_text())['returncode'] != 0:
        raise ValueError('MAME did not complete')
    pixels = []
    for phase in ('reference', 'restored'):
        with Image.open(run / f'snap/nsm1_state_{phase}.png') as image:
            if image.size != (84, 48):
                raise ValueError('own LCD geometry differs')
            pixels.append(image.convert('RGB').tobytes())
    result = check_trace((run / 'error.log').read_text(), files['sim_card'],
                         (run / 'nvram/nsm1r4t/sim_card').read_bytes(), *pixels)
    saved = run / 'sta/nsm1r4t/nsm1_native_ui.sta'
    if not saved.is_file() or not saved.stat().st_size:
        raise ValueError('saved artifact missing')
    result['save_sha256'] = hashlib.sha256(saved.read_bytes()).hexdigest()
    return result


def execute(binary, seed, run):
    prepare(seed, run)
    script = Path(__file__).with_name('nsm1_native_state.lua').resolve()
    command = [str(binary), 'nsm1r4t', '-rompath', str(Path(__file__).resolve().parents[1] / 'roms'),
               '-noreadconfig', '-video', 'none', '-sound', 'none', '-nothrottle',
               '-joystickprovider', 'none', '-midiprovider', 'none',
               '-debug', '-debugger', 'none', '-log', '-verbose', '-seconds_to_run', '54',
               '-autoboot_delay', '0', '-autoboot_script', str(script)]
    (run / 'state_invocation.json').write_text(json.dumps(dict(
        command=command, mame_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
        script_sha256=hashlib.sha256(script.read_bytes()).hexdigest()), indent=2)+'\n')
    with (run / 'console.log').open('w') as output:
        process = subprocess.run(command, cwd=run, stdout=output, stderr=subprocess.STDOUT, timeout=180)
    (run / 'exit_status.json').write_text(json.dumps(dict(returncode=process.returncode))+'\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('seed', type=Path)
    parser.add_argument('run', type=Path)
    parser.add_argument('--mame', type=Path)
    args = parser.parse_args()
    try:
        seed, run = args.seed.resolve(), args.run.resolve()
        if args.mame:
            execute(args.mame.resolve(), seed, run)
        result = check(seed, run)
    except (OSError, ValueError, KeyError, ET.ParseError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSM-1 native restoration FAIL: {error}\n')
    print(f"PASS: 128KiB research RAM, native architecture, {result['io_replay_events']} DSP I/O writes and physical Messages replay; RF/speech unproved")


if __name__ == '__main__':
    main()
