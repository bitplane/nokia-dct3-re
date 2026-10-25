"""Prepare own NSB-1 cold storage with an external Phase-2+ lab card."""
import argparse
import json
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_current_profile
from tools.nsb1_rom4_compare import INPUTS, prepare_cold_run


def profile_metadata(profile):
    if profile not in (0, 1, 3, 4, 7, 9):
        raise ValueError('unqualified external Toolkit profile')
    return {'profile': profile,
            'description': ({0:'Phase 2 control',1:'Phase 2+ with one DISPLAY TEXT command',
                             3:'Phase 2+ with DISPLAY TEXT, GET INKEY and GET INPUT',
                             4:'Phase 2+ with interactive commands and installed menu',
                             7:'Phase 2+ with interactive menu and SELECT ITEM follow-up',
                             9:'Phase 2+ with interactive menu and REFRESH follow-up'}[profile]),
            'handset_storage_changed': False, 'persistent_card_changed': False}


def prepare(source, target, profile=1):
    metadata = profile_metadata(profile)
    def original_card(card):
        if card != make_current_profile(True):
            raise ValueError('requires the unchanged PIN-enabled laboratory card')
    prepare_cold_run(target, source, original_card, INPUTS)
    path = target / 'cfg/nsb1r4t.cfg'
    tree = ET.parse(path)
    inputs = tree.find('./system/input')
    if inputs is None or inputs.find("port[@tag=':SATCFG']") is not None:
        raise ValueError('missing own inputs or pre-existing Toolkit override')
    ET.SubElement(inputs, 'port', tag=':SATCFG', type='CONFIG',
                  mask='15', defvalue='0', value=str(profile))
    tree.write(path, encoding='utf-8', xml_declaration=True)
    manifest_path = target / 'input_manifest.json'
    manifest = json.loads(manifest_path.read_text())
    manifest['external_toolkit_card'] = metadata
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('target', type=Path)
    parser.add_argument('--control', action='store_true')
    args = parser.parse_args()
    try:
        prepare(args.source.resolve(), args.target.resolve(), 0 if args.control else 1)
    except (OSError, ValueError, ET.ParseError) as error:
        parser.exit(1, f'NSB-1 Toolkit preparation FAIL: {error}\n')
    print('Prepared own cold storage; only external Toolkit input differs')


if __name__ == '__main__':
    main()
