"""Own NSK-3 cold storage with an explicit external Toolkit card input."""
import argparse
import json
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_current_profile
from tools.nsk3_cold_input import prepare as prepare_cold
from tools.nsk3_diagnostic_display_check import check_bank


def profile_metadata(profile):
    if profile not in (0, 1):
        raise ValueError('only Phase-2 control and DISPLAY TEXT inputs are allowed')
    return dict(profile=profile, handset_storage_changed=False,
                persistent_card_changed=False)


def prepare(source, seed, run, profile=1):
    metadata = profile_metadata(profile)
    storage = seed / 'nvram/nsk3r4t'
    check_bank((storage / 'eeprom').read_bytes(), True)
    if (storage / 'sim_card').read_bytes() != make_current_profile(True):
        raise ValueError('requires original complete PIN-enabled laboratory card')
    path = seed / 'cfg/nsk3r4t.cfg'
    tree = ET.parse(path)
    inputs = tree.find('./system/input')
    if inputs is None or inputs.find("port[@tag=':SATCFG']") is not None:
        raise ValueError('missing own inputs or pre-existing external-card override')
    prepare_cold(source, seed, run)
    ET.SubElement(inputs, 'port', tag=':SATCFG', type='CONFIG',
                  mask='15', defvalue='0', value=str(profile))
    tree.write(run / 'cfg/nsk3r4t.cfg', encoding='utf-8', xml_declaration=True)
    (run / 'toolkit_input.json').write_text(json.dumps(metadata, indent=2)+'\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'seed', 'run'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--control', action='store_true')
    args = parser.parse_args()
    try:
        prepare(args.source.resolve(), args.seed.resolve(), args.run.resolve(),
                0 if args.control else 1)
    except (OSError, ValueError, ET.ParseError) as error:
        parser.exit(1, f'NSK-3 Toolkit preparation FAIL: {error}\n')
    print('Prepared own cold storage; only declared external card input differs')
