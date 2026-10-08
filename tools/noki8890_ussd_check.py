"""Own NSB-6 physical USSD, reviewed frames and persisted location acceptance."""
import argparse
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools import radio_ussd_trace_check as protocol
from tools.noki8210_supplementary_check import verify_transaction
from tools.noki8890_registration_check import verify as verify_registration

KEYS = ('Keypad *', 'Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad #', 'Call / Send')
RESULT = 'a8da777883404e560788e40d1709459b12dd266ef6d0b29481ec8a36cab82495'
IDLE = '8187cbe68f4b7fe0a15cf10c16b742b3236f3240af3f79556c319d5ed337f2ee'


def verify(text, frames, storage):
    verify_transaction(text, frames, 'ussd', KEYS, protocol, RESULT, IDLE,
                       product='8890')
    verify_registration(text)
    if len(storage) < 1611 or storage[1604:1609] != bytes.fromhex('00f1100001') or storage[1610] != 0:
        raise ValueError('persisted EF_LOCI lacks updated laboratory location')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    args = parser.parse_args()
    try:
        run = args.run_directory
        verify((run / 'error.log').read_text(errors='replace'), run / 'snap',
               (run / 'nvram/nsb6hle/sim_card').read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f'8890 USSD FAIL: {error}\n')
    print('8890 physical USSD, reviewed result/idle and persisted registration PASS')


if __name__ == '__main__':
    main()
