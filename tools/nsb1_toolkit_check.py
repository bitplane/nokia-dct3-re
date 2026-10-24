"""Qualify own NSB-1 DISPLAY TEXT or its matched Phase-2 card control."""
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
from tools.nsb1_pin_roundtrip import check_correct
from tools.nsb1_rom4_compare import check_card_run, check_cold_link, check_frame, run_mame
from tools.nsb1_toolkit_prepare import prepare, profile_metadata
from tools.sim_fdn_protocol_check import apdus
from tools.sim_security_trace_check import statuses

DISPLAY = bytes.fromhex('d0148103012180820281028d0904') + b'DCT3 SAT'
RESPONSE = bytes.fromhex('810301218002028281030100')
TERMINAL_PROFILE = bytes.fromhex('0f41ff7700')
DISPLAY_FRAME = 'c390ab782b18d054bb4e134ee5bf6b62e99cee154a90b986ec139db956bd0b64'
DISMISSED_FRAME = '8fe18df5185a637968b1e15fe088b069c1978d70688e6cd4ccf0eae1e587eef2'


def wire(text, direction):
    values = re.findall(r'nsb1_sim_wire: direction=' + direction + r' data=([0-9a-f]{2}) ', text)
    if not values or len(values) >= 8192:
        raise ValueError('missing or capped SIM wire observation')
    return bytes.fromhex(''.join(values))


def check_trace(text, control=False):
    tx, rx = wire(text, 'TX'), wire(text, 'RX')
    commands = list(apdus(tx))
    if any(row[0] not in (0xa4, 0x20, 0xb0, 0xb2, 0xc0, 0xf2, 0x10, 0x12, 0x14)
           for row in commands):
        raise ValueError('unrelated SIM operation')
    contacts = re.findall(r'nsb1_toolkit_physical: action=dismiss down=([01]) t=([0-9.]+)', text)
    if contacts != [('1', '68.000000000'), ('0', '68.200000000')]:
        raise ValueError('physical dismissal differs')
    if any(marker in text for marker in ('nsb1_reset_reason:', 'LUA ERROR', 'Error in')):
        raise ValueError('reset or failed observer')
    toolkit = [row[:5] for row in commands if row[0] in (0x10, 0x12, 0x14)]
    expected = [] if control else [
        (0x10, 0, 0, 5, TERMINAL_PROFILE),
        (0x12, 0, 0, len(DISPLAY), b''),
        (0x14, 0, 0, len(RESPONSE), RESPONSE),
    ]
    if toolkit != expected:
        raise ValueError('own terminal profile, FETCH or terminal response differs')
    phase = '02' if control else '03'
    if f'read-binary fid=6fae offset=0 length=1 first={phase}' not in text:
        raise ValueError('card phase was not read')
    for instruction in (0x10, 0x14):
        expected_status = [] if control else [(0x9000, 3, 10)]
        if statuses(text, instruction) != expected_status:
            raise ValueError('Toolkit completion status differs')
    if control:
        if 'SIM completion ins=f2 sw=91' in text or DISPLAY in rx:
            raise ValueError('Phase-2 control produced proactive traffic')
    else:
        if text.count('SIM completion ins=f2 sw=9116') != 1 or rx.count(DISPLAY + b'\x90\x00') != 1:
            raise ValueError('missing unique pending STATUS or card-originated payload')
        before = text.split('nsb1_toolkit_physical: action=dismiss down=1', 1)[0]
        if DISPLAY + b'\x90\x00' not in wire(before, 'RX'):
            raise ValueError('dismissal precedes proactive payload')
        if any(row[0] == 0x14 for row in apdus(wire(before, 'TX'))):
            raise ValueError('terminal response precedes physical dismissal')


def check_configuration(source, run, profile):
    path = run / 'cfg/nsb1r4t.cfg'
    tree = ET.parse(path)
    inputs = tree.find('./system/input')
    if inputs is None:
        raise ValueError('missing input configuration')
    ports = inputs.findall("port[@tag=':SATCFG']")
    # MAME omits a default-valued configuration field when saving its CFG.
    if ports or profile != 0:
        if len(ports) != 1 or ports[0].attrib != dict(
                tag=':SATCFG', type='CONFIG', mask='15', defvalue='0', value=str(profile)):
            raise ValueError('external Toolkit input differs')
        inputs.remove(ports[0])
    original = ET.parse(source / 'cfg/nsb1r4t.cfg')
    if ET.tostring(tree.getroot()) != ET.tostring(original.getroot()):
        raise ValueError('unrelated handset configuration changed')
    manifest = json.loads((run / 'input_manifest.json').read_text())
    if manifest.get('external_toolkit_card') != profile_metadata(profile):
        raise ValueError('external card provenance differs')


def check(source, run, control=False):
    check_cold_link(run, source)
    check_configuration(source, run, 0 if control else 1)
    code = json.loads((run / 'exit_status.json').read_text())['returncode']
    text, card = check_card_run(run, code, 74)
    if card != make_current_profile(True):
        raise ValueError('complete persistent laboratory card changed')
    check_correct(text, card, run / 'snap')
    check_trace(text, control)
    if not control:
        check_frame(run / 'snap/nsb1_toolkit_display.png', DISPLAY_FRAME)
        # This is the pre-existing corrupted idle content, not a clean-idle claim.
        check_frame(run / 'snap/nsb1_toolkit_dismissed.png', DISMISSED_FRAME)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('run', type=Path)
    parser.add_argument('--control', action='store_true')
    parser.add_argument('--mame', type=Path, help='prepare and execute a fresh isolated run')
    args = parser.parse_args()
    source, run = args.source.resolve(), args.run.resolve()
    try:
        if args.mame:
            prepare(source, run, 0 if args.control else 1)
            script = Path(__file__).with_name('nsb1_toolkit_input.lua').resolve()
            binary = args.mame.resolve()
            (run / 'toolkit_invocation.json').write_text(json.dumps({
                'mame': str(binary), 'mame_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                'script_sha256': hashlib.sha256(script.read_bytes()).hexdigest(),
                'seconds': 74, 'profile': 0 if args.control else 1,
            }, indent=2) + '\n')
            run_mame(binary, run / 'inputs', run, script, 74, True)
        check(source, run, args.control)
    except (OSError, ValueError, KeyError, ET.ParseError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSB-1 Toolkit FAIL: {error}\n')
    print('NSB-1 Toolkit PASS: ' + ('Phase-2 negative control' if args.control else
          'organic DISPLAY TEXT and physical response; native RF/speech unproved'))


if __name__ == '__main__':
    main()
