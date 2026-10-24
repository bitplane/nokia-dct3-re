"""Source-pinned census of NPE-3 installed DSP word pairs, not a CFG decode."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


BLOCKS = {
    'bootstrap': (0x1eac4, 0x0590, 422),
    'loader2': (0x24b7c, 0x0a00, 629),
    'fragment': (0x24a50, 0xff80, 104),
}


def assess(image, captures=None):
    if hashlib.sha1(image).hexdigest() != '3d9ea319503e78ec69b60d72cda23e461e118ea9':
        raise ValueError('not the acquired NPE-3 image')
    blocks = {}
    for name, (offset, start, count) in BLOCKS.items():
        payload = image[offset:offset + count * 2]
        if captures is not None and captures[name] != payload:
            raise ValueError(f'{name} capture differs from own source')
        blocks[name] = {'source_offset': offset, 'start': start, 'words': count,
                        'sha1': hashlib.sha1(payload).hexdigest()}
    words = struct.unpack_from('>422H', image, BLOCKS['bootstrap'][0])
    branches, returns, other = [], [], []
    for index in range(0, len(words), 2):
        address, head, operand = 0x590 + index, words[index], words[index + 1]
        if head == 0xfc00 and operand == 0xf495:
            returns.append(address)
        elif head == 0xf073:
            owner = next((name for name, (_, start, count) in BLOCKS.items()
                          if start <= operand < start + count), None)
            branches.append({'address': address, 'target': operand, 'owner': owner})
        else:
            other.append({'address': address, 'words': [head, operand]})
    return {'blocks': blocks, 'pairs_examined': len(words) // 2,
            'return_nop_pairs': len(returns), 'branch_pairs': branches,
            'other_pairs': other,
            'unowned_branch_pairs': sum(x['owner'] is None for x in branches),
            'scope': 'Aligned word-pair census; not code/data classification, CFG, '
                     'execution coverage or proof of absent codec setup.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--captures', type=Path,
                        help='Directory of native npe3_{bootstrap,loader2,fragment}.bin captures')
    args = parser.parse_args()
    try:
        captures = None if args.captures is None else {
            name: (args.captures / f'npe3_{name}.bin').read_bytes() for name in BLOCKS}
        print(json.dumps(assess(args.image.read_bytes(), captures), indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, f'NPE-3 installed program FAIL: {error}\n')


if __name__ == '__main__':
    main()
