"""Own NSK-3 DISPLAY TEXT capability refusal and matched Phase-2 control."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_current_profile
from tools.nsk3_static_check import inspect
from tools.nsk3_diagnostic_display_check import (
    FRAMES, IDLE, check_bank, check_link, contract, execute, input_signature,
)
from tools.nsk3_pin_check import check_trace as check_pin
from tools.nsk3_security_check import check_trace as check_phone
from tools.nsk3_boot_check import check_upload_handoffs
from tools.nsk3_toolkit_prepare import prepare, profile_metadata
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_toolkit_display_check import DISPLAY, REFUSAL, wire, check_display

TERMINAL_PROFILE = bytes.fromhex('03018000')


def check_trace(text, control=False):
    check_pin(text, True, tx_limit=4096)
    check_phone(text, True)
    check_upload_handoffs(text, 1)
    check_display(text, prefix='nsk3', terminal_profile=TERMINAL_PROFILE,
                  control=control, capability_refusal=True,
                  reset_markers=('nsk3_reset_request:',))
    native = re.findall(r'nsk3_toolkit_native: sample=(\d+) pc=([0-9a-f]{6}) stopped=([01]) idle=([01]) observer_writes=0', text)
    if ([(row[0], row[2]) for row in native] != [('1', '0'), ('2', '0')]
            or text.count('nsk3_toolkit_native:') != 2):
        raise ValueError('native observations missing or stopped')


def check_configuration(seed, run, profile):
    profile_metadata(profile)
    tree = ET.parse(run / 'cfg/nsk3r4t.cfg')
    inputs = tree.find('./system/input')
    if inputs is None:
        raise ValueError('own inputs missing')
    ports = inputs.findall("port[@tag=':SATCFG']")
    if ports or profile:
        if len(ports) != 1 or ports[0].attrib != dict(
                tag=':SATCFG', type='CONFIG', mask='15', defvalue='0', value=str(profile)):
            raise ValueError('external Toolkit input differs')
        inputs.remove(ports[0])
    signature = [(node.tag, sorted(node.attrib.items()), (node.text or '').strip())
                 for node in inputs.iter()]
    if (tree.find('system').get('name') != 'nsk3r4t'
            or signature != input_signature(seed / 'cfg/nsk3r4t.cfg')):
        raise ValueError('unrelated configuration changed')
    if json.loads((run / 'toolkit_input.json').read_text()) != profile_metadata(profile):
        raise ValueError('external card provenance differs')


def check(source, seed, run, control=False):
    _, image = inspect(source, 'boot4')
    contract(image)
    check_link(seed, run)
    check_configuration(seed, run, 0 if control else 1)
    for directory in (seed, run):
        storage = directory / 'nvram/nsk3r4t'
        if (storage / 'flash').read_bytes() != image or (storage / 'sim_card').read_bytes() != make_current_profile(True):
            raise ValueError('own executable or complete original card changed')
        check_bank((storage / 'eeprom').read_bytes(), True)
    if json.loads((run / 'exit_status.json').read_text())['returncode'] != 0:
        raise ValueError('MAME did not complete')
    check_trace((run / 'error.log').read_text(), control)
    frame_check(run / 'snap/nsk3_toolkit_display.png', IDLE)
    frame_check(run / 'snap/nsk3_toolkit_dismissed.png', FRAMES[0])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'seed', 'run'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--control', action='store_true')
    parser.add_argument('--mame', type=Path)
    args = parser.parse_args()
    source, seed, run = (getattr(args, name).resolve() for name in ('source', 'seed', 'run'))
    try:
        if args.mame:
            prepare(source, seed, run, 0 if args.control else 1)
            execute(args.mame.resolve(), Path(__file__).resolve().parents[1] / 'roms',
                    run, 'nsk3_toolkit_input.lua', 74)
        check(source, seed, run, args.control)
    except (OSError, ValueError, KeyError, ET.ParseError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSK-3 Toolkit FAIL: {error}\n')
    print('NSK-3 Toolkit PASS: ' + ('matched Phase-2 negative control' if args.control else
          'organic FETCH and result30 capability refusal, not DISPLAY TEXT support'))


if __name__ == '__main__':
    main()
