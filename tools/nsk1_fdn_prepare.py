"""Explicit FDN card input over qualified own NSK-1 persistent storage."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsk1_pin_roundtrip import check_run
from tools.nsk1_phonebook_roundtrip import copy_persistent
from tools.sim_fdn_profile import extend


def prepare(source, output):
    _, wrong = check_run(source / 'wrong')
    _, before = check_run(source / 'correct', True, wrong)
    card = extend(before, True)
    copy_persistent(output, source / 'correct')
    (output / 'nvram/nsk1r4t/sim_card').write_bytes(card)
    (output / 'initial_sim_card.bin').write_bytes(card)
    path = output / 'input_manifest.json'
    manifest = json.loads(path.read_text())
    manifest['card_fixture'] = {'kind': 'explicit FDN profile with invalid-ADN access',
                               'base_sha256': hashlib.sha256(before).hexdigest(),
                               'input_sha256': hashlib.sha256(card).hexdigest(),
                               'handset_storage_changed': False}
    path.write_text(json.dumps(manifest, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    try:
        prepare(args.source.resolve(), args.output.resolve())
    except (ValueError, OSError) as error:
        print(f'NSK-1 FDN preparation failed: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
