"""Validate own NSM-3 laboratory registration and persistent EF_LOCI."""

import argparse
from pathlib import Path
import re


CHECKPOINTS = (
    r'TX packet type=56 payload=160 .*data=0004',
    r'TX packet type=02 .*radio_phase=candidate_channel_change data=040000000000005050000004',
    r'TX packet type=0c .*radio_phase=random_access',
    r'RX enqueue type=89 payload=8 .*data=0100000000000000',
    r'TX packet type=1b .*data=0080013f490508(?:70|72)[0-9a-f]{10}33080910101032547698',
    r'LAPDm Location Updating Accept acknowledged nr=1',
    r'TX packet type=1b .*data=0080032101',
    r'LAPDm Channel Release acknowledged nr=2',
    r'TX packet type=1b .*data=0080034101',
    r'update-binary fid=6f7e offset=4 length=5',
    r'TX packet type=02 .*radio_phase=release_channel_change data=040000000000001a600000040000000f00000000',
    r'RX enqueue type=89 payload=8 .*data=0000000000000000',
    r'RX enqueue type=80 payload=34 .*data=60[0-9a-f]{18}1506210001f0',
)


def verify(text, storage):
    cursor = 0
    for pattern in CHECKPOINTS:
        match = re.search(pattern, text[cursor:])
        if not match:
            raise ValueError(f'missing ordered registration evidence: {pattern}')
        cursor += match.end()
    if len(storage) < 1611 or storage[1604:1609] != bytes.fromhex('00f1100001'):
        raise ValueError('persisted EF_LOCI lacks laboratory LAI')
    if storage[1610] != 0:
        raise ValueError('persisted EF_LOCI is not location-updated')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('sim_nvram', type=Path)
    args = parser.parse_args()
    try:
        with args.log.open(errors='replace') as stream:
            text = ''.join(line for line in stream if 'TX packet' in line or
                           'RX enqueue' in line or 'acknowledged' in line or
                           'update-binary' in line)
        verify(text, args.sim_nvram.read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 registration FAIL: {error}\n')
    print('8210 own registration/release/paging and persisted EF_LOCI PASS; inspect idle capture separately')


if __name__ == '__main__':
    main()
