#!/usr/bin/env python3
"""Pin bounded NSE-1 scan-result MCU contracts, not RF acquisition."""
import argparse
import hashlib
from pathlib import Path


IMAGE_SHA256 = '92778c5f51ded38ef395cc45aa5e40abbc51340b3eec9ff30827bfc481647d0c'
FRAGMENTS = {
    0x210f94: bytes.fromhex('0021153400211516002114fc002114a20021128200210fbc00210fbc00211424002112820021127c'),
    0x211534: bytes.fromhex('4640f7fffcb91e402800d00638022800d12d2001f7fefba4e029493a2010780a43107008f7fffc78e021'),
    0x210eac: bytes.fromhex('b5101c0479a02800d00d'),
    0x210eca: bytes.fromhex('200306000e00bd10'),
    0x210f0a: bytes.fromhex('2001bd10'),
    0x211638: bytes.fromhex('001084bf'),
    0x2114fc: bytes.fromhex('4c69892030018120f06cff5689214281dd497ee00940d20ae045'),
    0x2116a4: bytes.fromhex('001084a4'),
    0x20fbb8: bytes.fromhex('b5704c3320017020201102801c21f059fbe3200221002200f002f9cc'),
    0x20fc20: bytes.fromhex('f7ffff652065f044fa2520652101f059f953205af044fa1e205a2101f059f94c201df044fa176be8f045fd1663ee6b28f045fd12632e6a28f045fd0e622e'),
    0x27e3b4: bytes.fromhex('b510200ff7d6f9e248118804200ff7d6fa411c20bd10'),
    0x27e404: bytes.fromhex('001098c4'),
    0x26bd98: bytes.fromhex('48a046418008200af7e8ff70'),
    0x26c01c: bytes.fromhex('0000138e'),
}


def check_fragments(image):
    for address, expected in FRAGMENTS.items():
        offset = address - 0x200000
        if image[offset:offset + len(expected)] != expected:
            raise ValueError(f'scan continuation contract changed at {address:08x}')
    return len(FRAGMENTS)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    args = parser.parse_args()
    try:
        image = args.image.read_bytes()
        if hashlib.sha256(image).hexdigest() != IMAGE_SHA256:
            raise ValueError('requires acquired NSE-1 v5.30 MCU image')
        count = check_fragments(image)
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSE-1 scan continuation FAIL: {error}\n')
    print(f'NSE-1 scan continuation PASS fragments={count} writer_closure=0 acquisition_claim=0')


if __name__ == '__main__':
    main()
