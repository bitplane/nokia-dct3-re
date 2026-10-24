"""Declare a laboratory FDN card extension without changing handset storage."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_phonebook_check import prepare_reload
from tools.nsb3_pin_check import check_card
from tools.sim_fdn_profile import extend


def prepare(source, output):
    before = (source / 'nvram/nsb3r4t/sim_card').read_bytes()
    check_card(before, 3)
    card = extend(before, True)
    prepare_reload(source, output, lambda data: check_card(data, 3))
    (output / 'nvram/nsb3r4t/sim_card').write_bytes(card)
    (output / 'initial_sim_card.bin').write_bytes(card)
    path = output / 'cold_source.json'
    metadata = json.loads(path.read_text())
    metadata['external_card_fixture'] = {
        'kind': 'explicit FDN extension with invalid-ADN read access',
        'base_sha256': hashlib.sha256(before).hexdigest(),
        'input_sha256': hashlib.sha256(card).hexdigest(),
        'handset_storage_changed': False}
    path.write_text(json.dumps(metadata, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    try:
        prepare(args.source, args.output)
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 FDN input: FAIL: {exc}\n')
    print('PASS: explicit laboratory FDN card; own handset storage copied unchanged')


if __name__ == '__main__':
    main()
