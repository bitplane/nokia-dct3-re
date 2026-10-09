#!/usr/bin/env python3
"""Own-storage, three-process NHM-3 forwarding subscription acceptance."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.run_noki6250_acceptance import prepare_run, check_supplementary
from tools.radio_call_divert_incoming_trace_check import verify as verify_routing


def check_query(text, active):
    expected = f'gsm_ss: request=interrogate transaction=1b invoke=1 service=21 active={int(active)}'
    if expected not in text or 'gsm_ss: request=register' in text:
        raise ValueError('cold query missing status or re-registered forwarding')
    cursor = 0
    for key in ('Keypad *', 'Keypad #', 'Keypad 2', 'Keypad 1', 'Keypad #', 'Send'):
        event = '6250_divert_physical: key=' + key
        index = text.find(event, cursor)
        if index < 0:
            raise ValueError('missing physical cold query key: ' + key)
        cursor = index + len(event)
    if expected not in text[cursor:]:
        raise ValueError('cold query precedes physical Send')
    after = text.split(expected, 1)[1]
    size = 21 if active else 17
    response = f'GSM service downlink kind=27 sapi=0 pd=0b message=2a length={size}'
    if response not in after:
        raise ValueError('cold query missing forwardingInfo response')
    tail = after.split(response, 1)[1]
    if 'radio_phase=release_deconfigure' not in tail:
        raise ValueError('cold query missing RR release')
    tail = tail.split('radio_phase=release_deconfigure', 1)[1]
    if '6250_divert_physical: key=Back' not in tail:
        raise ValueError('cold query missing physical idle recovery')


def check_frame(path, expected):
    from PIL import Image
    with Image.open(path) as frame:
        if frame.size != (96, 60) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != expected:
            raise ValueError('cold forwarding frame differs: ' + str(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path, required=True)
    parser.add_argument('--port', type=int, default=16251)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    mame = args.mame.resolve()
    try:
        run.mkdir(parents=True, exist_ok=False)
        retained, fresh = run / 'retained', run / 'fresh'
        prepare_run(retained, root)
        prepare_run(fresh, root)
        commands = []

        def execute(directory, phase, script, host=False):
            command = [str(mame), 'nhm3hle', '-rompath', f'{directory / "roms"};{root / "roms"}',
                       '-nvram_directory', 'nvram', '-cfg_directory', 'cfg', '-noreadconfig',
                       '-autoboot_script', str(root / 'tools' / script), '-autoboot_delay', '0',
                       '-seconds_to_run', '45', '-video', 'none', '-sound', 'none', '-nothrottle', '-log', '-verbose']
            if host:
                config = ET.Element('mameconfig', version='10')
                inputs = ET.SubElement(ET.SubElement(config, 'system', name='nhm3hle'), 'input')
                ET.SubElement(inputs, 'port', tag=':CALLHOST', type='CONFIG', mask='1', defvalue='0', value='1')
                ET.ElementTree(config).write(directory / 'cfg/nhm3hle.cfg', encoding='utf-8', xml_declaration=True)
                command += ['-http', '-http_port', str(args.port)]
                command = [sys.executable, str(root / 'tools/run_host_diverted_call_gate.py'),
                           '--cwd', str(directory), '--port', str(args.port),
                           '--ready-source', 'active-query', '--'] + command
            commands.append({'phase': phase, 'command': command})
            with (directory / f'{phase}-console.log').open('w') as output:
                subprocess.run(command, cwd=directory, stdout=output, stderr=subprocess.STDOUT, check=True,
                               env=os.environ.copy())
            shutil.copyfile(directory / 'error.log', directory / f'{phase}-error.log')
            text = (directory / 'error.log').read_text(errors='replace')
            if '[LUA ERROR]' in text or 'rom4_reset_request' in text:
                raise ValueError('cold fixture failed: ' + phase)
            return text

        registered = execute(retained, 'register', 'noki6250_divert_register_input.lua')
        if 'gsm_ss: request=register transaction=1b invoke=1 service=21 number_length=5 active=1' not in registered:
            raise ValueError('missing physical subscription registration')
        storage = retained / 'nvram/nhm3hle/gsm_network'
        before = storage.read_bytes()
        query = execute(retained, 'cold-query', 'noki6250_divert_input.lua', host=True)
        check_query(query, True)
        verify_routing(query)
        if storage.read_bytes() != before:
            raise ValueError('read-only cold interrogation changed subscription storage')
        check_frame(retained / 'snap/6250_divert_result.png',
                    '466a5a0241eb09e162c00227e8737eaddc5717251ec2663bf9a97b3c8c8c58d9')
        check_frame(retained / 'snap/6250_divert_after_back.png',
                    '1b71b66d25d97802d202d88e4eda7d3fd6a41778f636634e05855d9eb8c25419')
        negative = execute(fresh, 'fresh-query', 'noki6250_divert_input.lua')
        check_query(negative, False)
        check_supplementary(negative, fresh / 'snap', 'divert')
        (run / 'acceptance.json').write_text(json.dumps({'product': '6250', 'native_speech_claim': False,
            'cold_subscription_query': True, 'forwarded_host_call': True, 'fresh_negative': True,
            'commands': commands}, indent=2) + '\n')
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'6250 cold forwarding FAIL: {error}\n')
    print('6250 cold forwarding query/routing/fresh-negative PASS; native speech unproved')


if __name__ == '__main__':
    main()
