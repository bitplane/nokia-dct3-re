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
WORKER_LOGIC_BASE = 0x23DD5
WORKER_LOGIC_WITNESS = (0xF000, 0x0004, 0x1892, 0x1A8A, 0x6D8A, 0x8082)
FULL_WORKER_WITNESS = (WORKER_WITNESS + WORKER_COPY_WITNESS
                       + (0x8393, 0x8193, 0xF4AB, 0x8C93)
                       + (0x108A, 0x6D8A, 0xFD30, 0x8082, 0x6DEA, 0x0003)
                       + WORKER_LOGIC_WITNESS + WORKER_TAIL_WITNESS)
TIMER_VECTOR_BASE = 0x2204C
TIMER_VECTOR_WITNESS = (0x4A1E, 0xF495, 0xF882, 0x3D73)
TIMER_RETURN_BASE = 0x24044
IDLE_BASE = 0x298D0
IDLE_WITNESS = (0x4A11, 0xEEFF, 0x10F8, 0xA531, 0xF844, 0x98E9,
                0x7711, 0x0025, 0x7681, 0x0001, 0x7711, 0x0026, 0x7681, 0x0020,
                0x7711, 0x0000, 0x6881, 0xFFF7, 0x7711, 0x0000, 0x6981, 0x08C0,
                0x76F8, 0xA531, 0x0001, 0xF4E1, 0xEE01, 0x8A11, 0xF4E4)
IDLE_WRAPPER_BASE = 0x23D2F
IDLE_WRAPPER_WITNESS = (0x4918, 0x68F8, 0x0018, 0xFFFE, 0xF7BE, 0xF495,
                        0x4A0B, 0x4A0B, 0xF982, 0x98D0, 0xF6BE, 0x8A18,
                        0xF495, 0xF495, 0xFC00)
BOOTSTRAP_BASE = 0xFF80
BOOTSTRAP_WITNESS = (0x771D, 0xFFC8, 0x7710, 0x0060, 0xF070, 0xFF20,
                     0x7C90, 0x0060, 0x771D, 0xFFE8, 0xF880, 0x0E41)
BOOT_MEMORY_SHIFT_BASE = 0x3291
BOOT_MEMORY_SHIFT_WITNESS = (0x6FE6, 0x0010, 0x0D48)
BOOT_BLOCK_REPEAT_BASE = 0x32EC
BOOT_BLOCK_REPEAT_WITNESS = (0x891A, 0xF495, 0xF072, 0x32F4,
                             0x1386, 0xF678, 0xF3E8, 0xF1A0, 0x8196)


def decode_block_repeat_setup(opcode, literal, address):
    """SPRU172C RPTB[D] register geometry, not a pipeline simulation."""
    if opcode not in (0xF072, 0xF272):
        raise ValueError('expected RPTB or RPTBD')
    if not 0 <= literal <= 0xFFFF or not 0 <= address <= 0x7FFFFF:
        raise ValueError('block-repeat operand or address out of range')
    delayed = opcode == 0xF272
    return {
        'operation': 'RPTBD' if delayed else 'RPTB',
        'rsa': (address + (4 if delayed else 2)) & 0xFFFF,
        'rea': literal,
        'delay_words': 2 if delayed else 0,
        'instruction_words': 2,
        'nominal_cycles': 2 if delayed else 4,
        'interruptible': True,
        'repeatable': False,
        'count_rule': 'BRC+1 block executions',
        'counter_phase': 'last block instruction Decode',
        'source': 'SPRU172C RPTB[D]; SPRU131G sections 6.8 and 7.5.8',
        'scope': 'static architectural contract; no staged execution acceptance',
    }


def decode_boot_block_repeat(words):
    for offset, expected in enumerate(BOOT_BLOCK_REPEAT_WITNESS):
        address = BOOT_BLOCK_REPEAT_BASE + offset
        if words.get(address) != expected:
            raise ValueError(f'boot block-repeat witness changed or absent at {address:06x}')
    return {
        'store': {'address': '0032ec', 'operation': 'STLM B,BRC', 'opcode': '891a'},
        'latency_word': {'address': '0032ed', 'operation': 'NOP', 'opcode': 'f495'},
        'setup': {'address': '0032ee', 'operation': 'RPTB', 'opcode': 'f072', 'end_literal': '32f4'},
        'setup_contract': decode_block_repeat_setup(0xF072, 0x32F4, 0x32EE),
        'block': {'first_word': '0032f0', 'last_word': '0032f4',
                  'words': [f'{word:04x}' for word in BOOT_BLOCK_REPEAT_WITNESS[4:]],
                  'instructions': ['LDU *AR6,B', 'SFTA B,-8,A',
                                   'SFTL B,8,B', 'OR A,0,B', 'STL B,*AR6+'],
                  'data_result': 'swap the two bytes of each unsigned 16-bit source word in place',
                  'pointer_result': 'AR6 advances once per block execution',
                  'source': 'SPRU172C LDU, SFTA, SFTL, OR syntax 4, STL syntax 1',
                  'scope': 'instruction decode and desk arithmetic only; no executed block acceptance'},
        'scope': 'nine pinned original AA55 words; spacing satisfies SPRU131G table 7-3 category 9; '
                 'no staged block-repeat execution, iterations, interrupt or endpoint acceptance',
    }


def decode_extended_shift_load(words, address):
    """Decode LD syntax 4; address calculation and stage timing are not inferred."""
    opcode = words.get(address)
    if opcode is None or opcode & 0xff00 != 0x6f00 or opcode & 0xff > 0xf8:
        raise ValueError(f'expected extended Smem opcode at {address:06x}')
    mode = opcode & 0xff
    extended = mode >= 0xe0
    operation_address = address + 1 + extended
    extension = words.get(operation_address)
    if extension is None or extension & 0xfee0 != 0x0c40:
        raise ValueError(f'expected LD shift extension at {operation_address:06x}')
    literal = words.get(address + 1) if extended else None
    if extended and literal is None:
        raise ValueError(f'missing address extension at {address + 1:06x}')
    shift = extension & 0x1f
    return {
        'address': f'{address:06x}', 'opcode': f'{opcode:04x}',
        'operation': 'LD Smem,SHIFT,dst', 'destination': 'B' if extension & 0x100 else 'A',
        'shift': shift - 32 if shift & 16 else shift,
        'address_mode': f'{mode:02x}',
        'address_extension': f'{literal:04x}' if extended else None,
        'operation_extension_address': f'{operation_address:06x}',
        'operation_extension': f'{extension:04x}', 'words': 3 if extended else 2,
        'status': 'SXM extends source; OVM saturates shifted result; OVdst is affected',
        'scope': 'instruction fields only; no DAGEN, sampling, publication or pipeline acceptance',
    }


def decode_boot_memory_shift(words):
    for offset, expected in enumerate(BOOT_MEMORY_SHIFT_WITNESS):
        address = BOOT_MEMORY_SHIFT_BASE + offset
        if words.get(address) != expected:
            raise ValueError(f'boot memory-shift witness changed or absent at {address:06x}')
    return decode_extended_shift_load(words, BOOT_MEMORY_SHIFT_BASE)


def decode_bootstrap_repeat(words):
    for offset, expected in enumerate(BOOTSTRAP_WITNESS):
        address = BOOTSTRAP_BASE + offset
        if words.get(address) != expected:
            raise ValueError(f'bootstrap repeat witness changed or absent at {address:06x}')
    first, count = BOOTSTRAP_WITNESS[7], BOOTSTRAP_WITNESS[5] + 1
    last = first + count - 1
    known = sum(address in words for address in range(first, last + 1))
    return {
        'segment': 'aa55', 'matched_word_range': ['00ff80', '00ff8b'],
        'matched_words': len(BOOTSTRAP_WITNESS),
        'setup': {'pmst_before': 'ffc8', 'ar0_initial': '0060'},
        'repeat': {'address': '00ff84', 'opcode': 'f070', 'operand': 'ff20',
                   'executions': count, 'body': ['7c90', '0060'],
                   'operation': 'MVPD 0060,*AR0+',
                   'program_source_range': [f'{first:04x}', f'{last:04x}'],
                   'data_destination_range': [f'{first:04x}', f'{last:04x}'],
                   'ar0_after': f'{last + 1:04x}',
                   'source_increment': 'single-repeat implicit program-address increment',
                   'destination_increment': 'encoded AR0 postincrement',
                   'uploaded_source_words': known,
                   'source_words_not_in_upload': count - known},
        'continuation': {'pmst_after': 'ffe8', 'branch': 'FB 00:0e41'},
        'scope': '12 pinned original words; TI single-repeat/MVPD address semantics; '
                 'not source-hole values, staged bus placement or native execution acceptance',
    }


def idle_route(flag):
    if not isinstance(flag, int) or not 0 <= flag <= 0xFFFF:
        raise ValueError('idle flag must be an unsigned 16-bit word')
    return 'skip_setup' if flag else 'initialize_before_idle'


def decode_idle_contract(words):
    for base, witness in ((IDLE_BASE, IDLE_WITNESS), (IDLE_WRAPPER_BASE, IDLE_WRAPPER_WITNESS)):
        for offset, expected in enumerate(witness):
            address = base + offset
            if words.get(address) != expected:
                raise ValueError(f'idle lifecycle witness changed or absent at {address:06x}')
    return {
        'matched_word_ranges': [['0298d0', '0298ec'], ['023d2f', '023d3d']],
        'matched_words': len(IDLE_WITNESS) + len(IDLE_WRAPPER_WITNESS),
        'flag': {'address': 'a531', 'load': '10f8 a531', 'branch': 'BC ANEQ,98e9',
                 'coverage': {'tested': 65536, **dict(Counter(idle_route(flag) for flag in range(65536)))},
                 'local_commit': {'address': '0298e6', 'value': 1}},
        'zero_path': [
            {'address': '0298d8', 'register': 'PRD', 'value': '0001'},
            {'address': '0298dc', 'register': 'TCR', 'value': '0020'},
            {'address': '0298e0', 'register': 'IMR', 'operation': 'ANDM #fff7'},
            {'address': '0298e4', 'register': 'IMR', 'operation': 'ORM #08c0'},
            {'address': '0298e6', 'memory': 'a531', 'value': '0001'},
        ],
        'common_tail': ['IDLE 1', 'FRAME +1', 'POPM AR1', 'FRET'],
        'wrapper': {'alignment': 'save old SP in B; clear SP bit 0', 'call': 'FCALL 02:98d0',
                    'return': 'clear CPL; POPM SP; two NOPs; RET'},
        'scope': '44 original words and exhaustive local predicate; no global flag-writer census, wake source or full runtime acceptance',
    }


def decode_timer_wrapper(words):
    for offset, expected in enumerate(TIMER_VECTOR_WITNESS):
        address = TIMER_VECTOR_BASE + offset
        if words.get(address) != expected:
            raise ValueError(f'timer vector witness changed or absent at {address:06x}')
    if words.get(TIMER_RETURN_BASE) != 0xF4E5:
        raise ValueError('timer return witness changed or absent at 024044')
    return {
        'uploaded_vector_range': ['02204c', '02204f'],
        'matched_words': 5,
        'save': {'opcode': '4a1e', 'register': 'XPC', 'mmr': '001e'},
        'branch': {'opcode': 'f882', 'operation': 'FB', 'target': '023d73', 'delayed': False},
        'return': {'address': '024044', 'opcode': 'f4e5', 'operation': 'FRETE', 'clears_intm': True},
        'scope': 'original upload words only; low-vector runtime mapping and complete timer entry/return require execution evidence',
    }


def decode_full_worker(words):
    for offset, expected in enumerate(FULL_WORKER_WITNESS):
        address = WORKER_BASE + offset
        if words.get(address) != expected:
            raise ValueError(f'complete worker witness changed or absent at {address:06x}')
    return {
        'matched_word_range': ['023dbc', '023de1'],
        'matched_words': len(FULL_WORKER_WITNESS),
        'body_word_range': ['023dc5', '023dda'],
        'body_words': 22,
        'body_entry': 'second source word equals zero; other values branch to 023ddb',
        'conditional_store': 'CMPR EQ,AR3 compares against AR0; XC TC controls STL at 023dd2',
        'buffer_publications': 'copied source word followed by high/low B and T',
        'return': 'saved INTM predicate and two-word RETD delay',
        'scope': '38 original words pinned; no real operand provenance, full IRQ context or native recording proof',
    }


def decode_worker_logic(words):
    for offset, expected in enumerate(WORKER_LOGIC_WITNESS):
        address = WORKER_LOGIC_BASE + offset
        if words.get(address) != expected:
            raise ValueError(f'worker logical witness changed or absent at {address:06x}')
    return {
        'matched_word_range': ['023dd5', '023dda'],
        'matched_words': len(WORKER_LOGIC_WITNESS),
        'sequence': ['ADD #4,A', 'AND *AR2+,A', 'OR *AR2-,A', 'MAR *AR2-', 'STL A,*AR2'],
        'memory_operands': 'AND and OR zero-extend their Smem words independently of SXM',
        'pointer_effect': 'increment, decrement, decrement; final store uses initial AR2 minus one',
        'scope': 'six pinned words; memory meanings and complete worker/recorder behavior unclassified',
    }


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
    result['worker_logic'] = decode_worker_logic(words)
    result['full_worker'] = decode_full_worker(words)
    result['timer_wrapper'] = decode_timer_wrapper(words)
    result['idle_lifecycle'] = decode_idle_contract(words)
    bootstrap_words = uploaded_word_image(bytes.fromhex(source.decode('ascii')), 'aa55')
    result['bootstrap_repeat'] = decode_bootstrap_repeat(bootstrap_words)
    result['boot_memory_shift'] = decode_boot_memory_shift(bootstrap_words)
    result['boot_block_repeat'] = decode_boot_block_repeat(bootstrap_words)
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
