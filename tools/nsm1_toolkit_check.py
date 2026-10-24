"""Own NSM-1 physical DISPLAY TEXT and matched Phase-2 control."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsm1_native_state_check import prepare as prepare_cold, check_storage, signature
from tools.nsm1_security_check import check_trace as check_phone, MENU
from tools.nsk3_pin_check import check_trace as check_pin
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_toolkit_display_check import check_display

PROFILE = bytes.fromhex('0b41ff7700')
PROMPT = '0b61ca941a012e9feffe2d13b834c720031b71295220fc32139decd034172b2c'
IDLE = '1d8403273fd916cd003bb18471343ed23d6a405a3b31ef2a165c5c0e276f3aa1'


def prepare(seed, run, control=False):
    prepare_cold(seed, run)
    path = run / 'cfg/nsm1r4t.cfg'
    tree = ET.parse(path)
    inputs = tree.find('./system/input')
    if inputs.find("port[@tag=':SATCFG']") is not None:
        raise ValueError('seed already overrides external card input')
    ET.SubElement(inputs, 'port', tag=':SATCFG', type='CONFIG',
                  mask='15', defvalue='0', value='0' if control else '1')
    tree.write(path, encoding='utf-8', xml_declaration=True)


def check_trace(text, control=False):
    check_pin(text, True, prefix='nsm1', tx_limit=4096)
    check_phone(text, True, ())
    check_display(text, prefix='nsm1', terminal_profile=PROFILE, control=control,
                  reset_markers=('nsm1_reset_request:', 'nsm1_restart_request:'))


def check_configuration(seed, run, control=False):
    expected = signature(seed / 'cfg/nsm1r4t.cfg')
    actual = signature(run / 'cfg/nsm1r4t.cfg')
    card = ('port', sorted(dict(tag=':SATCFG', type='CONFIG', mask='15',
                               defvalue='0', value='0' if control else '1').items()), '')
    # MAME omits the explicitly selected zero/default after saving CFG.
    if actual != expected + [card] and not (control and actual == expected):
        raise ValueError('unrelated external inputs changed')


def check(seed, run, control=False):
    files = check_storage(seed)
    check_storage(run)
    metadata = json.loads((run / 'cold_input.json').read_text())
    if metadata != dict(source=str(seed.resolve()), source_sha256={
            name: hashlib.sha256(data).hexdigest() for name, data in files.items()},
            copied_directories=['nvram', 'cfg'], ram_or_save_state_copied=False):
        raise ValueError('cold provenance differs')
    for name in ('eeprom', 'sim_card'):
        if (run / f'initial_{name}.bin').read_bytes() != files[name]:
            raise ValueError('initial complete storage differs')
    check_configuration(seed, run, control)
    if json.loads((run / 'exit_status.json').read_text()) != dict(returncode=0):
        raise ValueError('MAME did not complete')
    check_trace((run / 'error.log').read_text(), control)
    frame_check(run / 'snap/nsm1_toolkit_before.png', IDLE if control else PROMPT)
    frame_check(run / 'snap/nsm1_toolkit_after.png', MENU if control else IDLE)


def execute(binary, seed, run, control=False):
    prepare(seed, run, control)
    script = Path(__file__).with_name('nsm1_toolkit_input.lua').resolve()
    command = [str(binary), 'nsm1r4t', '-rompath', str(Path(__file__).resolve().parents[1] / 'roms'),
               '-noreadconfig', '-video', 'none', '-sound', 'none', '-nothrottle',
               '-joystickprovider', 'none', '-midiprovider', 'none',
               '-debug', '-debugger', 'none', '-log', '-verbose', '-seconds_to_run', '74',
               '-autoboot_delay', '0', '-autoboot_script', str(script)]
    (run / 'invocation.json').write_text(json.dumps(dict(command=command,
        mame_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
        script_sha256=hashlib.sha256(script.read_bytes()).hexdigest()), indent=2)+'\n')
    with (run / 'console.log').open('w') as output:
        result = subprocess.run(command, cwd=run, stdout=output,
                                stderr=subprocess.STDOUT, timeout=180)
    (run / 'exit_status.json').write_text(json.dumps(dict(returncode=result.returncode))+'\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('seed', type=Path)
    parser.add_argument('run', type=Path)
    parser.add_argument('--control', action='store_true')
    parser.add_argument('--mame', type=Path)
    args = parser.parse_args()
    try:
        seed, run = args.seed.resolve(), args.run.resolve()
        if args.mame:
            execute(args.mame.resolve(), seed, run, args.control)
        check(seed, run, args.control)
    except (OSError, ValueError, KeyError, ET.ParseError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSM-1 Toolkit FAIL: {error}\n')
    print('NSM-1 Toolkit PASS: ' + ('matched Phase-2 control' if args.control else
          'card-originated DISPLAY TEXT, physical dismissal and successful response'))


if __name__ == '__main__':
    main()
