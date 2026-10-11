#!/usr/bin/env python3
"""Pin bounded NSE-1 scan-result MCU contracts, not RF acquisition."""
import argparse
import hashlib
from pathlib import Path


IMAGE_SHA256 = '92778c5f51ded38ef395cc45aa5e40abbc51340b3eec9ff30827bfc481647d0c'
FRAGMENTS = {
    0x2531c0: bytes.fromhex('b5302854da402930da3e08cb061b0e1b00dc'),
    0x253218: bytes.fromhex('2454435c19044ddb192d782c430c702c'),
    0x25358c: bytes.fromhex('00107610'),
    0x2535dc: bytes.fromhex('00107418'),
    0x253398: bytes.fromhex('9805f7ffff11'),
    0x28d726: bytes.fromhex('b530f002f9d01c054c6f7a202801d10af001fc682802d006786042a8d00370651c28f7c5fea3bd30'),
    0x28facc: bytes.fromhex('4842780046f7'),
    0x28fbd8: bytes.fromhex('0010b543'),
    0x28d8ec: bytes.fromhex('0010b588'),
    0x28f00a: bytes.fromhex('4872780007000f0006000e0046f7'),
    0x28f1d4: bytes.fromhex('0010ffac'),
    0x253492: bytes.fromhex('b5f04641464a4653b40eb0834681f03bfdb32803d16a48e578012900d06646482804dd01200446812000'),
    0x253840: bytes.fromhex('001073d1'),
    0x211f6c: bytes.fromhex('b5f01e402800d01638012800d00b38012800d13a'),
    0x211f92: bytes.fromhex('2000f07dfd2a48dc210070c17341bdf0'),
    0x21230c: bytes.fromhex('001066a0'),
    0x28f9ec: bytes.fromhex('b5101c04206942e0dd152c00d013f7d6fa4d09c02800d00e200449712200230056cb429cdb0831011c5206120e1238012800d1f4e0002200486a78c14291d00270c2f7fdfe7abd10'),
    0x28fbd0: bytes.fromhex('0010b540'),
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
