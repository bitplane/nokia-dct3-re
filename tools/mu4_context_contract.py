"""Read-only original R060 context/worker contracts; no full-ISR claim."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sys

if __package__:
    from .mu4_clock_contract import SOURCE_SHA256
    from .noki5510_a00_inventory import uploaded_word_image
else:
    from mu4_clock_contract import SOURCE_SHA256
    from noki5510_a00_inventory import uploaded_word_image


BASE = 0x23D73
MASK = 0xC004
WITNESS = (
    0xEEFF, 0x4A06, 0x4A07, 0xF6BE, 0xF6B9,
    0x4A08, 0x4A09, 0x4A0A, 0x4A0B, 0x4A0C, 0x4A0D,
    0xEA00, 0x577C, 0xF300, 0x0001, 0x4F7C, 0x487E,
    0xF030, MASK, 0xF050, MASK,
    0xFA44, 0x3D9A, 0x467F, 0xF495,
    0x4A10, 0x4A12, 0x4A13, 0x4A0E, 0x7131, 0x0012,
    0xF274, 0x3DBC, 0x770E, 0x0006,
    0x8A0E, 0x8A13, 0x8A12, 0x8A10,
    0x1036, 0xF7BE, 0x8A0D, 0x8A0C, 0x8A0B, 0x8006,
    0x8A0A, 0x8A09, 0x8A08, 0x8A07, 0x8A06, 0xFC00,
)
WORKER_BASE = 0x23DBC
WORKER_WITNESS = (0xFD00, 0x4A07, 0xFD00, 0xF7BB, 0x7192, 0x0010,
                  0x1092, 0xF844, 0x3DDB)
WORKER_TAIL_BASE = 0x23DDB
WORKER_TAIL_WITNESS = (0x8A08, 0xF030, 0x0800, 0xF495, 0xFE00, 0xFD45, 0xF6BB)
WORKER_COPY_BASE = 0x23DC5
WORKER_COPY_WITNESS = (0x6B82, 0x0002, 0x4B92, 0x7182, 0x0013, 0x8B93)


def decode_worker_copy(words):
    for offset, expected in enumerate(WORKER_COPY_WITNESS):
        address = WORKER_COPY_BASE + offset
        if words.get(address) != expected:
            raise ValueError(f'worker copy witness changed or absent at {address:06x}')
    return {
        'matched_word_range': ['023dc5', '023dca'],
        'matched_words': len(WORKER_COPY_WITNESS),
        'sequence': ['ADDM #2,*AR2', 'PSHD *AR2+', 'MVDK *AR2,AR3', 'POPD *AR3+'],
        'source_update': 'ADDM applies SXM/OVM arithmetic and C/OVA flags',
        'destination_pointer': 'second source word, loaded into AR3',
        'local_stack_effect': 'one word pushed and popped',
        'daram_order': 'PSHD samples the old source before concurrent ADDM Write',
        'scope': 'six pinned words; operand meanings, real inputs, later body and recorder completion unclassified',
    }


def restored_intm(saved_status):
    if not isinstance(saved_status, int) or not 0 <= saved_status <= 0xFFFF:
        raise ValueError('saved ST1 must be an unsigned 16-bit word')
    return 'masked' if saved_status & 0x0800 else 'unmasked'


def decode_worker_return(words):
    for offset, expected in enumerate(WORKER_TAIL_WITNESS):
        address = WORKER_TAIL_BASE + offset
        if words.get(address) != expected:
            raise ValueError(f'worker return witness changed or absent at {address:06x}')
    return {
        'matched_word_range': ['023ddb', '023de1'],
        'matched_words': len(WORKER_TAIL_WITNESS),
        'saved_status_pop': {'address': '023ddb', 'destination': 'AL'},
        'mask': '0800', 'restores': 'INTM only, not all ST1 fields',
        'return': {'address': '023ddf', 'opcode': 'fe00', 'operation': 'RETD',
                   'delay_words': ['fd45', 'f6bb'],
                   'delay_operations': ['XC 1,AEQ', 'RSBX INTM']},
        'clear_intm_when': '(saved ST1 & 0800) == 0',
        'status_coverage': {'tested': 65536,
                            **dict(Counter(restored_intm(value) for value in range(65536)))},
        'scope': 'seven pinned original words and exhaustive INTM predicate; '
                 'no full worker body or native recorder completion',
    }


def worker_route(control_word):
    if not isinstance(control_word, int) or not 0 <= control_word <= 0xFFFF:
        raise ValueError('worker control must be an unsigned 16-bit word')
    return 'skip_worker_body' if control_word else 'enter_worker_body'


def decode_worker_entry(words):
    for offset, expected in enumerate(WORKER_WITNESS):
        address = WORKER_BASE + offset
        if words.get(address) != expected:
            raise ValueError(f'worker entry witness changed or absent at {address:06x}')
    counts = Counter(worker_route(value) for value in range(0x10000))
    return {
        'matched_word_range': ['023dbc', '023dc4'],
        'matched_words': len(WORKER_WITNESS),
        'setup': {
            'xc': 'two unconditional XC 1 instructions, each protecting two words',
            'saved_register': 'ST1', 'status_update': 'SSBX INTM',
            'first_source': 'RAM[entry AR2]', 'first_destination': 'AR0',
            'second_source': 'RAM[(entry AR2 + 1) & ffff]',
            'second_destination': 'A', 'final_ar2': '(entry AR2 + 2) & ffff',
        },
        'branch': {
            'address': '023dc3', 'opcode': 'f844', 'condition': 'ANEQ',
            'delayed': False, 'target': '023ddb', 'fallthrough': '023dc5',
            'taken_when': 'second source word != 0',
            'documented_cycles': {'taken': 5, 'not_taken': 3},
            'control_coverage': {'tested': 65536, **dict(counts)},
        },
        'scope': 'nine pinned original words and exhaustive word predicate; '
                 'no source-writer census, branch pipeline execution, full worker or recorder completion',
    }


def route_for_status(status):
    if not isinstance(status, int) or not 0 <= status <= 0xFFFF:
        raise ValueError('status must be an unsigned 16-bit word')
    # LDM zero-extends; AND then XOR selects missing required bits.
    return 'skip_worker' if (status & MASK) ^ MASK else 'call_worker'


def decode_words(words):
    for offset, expected in enumerate(WITNESS):
        address = BASE + offset
        if words.get(address) != expected:
            raise ValueError(f'context witness changed or absent at {address:06x}')
    counts = Counter(route_for_status(value) for value in range(0x10000))
    return {
        'source_sha256': SOURCE_SHA256,
        'segment': 'aa22',
        'matched_word_range': ['023d73', '023da5'],
        'matched_words': len(WITNESS),
        'branch': {
            'address': '023d88', 'opcode': 'fa44', 'condition': 'ANEQ',
            'delayed': True, 'target': '023d9a',
            'documented_base_cycles': 3,
            'status_source': '007e', 'required_mask': 'c004',
            'taken_when': '(status & c004) != c004',
            'delay_slots': [
                {'address': '023d8a', 'words': ['467f'],
                 'operation': 'LD Smem,DP', 'source_offset': '7f',
                 'source_dp': 0, 'documented_base_cycles': 3},
                {'address': '023d8b', 'words': ['f495'], 'operation': 'NOP'},
            ],
            'status_coverage': {'tested': 65536, **dict(counts)},
        },
        'worker_call': {
            'address': '023d92', 'target': '023dbc', 'delayed': True,
            'delay_words': ['770e', '0006'], 'operation': 'STM #6,T',
            'continuation': '023d96', 'extra_saved_registers': ['AR0', 'AR2', 'AR3', 'T'],
        },
        'common_tail': {
            'entry': '023d9a', 'direct_load_offset': '36',
            'addressing_page': 'current DP; skip-worker path uses the delay-slot value; worker effects unclassified',
            'cpl_set': '023d9b', 'pop_bl': '023d9e',
            'following_compiler_store': '023d9f', 'store_offset': 6,
            'return_word': {'address': '023da5', 'opcode': 'fc00'},
        },
        'scope': '51 pinned original words and exhaustive status predicate; '
                 'no complete ISR, worker, writer census or physical pipeline validation',
    }


def contract(source):
    if hashlib.sha256(source).hexdigest() != SOURCE_SHA256:
        raise ValueError('expected original R060 InitData source')
    words = uploaded_word_image(bytes.fromhex(source.decode('ascii')), 'aa22')
    result = decode_words(words)
    result['worker_entry'] = decode_worker_entry(words)
    result['worker_return'] = decode_worker_return(words)
    result['worker_copy'] = decode_worker_copy(words)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    args = parser.parse_args()
    try:
        result = contract(args.source.read_bytes())
    except (OSError, ValueError) as error:
        print(f'MU4 context contract: {error}', file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
