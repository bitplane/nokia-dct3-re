"""Inventory NPM-5 logical NV requests without assigning guessed field meanings."""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki5510_nv_check import verify_erased_frontier
from tools.noki5510_package_check import FLASH_SHA256, thumb_reference_census


def records(text, marker):
    result = []
    for line in text.splitlines():
        if marker + ':' not in line:
            continue
        fields = {}
        for token in line.split(marker + ':', 1)[1].split():
            key, separator, value = token.partition('=')
            if not separator or key in fields:
                raise ValueError('malformed ' + marker)
            fields[key] = value
        if fields.get('observer_writes') != '0':
            raise ValueError('NV observation scope changed')
        result.append(fields)
    return result


def source_contract(firmware):
    if len(firmware) != 0x350000 or hashlib.sha256(firmware).hexdigest() != FLASH_SHA256:
        raise ValueError('expected pinned own NPM-5 v3.53 PPM C firmware')
    census = thumb_reference_census(firmware, (0x2fd578, 0x3ac638))
    if census['direct_calls'][0x2fd578] != [0x3ac64c]:
        raise ValueError('reader/wrapper encoded-reference contract changed')
    return {'reader': '002fd578', 'wrapper': '003ac638',
            'arguments': {'r0': 'logical byte offset', 'r1': 'destination', 'r2': 'byte count'},
            'bounds': 'observed nonwrapping requests: nonzero count and offset+count <= 3800',
            'wrapper_frame': 'push r4,r5,r6,lr; mutex 5; reader call; mutex release; pop',
            'owner_context': 'saved wrapper LR at reader-entry SP+12; not an instruction-retirement claim',
            'firmware_sha256': FLASH_SHA256,
            'reader_code_sha256': hashlib.sha256(firmware[0xfd578:0xfd5b4]).hexdigest(),
            'wrapper_code_sha256': hashlib.sha256(firmware[0x1ac638:0x1ac65c]).hexdigest(),
            'encoded_reference_census': census}


def check_reads(text, cache, contract):
    scopes, events, ends, stops = (records(text, marker) for marker in
        ('5510_nv_reads_scope', '5510_nv_read', '5510_nv_reads_terminal', '5510_nv_reads_stop'))
    if (any(len(items) != 1 for items in (scopes, ends, stops))
            or scopes[0] != {'reader': '002fd578', 'wrapper': '003ac638', 'cap': '1024',
                'identity': 'debugger_entry', 'operands': 'context_only', 'observer_writes': '0'}
            or ends[0].get('cap') != '1024'
            or not 0 < len(events) == int(ends[0]['events']) < 1024
            or not 8 <= float(stops[0]['t']) < 9):
        raise ValueError('missing, capped or unfinished logical NV inventory')
    negative = verify_erased_frontier(text, cache)
    known = set(contract['encoded_reference_census']['direct_calls'][0x3ac638])
    groups, owners, covered, indirect = Counter(), defaultdict(set), set(), Counter()
    decoded = []
    for ordinal, row in enumerate(events, 1):
        offset, count, destination, link, owner = (int(row[key], 16)
            for key in ('offset', 'bytes', 'destination', 'link', 'owner'))
        if (int(row['ordinal']) != ordinal or row.get('identity') != 'debugger_entry'
                or row.get('operands') != 'context_only' or link != 0x3ac651
                or not 0 <= offset < 0x3800 or not 0 < count <= 0x3800-offset
                or not 0x100000 <= destination < destination+count <= 0x180000
                or not owner & 1 or not 0x200000 <= (owner & ~1) < 0x550000):
            raise ValueError('NV request ABI, extent or wrapper ownership changed')
        call = (owner & ~1)-4
        groups[offset, count] += 1
        owners[offset, count].add(call)
        covered.update(range(offset, offset+count))
        if call not in known:
            indirect[call] += 1
        decoded.append((offset, count, owner))
    required = ((0,0x120,0x2a05a5),(0x120,0x136,0x24c671),
                (0x154,2,0x3368b3),(0x256,2,0x24c82f),(0x170,2,0x24c83d))
    previous = -1
    for request in required:
        indices = [index for index, row in enumerate(decoded) if row == request and index > previous]
        if not indices:
            raise ValueError('missing ordered own checksum-block request: ' + repr(request))
        previous = indices[0]
    observed = {call for calls in owners.values() for call in calls}
    return {'requests': len(events), 'distinct_ranges': len(groups), 'unique_requested_bytes': len(covered),
            'cache_bytes': 0x3800, 'erased_selftest_control': negative,
            'observed_caller_sites': [f'{call:08x}' for call in sorted(observed)],
            'unobserved_direct_call_candidates': [f'{call:08x}' for call in sorted(known-observed)],
            'ranges': [{'offset': f'{offset:04x}', 'bytes': count, 'requests': copies,
                        'caller_candidates': [f'{call:08x}' for call in sorted(owners[offset, count])]}
                       for (offset,count),copies in sorted(groups.items())],
            'owners_outside_direct_bl_census': {f'{call:08x}': copies for call,copies in sorted(indirect.items())},
            'source_contract': contract,
            'scope': 'Bounded debugger entry requests after mutex acquisition and encoded-reference census; '
                     'not returned bytes, memory-copy retirement, all-reader coverage, field semantics or valid provisioning'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--firmware', type=Path, required=True)
    args = parser.parse_args()
    try:
        result = check_reads((args.directory/'error.log').read_text(),
                             (args.directory/'npm5_nv_cache.bin').read_bytes(),
                             source_contract(args.firmware.read_bytes()))
        print(json.dumps(result, indent=2))
    except (KeyError, ValueError, OSError) as error:
        parser.exit(1, f'NPM-5 NV read inventory FAIL: {error}\n')


if __name__ == '__main__':
    main()
