"""Explicit laboratory FDN card-image extension; no handset state edits."""
import argparse
from pathlib import Path
import sys
if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH

HEADER=b'FDN\x01'
RECORD_LENGTH=32
RECORD_COUNT=10
EXTENDED_LENGTH=CURRENT_NVRAM_LENGTH+8+RECORD_LENGTH*RECORD_COUNT


def extend(card,adn_access_when_invalid=False):
    if len(card)!=CURRENT_NVRAM_LENGTH:raise ValueError('complete base card required')
    return card+HEADER+bytes((1,1,int(adn_access_when_invalid),0))+b'\xff'*(RECORD_LENGTH*RECORD_COUNT)


def expected_validity_card(before, phase):
    if phase not in range(4) or len(before) != EXTENDED_LENGTH or before[3570:3575] != b'FDN\x01\x01':
        raise ValueError('known phase and extended FDN card required')
    if before[3575] != (1 if phase in (0, 3) else 0):
        raise ValueError('initial FDN validity differs')
    after = bytearray(before)
    if phase in (0, 2):
        after[3575] = 0 if phase == 0 else 1
    return bytes(after)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path);parser.add_argument('output',type=Path)
    parser.add_argument('--adn-access-when-invalid',action='store_true')
    args=parser.parse_args()
    try:
        data=extend(args.source.read_bytes(),args.adn_access_when_invalid)
        with args.output.open('xb') as output:output.write(data)
    except (ValueError,OSError) as error:parser.exit(1,f'FDN profile failed: {error}\n')
    return 0


if __name__=='__main__':raise SystemExit(main())
