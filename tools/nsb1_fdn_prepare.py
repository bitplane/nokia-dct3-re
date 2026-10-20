"""Append the explicit laboratory FDN profile to qualified own NSB-1 inputs."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb1_rom4_compare import prepare_cold_run, INPUTS
from tools.nsb1_pin_roundtrip import check_card
from tools.sim_fdn_profile import extend


def prepare(source, output):
    before = (source / 'nvram/nsb1r4t/sim_card').read_bytes()
    check_card(before, 3)
    card = extend(before, True)
    prepare_cold_run(output, source, lambda data: check_card(data, 3), INPUTS)
    (output / 'nvram/nsb1r4t/sim_card').write_bytes(card)
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
        print(f'NSB-1 FDN preparation failed: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
