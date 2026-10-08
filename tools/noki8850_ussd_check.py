"""Own NSM-2 physical USSD, registered-idle and persisted location acceptance."""
import argparse
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools import radio_ussd_trace_check as protocol
from tools.noki8210_supplementary_check import verify_transaction
from tools.radio_registration_trace_check import verify as verify_registration

KEYS = ('Keypad *', 'Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad #', 'Call / Send')
RESULT = 'eac7017b7ae7da13b4b33e5e47aac57495c8ca92554f812b52efb6443119efb1'
IDLE = 'd6d4b05af24a06c42a97e31c3134f7e497e149f4420f87f3d243613f06926300'


def verify(text, frames, storage):
    verify_transaction(text, frames, 'ussd', KEYS, protocol, RESULT, IDLE,
                       product='8850')
    verify_registration(text, 'nsm2')
    if len(storage) < 1611 or storage[1604:1609] != bytes.fromhex('00f1100001') or storage[1610] != 0:
        raise ValueError('persisted EF_LOCI lacks updated laboratory location')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    args = parser.parse_args()
    try:
        run = args.run_directory
        verify((run / 'error.log').read_text(errors='replace'), run / 'snap',
               (run / 'nvram/nsm2hle/sim_card').read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f'8850 USSD FAIL: {error}\n')
    print('8850 physical USSD, reviewed result/idle and persisted registration PASS')


if __name__ == '__main__':
    main()
