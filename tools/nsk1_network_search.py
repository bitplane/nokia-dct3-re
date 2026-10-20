"""Qualify physical NSK-1 manual search initiation, not network discovery."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

from tools.nsk1_phonebook_roundtrip import check_contacts, copy_persistent, frame_check
from tools.nsk1_radio_trace_check import check as check_radio
from tools.nsk1_security_compare import check_phase, run_machine


KEYS = ('menu','down','down','down','menu','menu','down',
        'menu','down','down','down','menu','down','menu')
FRAMES = {11:'c9aabfe51ca1026ae528e86dd9433e25e0a0be308bc440b78a88014f904c019b',
          12:'fba8e17607d1761861e12e2b65d7084a881b991f886c18cea297d526ab1613f3',
          13:'f21b2eadafcd1bf394fffa1c8e7a7f30b25dc75660a128cd057ecef554f2c9fb'}
SEARCH_BODY = 'ce500485d467bf333aadb2a4e45328267e44eeed8a47426f32df4f5ec8e126ea'


def check_trace(trace):
    summary = check_radio(trace)
    check_contacts(trace, 'nsk1_network_contact', KEYS)
    codes = [int(value, 16) for value in re.findall(r'nsk1_code_key: value=([0-9a-f]{2})', trace)]
    expected = [1,2,3,4,5,0x19,0x19] + [0x19 if key == 'menu' else 0x18 for key in KEYS]
    if codes != expected:
        raise ValueError('physical network-menu decoded key order differs')
    requests = re.findall(r'nsk1_radio_dynamic_request: input=([0-9a-f]{4}) object=([0-9a-f]{8}) field2=([0-9a-f]{2})', trace)
    if requests != [('05df','00000000','00'), ('05df','00000000','03')]:
        raise ValueError('manual-search context-clear requests differ')
    received = re.findall(r'nsk1_radio_completion_server_receive: object=[0-9a-f]{8} header=([0-9a-f]{6})[0-9a-f]{2}', trace)
    if received != ['05df00', '05df03']:
        raise ValueError('manual-search server delivery differs')
    replies = re.findall(r'nsk1_radio_task16_receive: object=[0-9a-f]{8} header=(05e8[0-9a-f]{2})[0-9a-f]{2}', trace)
    if replies != ['05e800', '05e803']:
        raise ValueError('manual-search context-clear replies differ')
    summary['physical_manual_search_initiated'] = True
    summary['operator_discovery_proved'] = False
    return summary


def check_run(run, initial, card):
    summary, trace = check_phase(run, initial, through=59, minimum_records=1)
    summary.update(check_trace(trace))
    if (run/'nvram/nsk1r4t/sim_card').read_bytes() != card:
        raise ValueError('manual search changed persisted SIM storage')
    for index, digest in FRAMES.items():
        frame_check(run/f'snap/nsk1_network_{index}.png', digest)
    from PIL import Image
    with Image.open(run/'snap/nsk1_network_14.png') as source:
        image = source.convert('RGB')
        # Exclude only the animated search-progress strip above row 10.
        if image.size != (84,48) or hashlib.sha256(image.crop((0,10,84,48)).tobytes()).hexdigest() != SEARCH_BODY:
            raise ValueError('manual-search Searching/Quit frame differs')
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binary','source','run'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        source, run = args.source.resolve(), args.run.resolve()
        check_phase(source, (source/'initial_eeprom.bin').read_bytes(), through=59, minimum_records=1)
        card = (source/'nvram/nsk1r4t/sim_card').read_bytes()
        initial = copy_persistent(run, source)
        command = run_machine(args.binary, run, Path(__file__).with_name('nsk1_network_search_input.lua'), 61, True)
        summary = check_run(run, initial, card)
        summary['command'] = command
        (run/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
        print(json.dumps(summary, indent=2))
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSK-1 manual network search failed: {error}\n')


if __name__ == '__main__':
    main()
