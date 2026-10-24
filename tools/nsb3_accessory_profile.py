"""Three own NSB-3 raw-input contrasts, not measured board calibration."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_idle_profile import contract, check_card, frame, inputs
from tools.nsb3_phonebook_check import prepare_reload
from tools.nsb3_pin_check import check_trace as check_authentication
from tools.nsb3_static_check import accessory_contract

PROFILES = {
    'low': (0, 0, 0x10, '192aec2de3cd98478979cc093ad09b2f7ad825bcfc29ac2162f64ba7f37ea462'),
    'analog-high': (1023, 0, 0x20, 'c397f9fefdae7db7b8de490798591110473d92585bf1c13c61d10fa2bf0d1771'),
    'both-high': (1023, 2, 0x0f, '9b62da95e67601d1d4124ebeef9c1f3a956cd96506a7de0d097f2fd79e806dc2'),
}
PORTS = (':RESEARCH_ADC0', ':RESEARCH_EXT_STATUS')
MENU_FRAME = 'b2bde13413750ea3a6442a8a4d6c90362d4e8b16cde72bbf2f981a00089cecd0'


def metadata(profile):
    raw, external, _, _ = PROFILES[profile]
    return {'profile': profile, 'raw_adc0': raw, 'raw_external_status': external,
            'handset_storage_changed': False, 'physical_open_voltage_or_pin_proved': False}


def prepare(source, target, profile):
    image = (source / 'nvram/nsb3r4t/flash').read_bytes()
    contract(image)
    accessory_contract(image[:0xd6d00])
    prepare_reload(source, target, check_card)
    path = target / 'cfg/nsb3r4t.cfg'
    tree = ET.parse(path)
    input_node = tree.find('./system/input')
    if input_node is None or any(input_node.find(f"port[@tag='{tag}']") is not None for tag in PORTS):
        raise ValueError('requires own input configuration without accessory overrides')
    for tag, mask, value in zip(PORTS, (1023, 7), PROFILES[profile][:2]):
        ET.SubElement(input_node, 'port', tag=tag, type='CONFIG', mask=str(mask), defvalue='0', value=str(value))
    tree.write(path, encoding='utf-8', xml_declaration=True)
    (target / 'accessory_input.json').write_text(json.dumps(metadata(profile), indent=2) + '\n')


def check_trace(text, profile):
    raw, external, state, _ = PROFILES[profile]
    check_authentication(text, True)
    if re.findall(r'ccont_adc0_config: raw=([0-9a-f]+) external_input=1', text) != [f'{raw:03x}']:
        raise ValueError('raw analog configuration differs')
    if re.findall(r'mad2_external_config: raw=([0-9a-f]+) external_input=1', text) != [f'{external:x}']:
        raise ValueError('raw external-status configuration differs')
    rows = re.findall(r'nsb3_accessory_sample: second=(\d+) state=([0-9a-f]{2}) sample=([0-9a-f]{4}) external=([0-9a-f]{2}) t=[0-9.]+ observer_writes=0', text)
    if rows != [(str(t), f'{state:02x}', f'{raw:04x}', f'{external:02x}') for t in (5, 10, 24, 36, 40)]:
        raise ValueError('own accessory state/sample/input observations differ')
    writes = re.findall(r'nsb3_accessory_state: value=([0-9a-f]{2}) pc=([0-9a-f]{8}) t=[0-9.]+ observer_writes=0', text)
    expected = [('00', '0020010e'), ('0f', '002ad1aa')]
    if state != 0x0f:
        expected.append((f'{state:02x}', '00273d96' if state == 0x10 else '00273df6'))
    if writes != expected or 'nsb3_board_reset_request:' in text:
        raise ValueError('accessory transition ownership or native reset differs')


def configuration(path, source, profile):
    rows = inputs(path)
    original = inputs(source)
    other = [row for row in rows if not (row[0] == 'port' and dict(row[1]).get('tag') in PORTS)]
    if other != original:
        raise ValueError('unrelated input configuration changed')
    root = ET.parse(path).getroot()
    for tag, mask, value in zip(PORTS, (1023, 7), PROFILES[profile][:2]):
        ports = root.findall(f"./system/input/port[@tag='{tag}']")
        # MAME omits an explicit zero override after running because it is default.
        if len(ports) > 1 or (not ports and value):
            raise ValueError('missing or duplicate raw-input configuration')
        if ports and ports[0].attrib != dict(tag=tag, type='CONFIG', mask=str(mask), defvalue='0', value=str(value)):
            raise ValueError('raw-input configuration differs')


def check(source, run, profile):
    link = json.loads((run / 'cold_source.json').read_text())
    if Path(link['source']).resolve() != source.resolve() or link['copied_guest_ram_or_save_state'] is not False:
        raise ValueError('cold own-storage predecessor differs')
    if json.loads((run / 'accessory_input.json').read_text()) != metadata(profile):
        raise ValueError('raw-input provenance differs')
    configuration(run / 'cfg/nsb3r4t.cfg', source / 'cfg/nsb3r4t.cfg', profile)
    image = (source / 'nvram/nsb3r4t/flash').read_bytes()
    contract(image)
    accessory_contract(image[:0xd6d00])
    for name in ('flash', 'eeprom', 'sim_card'):
        before = (source / 'nvram/nsb3r4t' / name).read_bytes()
        after = (run / 'nvram/nsb3r4t' / name).read_bytes()
        if before != after or link['storage_sha256'][name] != hashlib.sha256(before).hexdigest():
            raise ValueError('complete cold storage changed')
        if name == 'sim_card':
            check_card(after)
            if (run / 'initial_sim_card.bin').read_bytes() != after:
                raise ValueError('initial complete card differs')
    check_trace((run / 'error.log').read_text(), profile)
    if frame(run / 'snap/nsb3_accessory_36.png') != PROFILES[profile][3]:
        raise ValueError('reviewed profile-dependent idle pixels differ')
    if frame(run / 'snap/nsb3_accessory_40.png') != MENU_FRAME:
        raise ValueError('shared physical Messages-menu pixels differ')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    prep = sub.add_parser('prepare')
    prep.add_argument('source', type=Path)
    prep.add_argument('run', type=Path)
    prep.add_argument('profile', choices=PROFILES)
    verify = sub.add_parser('check')
    verify.add_argument('source', type=Path)
    for name in ('low', 'analog_high', 'both_high'):
        verify.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        if args.command == 'prepare':
            prepare(args.source, args.run, args.profile)
        else:
            for run, profile in zip((args.low, args.analog_high, args.both_high), PROFILES):
                check(args.source, run, profile)
    except (OSError, ValueError, KeyError, ET.ParseError) as exc:
        parser.exit(1, f'NSB-3 accessory inputs: FAIL: {exc}\n')
    print('PASS: three raw-input branches, physical authentication/menu and unchanged storage; electrical ownership unproved'
          if args.command == 'check' else 'Prepared only external raw inputs; no handset/card edits')


if __name__ == '__main__':
    main()
