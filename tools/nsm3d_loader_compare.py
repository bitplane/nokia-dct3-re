"""Compare own NSM-3D upload branch tables without inferring mask contents."""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct

from tools.nsm3d_catalogue import catalogue, covering


def compare(old_image, new_image):
    old_entries = catalogue(old_image, '8250')
    new_entries = catalogue(new_image, '8250-v600')

    def payload(image, entry):
        offset = entry['payload_offset']
        return image[offset:offset + entry['words'] * 2]

    old = old_entries[0]
    new = new_entries[0]
    if old['header'] != new['header'] or old['words'] != 638:
        raise ValueError('first-loader descriptor differs from paired contract')
    old_body, new_body = payload(old_image, old), payload(new_image, new)
    old_words = struct.unpack('>638H', old_body)
    new_words = struct.unpack('>638H', new_body)
    if old_body[422 * 2:] != new_body[422 * 2:]:
        raise ValueError('difference extends beyond recovered 422-word branch table')
    changes = []
    for index, (before, after) in enumerate(zip(old_words, new_words)):
        if before == after:
            continue
        if index % 2 != 1 or old_words[index - 1] != 0xf073 or new_words[index - 1] != 0xf073:
            raise ValueError('changed word is not an absolute-B operand')
        changes.append({
            'word_index': index, 'table_program_address': 0x590 + index,
            'old_target': before, 'new_target': after, 'delta_words': after - before,
            'old_declared_coverage': covering(old_entries, before),
            'new_declared_coverage': covering(new_entries, after),
        })
    counts = Counter(change['delta_words'] for change in changes)
    if counts != Counter({-11: 19, 10: 3}):
        raise ValueError('paired branch-operand delta census differs')
    return {
        'products': ['8250-v502', '8250-v600'],
        'branch_table_words': 422, 'compared_upload_words': 638,
        'changed_operands': changes,
        'unchanged_executable_tail_words': 126,
        'executable_tail_sha1': hashlib.sha1(old_body[512 * 2:]).hexdigest(),
        'scope': 'Pinned own uploads and declared destination coverage. Overlaps '
                 'do not identify program ownership; relocation, native v6.00 boot '
                 'and fitted mask contents are not established.',
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('v502_flash', type=Path)
    parser.add_argument('v600_flash', type=Path)
    args = parser.parse_args()
    try:
        result = compare(args.v502_flash.read_bytes(), args.v600_flash.read_bytes())
    except (OSError, ValueError, struct.error) as error:
        parser.exit(1, f'NSM-3D loader comparison failed: {error}\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
