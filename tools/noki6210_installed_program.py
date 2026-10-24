"""Source-pinned census of NPE-3 installed DSP word pairs, not a CFG decode."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import re


BLOCKS = {
    'bootstrap': (0x1eac4, 0x0590, 422),
    'loader2': (0x24b7c, 0x0a00, 629),
    'fragment': (0x24a50, 0xff80, 104),
}


def catalogue(image, offset=0x315994):
    """Decode the bounded pointer candidate; do not infer a runtime selector ABI."""
    pointers = struct.unpack_from('>28I', image, offset)
    records = []
    for index, pointer in enumerate(pointers):
        source = pointer - 0x200000
        if source < 0 or source + 12 > len(image):
            raise ValueError(f'catalogue pointer outside source at {index:02x}')
        header = struct.unpack_from('>6H', image, source)
        end = source + 12 + header[2] * 2
        if end > len(image) or header[5] != 0:
            raise ValueError(f'catalogue payload/header invalid at {index:02x}')
        if index + 1 < len(pointers) and end > pointers[index + 1] - 0x200000:
            raise ValueError(f'catalogue payload overlaps next descriptor at {index:02x}')
        payload = image[source + 12:end]
        records.append({'index': index, 'descriptor': pointer,
                        'header': list(header), 'source_offset': source + 12,
                        'words': header[2], 'sha1': hashlib.sha1(payload).hexdigest()})
    return records


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
    return {'blocks': blocks, 'catalogue_candidate_offset': 0x315994,
            'catalogue_candidate': catalogue(image),
            'pairs_examined': len(words) // 2,
            'return_nop_pairs': len(returns), 'branch_pairs': branches,
            'other_pairs': other,
            'unowned_branch_pairs': sum(x['owner'] is None for x in branches),
            'scope': 'Aligned word-pair census; not code/data classification, CFG, '
                     'execution coverage or proof of absent codec setup.'}


def verify_catalogue_reads(image, text):
    records = catalogue(image)
    pattern = (r'npe3_catalogue_(flash|ram): address=([0-9a-f]{8}) '
               r'data=([0-9a-f]{8}) mask=([0-9a-f]{8}) pc=([0-9a-f]{8}) '
               r'r0=([0-9a-f]{8}) r1=([0-9a-f]{8}) r2=([0-9a-f]{8}) t=([0-9.]+)')
    rows = [match.groups() for match in re.finditer(pattern, text)]
    copies = [row for row in rows if row[0] == 'flash' and row[4] == '00200194']
    expected = [(0x515994 + 4 * i, record['descriptor'], 0xffffffff,
                 0x515994 + 4 * i, 0x170004 + 4 * i)
                for i, record in enumerate(records)]
    actual = [tuple(int(row[i], 16) for i in (1, 2, 3, 5, 6)) for row in copies]
    if actual != expected:
        raise ValueError('catalogue startup copy is incomplete or differs from own source')
    feeder = [row for row in rows if row[0] == 'flash' and row[4] != '00200194']
    if [tuple(int(row[i], 16) for i in (1, 2, 3, 4)) for row in feeder] != [
            (address, 0x220000, 0xffff0000, 0x426c5c)
            for address in (0x5159a0, 0x5159c0, 0x5159e0, 0x515a00)]:
        raise ValueError('catalogue strided verifier observations missing or changed')
    selections = [row for row in rows if row[0] == 'ram']
    sequence = [(0x1b, 0x426be8), (0, 0x426e1a), (0x14, 0x427396),
                (1, 0x427396), (0x15, 0x420356)]
    expected = [(0x170004 + 4 * index, records[index]['descriptor'], 0xffffffff, pc)
                for index, pc in sequence]
    actual = [tuple(int(row[i], 16) for i in (1, 2, 3, 4)) for row in selections]
    if actual != expected:
        raise ValueError('catalogue selection sequence differs')
    for name, count in (('flash', 32), ('ram', 5)):
        if re.findall(rf'npe3_catalogue_{name}: total=(\d+)', text) != [str(count)]:
            raise ValueError('catalogue observation totals missing or changed')
    if '[LUA ERROR]' in text:
        raise ValueError('catalogue observer failed')
    return {'copied_pointers': len(copies), 'ram_selections': [x[0] for x in sequence],
            'scope': 'Bounded fixture MCU reads; not successful DSP execution or unobserved selections.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--captures', type=Path,
                        help='Directory of native npe3_{bootstrap,loader2,fragment}.bin captures')
    parser.add_argument('--catalogue-log', type=Path,
                        help='Qualified passive catalogue observation (native boot or physical-call fixture)')
    args = parser.parse_args()
    try:
        captures = None if args.captures is None else {
            name: (args.captures / f'npe3_{name}.bin').read_bytes() for name in BLOCKS}
        image = args.image.read_bytes()
        result = assess(image, captures)
        if args.catalogue_log:
            result['catalogue_observation'] = verify_catalogue_reads(
                image, args.catalogue_log.read_text())
        print(json.dumps(result, indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, f'NPE-3 installed program FAIL: {error}\n')


if __name__ == '__main__':
    main()
