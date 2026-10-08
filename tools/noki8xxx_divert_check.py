"""Own-product physical service-21 interrogation on the 8850 and 8890."""
import argparse
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools import radio_call_divert_trace_check as protocol
from tools.noki8210_supplementary_check import verify_transaction
from tools.radio_registration_trace_check import verify as verify_8850_registration
from tools.noki8890_registration_check import verify as verify_8890_registration

PROFILES = {
    '8850': ('nsm2hle',
             '7213425cd2f688b40a09db063014725fee63d3babe121a1cb499d50e4b1dcb0b',
             '08737c08772c99df2b41fc8560587820c2c209f5f74c12893a657bd6f623e555'),
    '8890': ('nsb6hle',
             '74da883871955d18ac5a7b048f9800f84cc818b0043154eaac5a01ef9689de44',
             '03cd1654572ac2cbe001a2fda57cdc3ead7b8d578d81efcaffe2af3350e0d6bc'),
}
KEYS = ('Keypad *', 'Keypad #', 'Keypad 2', 'Keypad 1', 'Keypad #', 'Call / Send')


def verify(text, frames, storage, product):
    if product not in PROFILES:
        raise ValueError('unsupported own-product divert profile')
    _, result, idle = PROFILES[product]
    verify_transaction(text, frames, 'divert', KEYS, protocol, result, idle,
                       product=product)
    if product == '8850':
        verify_8850_registration(text, 'nsm2')
    else:
        verify_8890_registration(text)
    if len(storage) < 1611 or storage[1604:1609] != bytes.fromhex('00f1100001') or storage[1610] != 0:
        raise ValueError('persisted EF_LOCI lacks updated laboratory location')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('product', choices=PROFILES)
    parser.add_argument('run_directory', type=Path)
    args = parser.parse_args()
    try:
        run = args.run_directory
        machine = PROFILES[args.product][0]
        verify((run / 'error.log').read_text(errors='replace'), run / 'snap',
               (run / f'nvram/{machine}/sim_card').read_bytes(), args.product)
    except (OSError, ValueError) as error:
        parser.exit(1, f'{args.product} divert FAIL: {error}\n')
    print(f'{args.product} physical inactive call-divert interrogation and registered idle PASS')


if __name__ == '__main__':
    main()
