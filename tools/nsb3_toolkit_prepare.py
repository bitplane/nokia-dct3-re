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


def prepare(source, target):
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
    ET.SubElement(inputs, 'port', tag=':SATCFG', type='CONFIG', mask='15', defvalue='0', value='1')
    tree.write(path, encoding='utf-8', xml_declaration=True)
    metadata = json.loads((target / 'cold_source.json').read_text())
    metadata['external_toolkit_card'] = dict(profile=1, description='Phase 2+ with one DISPLAY TEXT command',
                                            handset_storage_changed=False, persistent_card_changed=False)
    (target / 'cold_source.json').write_text(json.dumps(metadata, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('target', type=Path)
    args = parser.parse_args()
    try:
        prepare(args.source, args.target)
    except (OSError, ValueError, KeyError, ET.ParseError) as exc:
        parser.exit(1, f'NSB-3 Toolkit preparation: FAIL: {exc}\n')
    print('Prepared explicit external Phase-2+ card; no handset/card storage edits')


if __name__ == '__main__':
    main()
