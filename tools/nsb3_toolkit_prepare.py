"""Prepare own cold storage with an explicitly configured external Toolkit card."""
import argparse
import json
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_phonebook_check import prepare_reload
from tools.make_sim_card_profile import make_current_profile


def profile_metadata(profile):
    descriptions = {1: 'Phase 2+ with one DISPLAY TEXT command',
                    3: 'Phase 2+ with DISPLAY TEXT, GET INKEY and GET INPUT',
                    4: 'Phase 2+ with interactive commands and SET UP MENU',
                    7: 'Phase 2+ menu presenting SELECT ITEM',
                    14: 'Phase 2+ menu starting a toolkit timer'}
    if profile not in descriptions:
        raise ValueError('unsupported external card fixture')
    return dict(profile=profile, description=descriptions[profile],
                handset_storage_changed=False, persistent_card_changed=False)


def prepare(source, target, profile=1):
    metadata_profile = profile_metadata(profile)
    def check_card(card):
        if card != make_current_profile(True):
            raise ValueError('requires qualified original PIN-enabled laboratory card')
    prepare_reload(source, target, check_card)
    path = target / 'cfg/nsb3r4t.cfg'
    tree = ET.parse(path)
    inputs = tree.find('./system/input')
    if inputs is None:
        raise ValueError('own input configuration missing')
    if inputs.find("port[@tag=':SATCFG']") is not None:
        raise ValueError('source already overrides Toolkit profile')
    ET.SubElement(inputs, 'port', tag=':SATCFG', type='CONFIG', mask='15', defvalue='0', value=str(profile))
    tree.write(path, encoding='utf-8', xml_declaration=True)
    metadata = json.loads((target / 'cold_source.json').read_text())
    metadata['external_toolkit_card'] = metadata_profile
    (target / 'cold_source.json').write_text(json.dumps(metadata, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('target', type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--interactive', action='store_true')
    mode.add_argument('--menu', action='store_true')
    mode.add_argument('--select-item', action='store_true')
    mode.add_argument('--timer', action='store_true')
    args = parser.parse_args()
    try:
        profile = 14 if args.timer else 7 if args.select_item else 4 if args.menu else 3 if args.interactive else 1
        prepare(args.source, args.target, profile)
    except (OSError, ValueError, KeyError, ET.ParseError) as exc:
        parser.exit(1, f'NSB-3 Toolkit preparation: FAIL: {exc}\n')
    print('Prepared explicit external Phase-2+ card; no handset/card storage edits')


if __name__ == '__main__':
    main()
