"""Read-only original R060 context/worker contracts; no full-ISR claim."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import sys

if __package__:
    from .mu4_clock_contract import SOURCE_SHA256
    from .noki5510_a00_inventory import uploaded_word_image
else:
    from mu4_clock_contract import SOURCE_SHA256
    from noki5510_a00_inventory import uploaded_word_image


BASE = 0x23D73
RAW_OUTPUT_COPY_WITNESSES = {
    0x24885: (0x7110,0x0011,0x7212,0xaae9,0x771a,0x03ff,0xf072,0x488e,0x5692,0x4e91),
    0x24ac4: (0x1004,0x8811,0x1009,0x8812,0x771a,0x007f,0xf072,0x4acd,0x5691,0x4e92),
    0x24ae7: (0x1004,0xf000,0x0380,0x8811,0x1009,0x8812,0x771a,0x007f,0xf072,0x4af2,0x5692,0x4e91),
    0x25c0b: (0xf072,0x5c14,0x5682,0x5784,0xe598,0xe5ba,0xe558,0xe57a,0x4e8b,0x4f8d),
}


def decode_raw_output_copies(words):
    for base, witness in RAW_OUTPUT_COPY_WITNESSES.items():
        if any(words.get(base + i) != word for i, word in enumerate(witness)):
            raise ValueError('raw output copy source changed or absent')
    return {'pinned_words': 42,
            'copies': [{'load': '02488d', 'store': '02488e', 'pairs': 1024,
                        'source': 'AR2 from data[aae9]', 'destination': 'AR1 from direct operand $10 under current CPL'},
                       {'load': '024acc', 'store': '024acd', 'pairs': 128,
                        'source': 'AR1 from direct operand $04 under current CPL',
                        'destination': 'AR2 from direct operand $09 under current CPL'},
                       {'load': '024af1', 'store': '024af2', 'pairs': 128,
                        'source': 'AR2 from data[SP:09]', 'destination': 'AR1 from data[SP:04] + 0x380'}],
            'permutation': {'first_word': '025c0b', 'last_word': '025c14',
                            'body': 'DLD AR2/A; DLD AR4/B; four single-word MVDD transfers; DST A/AR3; DST B/AR5',
                            'scope': 'Body only; pointer setup, iteration count and complete in-place permutation not validated'},
            'scope': 'Copy grammar only; live GENPC is not retained instruction ownership, and upstream sample arithmetic is unvalidated'}


BIT_WINDOW_BASE = 0x2510C
NIBBLE_BASE = 0x2C62B
BYTE_COUNT_BASE = 0x2AB09
ESCAPE_STORE_BASE = 0x3BDB6
POLY_CALLER_BASE = 0x260B3
SQUARE_LOOP_BASE = 0x260CD
REGISTER_PRODUCT_BASE = 0x25B0D
REGISTER_PRODUCT_WITNESS = (0x1024,0xf010,0x0003,0x880e,0xf062,0x0010,0xf461,0xf48c,0x801d)
SQUARE_LOOP_WITNESS = (0x771a,0x0002,0xf272,0x60ee,0x7715,0x1179,0x7714,0x1176,0xe800,0xa610,0xf451,0x3882,0x8294,0x8084)
POLY_CALLER_WITNESS = (0x30f8,0x1170,0x7712,0xc22a,0x4492,0x4592,0xec02,0x3692,0x4ef8,0x1174)
ESCAPE_STORE_WITNESS = (0x1106,0x08f8,0x000b,0xf843,0xbdd2,0xf640,0x10f8,0x0008,0x4e0a,0x560a,0x7002,0x0011,0x4e00)
BYTE_COUNT_WITNESS = (0x10e1,5,0xf010,1,0x7102,0x0011,0x4808,0xf47d,0xf000,1,0x4808,0x4ee1,6)
SERVICE_GATE_BASE = 0x204C1
SERVICE_GATE_WITNESS = (0x10e1,6,0xf845,0x04d2,0x44f8,0x15d9,0x3ce1,0x16,
                        0x82f8,0x12,0xf495,0xf495,0x6c82,0x04d2,
                        0x76f8,0xaa56,1,0x10f8,0xaa56,0xf845,0x05ae)
POST_FLAG_BASE = 0x2532E
POST_FLAG_WITNESS = (0xe801,0xf982,0x510c,0x80f8,0xba66,0x10f8,8,0xf845,0x5343,
                     0x1081,0xf844,0x533f,0xe800,0xf982,0x4538,0xf073,0x5343,
                     0xf020,0xba66,0xf982,0x5232)
FLAG_CONTEXT_BASE = 0x25F94
FLAG_CONTEXT_WITNESS = (
    0x4a11,0xeeff,0xf495,0x7104,0x13,0x7105,0x12,0x8811,0x6ce3,0xfffe,0x5fae,
    0x7681,0,0x76e1,4,8,0x10e2,4,0x80e1,6,0x10e2,5,0x80e1,7,0xf073,0x5fbe,
    0x7681,1,0x76e1,4,1,0x10e2,2,0x80e1,6,0x10e2,3,0x80e1,7,0x76e1,9,1,
    0xee01,0x8a11,0xf4e4)
RECORD_HEADER_BASE = 0x3A930
RECORD_HEADER_WIDTHS = (12,1,2,1,2,4,1,3,1,1,1,1,13,11,2)
RECORD_WINDOW_BASE = 0x3EE2B
RECORD_WINDOW_WITNESS = (0x4a11,0xeeed,0xf495,0x8002,0x101b,0x001a,0x7118,0x0011,
                         0x4808,0x4e0a,0x4e00,0x5681,0xf983,0xe69b,0xf6b8,0x4e04)


def decode_recording_window_arguments(words):
    if any(words.get(RECORD_WINDOW_BASE + offset) != word for offset, word in enumerate(RECORD_WINDOW_WITNESS)):
        raise ValueError('AA88 recording-window prefix changed or absent')
    return {'entry': '03ee2b', 'last_word': '03ee3a', 'pinned_words': 16,
            'span': 'LD SP(1b),A; ADD SP(1a),A; retain low word and pass as double word',
            'cursor': 'MVDK SP(18),AR1; DLD *AR1,A; FCALL 03e69b',
            'observed_frame_words_16_1b': ['0000','0020','ffa0','0000','001c','0004'],
            'scope': 'Prefix and observed caller frame; not complete modulo helper, partial-tail or offset-ownership acceptance'}


def reference_recording_windows(source, state, *, reverse=True):
    """Bounded host model: aligned 32-byte chunks, selected first four bytes."""
    if len(source) % 32 or len(state) != 82 or any(not 0 <= word <= 0xffff for word in state):
        raise ValueError('recording-window model needs complete aligned chunks and word state')
    result = bytearray(source)
    for offset in range(0, len(result), 32):
        words = [int.from_bytes(result[offset+i:offset+i+2], 'little') for i in (0,2)]
        decoded = reference_transform_block(*words, state, reverse=reverse)
        result[offset:offset+4] = b''.join(word.to_bytes(2,'little') for word in decoded)
    return bytes(result)


def decode_recording_header_writer(words):
    fields = []
    for index, width in enumerate(RECORD_HEADER_WIDTHS):
        address = RECORD_HEADER_BASE + 10 * index
        block = tuple(words.get(address + offset) for offset in range(10))
        if (block[:3] != (0xf7b8,0x10e6,0x227 + index) or
                block[3:6] not in ((0x7602,width,0x4e00), (0x4e00,0x7602,width)) or
                block[6:] != (0xfb83,0xacc5,0xf495,0x4811)):
            raise ValueError(f'AA88 header-writer field {index} changed or absent')
        fields.append({'field': index, 'source_offset': f'{0x227 + index:04x}',
                       'width': width, 'call': f'{address + 6:06x}'})
    return {'entry': '03a930', 'last_word': '03a9c5', 'pinned_words': 150,
            'fields': fields, 'total_bits': sum(RECORD_HEADER_WIDTHS),
            'writer': '03acc5', 'arguments': '32-bit value at caller SP(0..1), width at SP(2), receiver A',
            'scope': 'Header field/call grammar only; not CPU packing, file offsets or AAC payload validity'}


def reference_recording_header(values):
    if len(values) != len(RECORD_HEADER_WIDTHS):
        raise ValueError('recording header requires fifteen fields')
    bits = ''
    for value, width in zip(values, RECORD_HEADER_WIDTHS):
        if not 0 <= value < 1 << width:
            raise ValueError('recording header field outside its width')
        bits += f'{value:0{width}b}'
    return int(bits, 2).to_bytes(7, 'big')


def recording_header_observations(text, frames=8):
    if frames not in range(1, 9):
        raise ValueError('header observation requires one to eight bounded frames')
    overlay = ('mu4_native_recorder_overlay: PASS marker=aa88 records=4622 words=73766 '
               'checked_ram_words=73762 excluded_register_words=4 before_execution=1')
    if text.count(overlay) != 1 or text.find('mu4_record_header_argument:') < text.index(overlay):
        raise ValueError('header observation lacks its preceding verified AA88 upload')
    pattern = (r'mu4_record_header_argument: field=(\d+) ordinal=(\d+) caller=([0-9a-f]{6}) '
               r'high=([0-9a-f]{4}) low=([0-9a-f]{4}) width=(\d+) receiver=([0-9a-f]{4}) '
               r'sp=([0-9a-f]{4}) t=([0-9.]+) context_pc=([0-9a-f]{6}) '
               r'boundary=data_read ownership=unvalidated observer_writes=0')
    rows = re.findall(pattern, text)
    if len(rows) != 15 * frames or text.count('mu4_record_header_argument:') != len(rows):
        raise ValueError('incomplete, duplicate or malformed header arguments')
    result, previous = [], 0.0
    for frame in range(frames):
        values, receiver = [], None
        for field, row in enumerate(rows[frame*15:(frame+1)*15]):
            index, ordinal, caller, high, low, width, pointer, sp, time, pc = row
            when = float(time)
            if (int(index) != field or int(ordinal) != frame + 1 or
                    int(caller,16) != 0x3a93a + 10*field or int(width) != RECORD_HEADER_WIDTHS[field] or
                    not 0x3acc8 <= int(pc,16) <= 0x3accf or not 0.8 <= when <= 2 or when <= previous or
                    (receiver is not None and receiver != pointer)):
                raise ValueError('header argument identity, order, receiver or scope mismatch')
            receiver, previous = pointer, when
            values.append((int(high,16) << 16) | int(low,16))
        result.append({'ordinal': frame+1, 'values': values,
                       'header_hex': reference_recording_header(values).hex(), 'receiver': receiver})
    closed = re.findall(r'mu4_record_header_taps_closed: t=([0-9.]+) observer_writes=0', text)
    if len(closed) != 1 or text.count('mu4_record_header_taps_closed:') != 1 or not 2 < float(closed[0]) < 3:
        raise ValueError('missing or invalid header observation closure')
    return {'frames': result, 'argument_records': len(rows),
            'scope': 'Data-read-selected caller contexts and scalar header model; not retained CPU arguments, encoded payload or native audio'}


def decode_flag_context(words):
    if any(words.get(FLAG_CONTEXT_BASE + offset) != word for offset, word in enumerate(FLAG_CONTEXT_WITNESS)):
        raise ValueError('AA99 flag-context helper changed or absent')
    return {'entry': '025f94', 'last_word': '025fc0', 'pinned_words': 45,
            'selector': 'MVDK *SP(4),AR3; BANZ *(AR3-2),025fae',
            'selector_2': 'store 0 through AR1 and 8 at receiver+4',
            'other_selector': 'store 1 through AR1 and 1 at receiver+4',
            'scope': 'Complete local helper; field/codec names and input parsing remain unproved'}


def reference_flag_context(selector):
    if selector not in range(4):
        raise ValueError('flag-context selector outside recovered two-bit domain')
    return int(selector != 2)


def decode_post_flag_branch(words):
    if any(words.get(POST_FLAG_BASE + offset) != word for offset, word in enumerate(POST_FLAG_WITNESS)):
        raise ValueError('AA99 post-flag branch changed or absent')
    return {'entry': '02532e', 'last_word': '025342', 'pinned_words': 21,
            'request': 'LD #1,A; FCALL 02510c; STL A,ba66',
            'zero_flag': 'BC AEQ skips to 025343',
            'nonzero_flag': 'LD *AR1,A; BC ANEQ selects 02533f',
            'zero_indirect_word': 'LD #0,A; FCALL 024538 at 02533b',
            'nonzero_indirect_word': 'LD #ba66,A; FCALL 025232',
            'scope': 'Local branch only; AR1 field semantics, stream alignment and codec validity unproved'}


def post_flag_route(flag, indirect_word):
    if flag not in (0, 1) or not 0 <= indirect_word <= 0xffff:
        raise ValueError('post-flag route outside one-bit/16-bit domain')
    if not flag:
        return 'skip'
    return 'conditional_parser' if indirect_word else 'restore'


def decode_service_gate(words):
    if any(words.get(SERVICE_GATE_BASE + offset) != word for offset, word in enumerate(SERVICE_GATE_WITNESS)):
        raise ValueError('AA99 processing-latch gate changed or absent')
    if tuple(words.get(a) for a in range(0x20499, 0x2049d)) != (0x7211,0xaa59,0x7217,0xaa60):
        raise ValueError('AA99 service receiver loads changed or absent')
    return {'entry': '0204c1', 'last_word': '0204d5', 'pinned_words': 25,
            'receiver': 'MVDM aa59,AR1; MVDM aa60,AR7 (memory to register, not stores)',
            'first_skip': 'receiver word 6 equals zero',
            'sum': 'ADD receiver word 22,16 to LD 15d9,16; STH result to AR2',
            'second_skip': 'BANZ AR2 nonzero skips aa56=1',
            'tail': 'LD aa56,A; BC AEQ,05ae bypasses processing when zero',
            'scope': 'Pinned local source; no flag ownership, producer closure or codec/audio acceptance'}


def decode_byte_count_prefix(words):
    if any(words.get(BYTE_COUNT_BASE + offset) != word for offset, word in enumerate(BYTE_COUNT_WITNESS)):
        raise ValueError('AA99 byte-count prefix changed or absent')
    return {'entry': '02ab09', 'last_word': '02ab15', 'pinned_words': 13,
            'formula_domain': 'positive 16-bit bit count', 'formula': 'ceil(bits / 8)',
            'dependency': 'f000 0001 ADD followed by 4808 LDM AL,A',
            'destination': 'double word at AR1+6; low count at AR1+7',
            'scope': 'Original scalar prefix, not all constructor branches or initializer argument ownership'}


def decode_escape_store_prefix(words):
    if any(words.get(ESCAPE_STORE_BASE + offset) != word for offset, word in enumerate(ESCAPE_STORE_WITNESS)):
        raise ValueError('AA88 escape store/load prefix changed or absent')
    return {'entry': '03bdb6', 'last_word': '03bdc2', 'pinned_words': 13,
            'dependency': 'DST A,*SP(a) followed by DLD *SP(a),A',
            'scope': 'Original source prefix; execution and pending-write bypass require the isolated CPU fixture'}


def decode_poly_caller(words):
    if any(words.get(POLY_CALLER_BASE + offset) != word for offset, word in enumerate(POLY_CALLER_WITNESS)):
        raise ValueError('AA99 polynomial caller changed or absent')
    return {'entry': '0260b3', 'last_word': '0260bc', 'pinned_words': 10,
            'sequence': 'LD 1170,T; STM c22a,AR2; LD *AR2+,16,A; LD *AR2+,16,B; RPT #2; POLY *AR2+; DST A,1174',
            'scope': 'Original sequential source, not proof of dynamic predecessor execution or complete decoder validity'}


def decode_square_loop(words):
    if any(words.get(SQUARE_LOOP_BASE + offset) != word for offset, word in enumerate(SQUARE_LOOP_WITNESS)):
        raise ValueError('AA99 square-accumulate block changed or absent')
    return {'entry':'0260cd', 'last_word':'0260da', 'pinned_words':14,
            'operation':'0260d8: SQURA *AR2,A',
            'scope':'Original RPTB setup and local square source; not full loop or execution acceptance'}


def decode_register_product_caller(words):
    if any(words.get(REGISTER_PRODUCT_BASE + offset) != word for offset, word in enumerate(REGISTER_PRODUCT_WITNESS)):
        raise ValueError('AA99 accumulator/T product caller changed or absent')
    return {'entry':'025b0d', 'last_word':'025b15', 'pinned_words':9,
            'operation':'025b14: MPYA A',
            'scope':'Original local source and T producer; not dynamic operand ownership or complete decoder acceptance'}


def reference_poly_step(a, b, t, coefficient, st1):
    if not (0 <= a < 1 << 40 and 0 <= b < 1 << 40 and
            all(0 <= value <= 0xffff for value in (t, coefficient, st1))):
        raise ValueError('POLY operand outside word/accumulator domain')
    high = (a >> 16) & 0x1ffff
    if high & 0x10000:
        high -= 0x20000
    signed_t = t - 0x10000 if t & 0x8000 else t
    signed_b = b - (1 << 40) if b & (1 << 39) else b
    result = ((high * signed_t * (2 if st1 & 0x40 else 1) + signed_b + 0x8000) // 0x10000) * 0x10000
    overflow = not -0x80000000 <= result <= 0x7fffffff
    if overflow and st1 & 0x200:
        result = max(-0x80000000, min(0x7fffffff, result))
    if coefficient & 0x8000 and st1 & 0x100:
        coefficient -= 0x10000
    return result & ((1 << 40)-1), (coefficient * 0x10000) & ((1 << 40)-1), overflow


def playback_poly_observation(text):
    rows = []
    for line in text.splitlines():
        if 'mu4_playback_poly_bus:' not in line:
            continue
        pairs = [token.split('=',1) for token in line.split('mu4_playback_poly_bus:',1)[1].split()]
        if any(len(pair) != 2 for pair in pairs) or len(dict(pairs)) != len(pairs):
            raise ValueError('malformed polynomial bus record')
        rows.append(dict(pairs))
    keys = {'kind','address','value','ordinal','st0','st1','pmst','t','context_pc','boundary','ownership','observer_writes'}
    if any(set(row) != keys for row in rows):
        raise ValueError('polynomial bus record fields changed')
    if len(rows) != 8 or [(r['kind'],r['address']) for r in rows] != (
            [('read',f'{a:04x}') for a in (0x1170,0xc22a,0xc22b,0xc22c,0xc22d,0xc22e)] +
            [('write','1174'),('write','1175')]):
        raise ValueError('polynomial bus footprint incomplete or reordered')
    if [row['ordinal'] for row in rows] != ['1','2','3','4','5','6','1','2']:
        raise ValueError('polynomial bus ordinals changed')
    words = [int(r['value'],16) for r in rows]
    st1, pmst = int(rows[0]['st1'],16), int(rows[0]['pmst'],16)
    if not 0 <= st1 <= 0xffff or not 0 <= pmst <= 0xffff or pmst & 2 or any(not 0 <= word <= 0xffff for word in words):
        raise ValueError('polynomial observation outside admitted SMUL-clear/word domain')
    times = [float(r['t']) for r in rows]
    if times != sorted(set(times)) or not 0.9 <= times[0] < times[-1] <= 1.2:
        raise ValueError('polynomial bus timestamps outside ordered scope')
    for row in rows:
        if (row['boundary'] != 'data_bus' or row['ownership'] != 'unvalidated' or row['observer_writes'] != '0' or
                int(row['st1'],16) != st1 or int(row['pmst'],16) != pmst or
                not 0x260a0 <= int(row['context_pc'],16) <= 0x260d0):
            raise ValueError('polynomial observation identity/configuration changed')
    def extended(word):
        return ((word - 0x10000 if word & 0x8000 and st1 & 0x100 else word) * 0x10000) & ((1 << 40)-1)
    a, b = extended(words[1]), extended(words[2])
    for coefficient in words[3:6]:
        a, b, _ = reference_poly_step(a,b,words[0],coefficient,st1)
    actual = (words[6] << 16) | words[7]
    if actual != a & 0xffffffff:
        raise ValueError('native polynomial store disagrees with captured inputs')
    return {'coefficient_reads':5, 'steps':3, 'result':f'{actual:08x}',
            'scalar_match':True, 'native_audio_validated':False,
            'scope':'Actual bus reads/stores and pinned caller; live PC is context, not retained ownership or retirement proof'}


def reference_byte_count(bits):
    if not 1 <= bits <= 0xffff:
        raise ValueError('byte-count prefix outside recovered positive domain')
    return (bits + 7) // 8


NIBBLE_WITNESS = (
    0x4A11, 0xEEFF, 0xF495, 0x8000, 0xE803, 0xF6B8, 0x0804, 0x8004,
    0x7100, 0x0011, 0x6F04, 0x0C82, 0x1004, 0xF484, 0x880E, 0xEE01,
    0x1481, 0x8A11, 0xF030, 0x000F, 0xF495, 0xF495, 0xF4E4,
)


def decode_nibble_selector(words):
    if any(words.get(NIBBLE_BASE + offset) != word for offset, word in enumerate(NIBBLE_WITNESS)):
        raise ValueError('AA99 nibble-selector source witness changed or absent')
    return {'entry': '02c62b', 'last_word': '02c641', 'pinned_words': 23,
            'selector_domain': [0, 3], 'shift': '4 * (3 - selector)',
            'dependency': '6f04 0c82 shifted store followed immediately by 1004 load',
            'scope': 'Scalar contract for complete original helper; not cipher or native playback acceptance'}


def reference_nibble(value, selector):
    if not 0 <= value <= 0xffff or not 0 <= selector <= 3:
        raise ValueError('nibble selector outside recovered scalar domain')
    return (value >> (4 * (3 - selector))) & 15


def decode_prefix_transform(words):
    try:
        digest = hashlib.sha256(b''.join(words[address].to_bytes(2, 'big')
                                        for address in range(0x2c9ac, 0x2cd14))).hexdigest()
    except KeyError as error:
        raise ValueError('incomplete AA99 prefix-transform source') from error
    if digest != '472e17feff646a45de1ff5ab4c3d46cc0183297d5dff813032fc4741f6acb192':
        raise ValueError('AA99 prefix-transform source changed')
    return {'entry': '02c9ac', 'last_word': '02cd13', 'pinned_words': 872,
            'sha256_be_words': digest, 'state_base': 'b9de', 'state_words': 82,
            'rounds': 16, 'half_bits': 16, 'table_entries': [16, 16, 16, 16],
            'scope': 'Scalar transform model; not key initialization, packaging, cipher identification or playback acceptance'}


def reference_transform_block(left, right, state, *, reverse=True):
    """Independent 16-bit-half model for the mapped lookup/XOR/add sequence."""
    if len(state) != 82 or any(not 0 <= word <= 0xffff for word in (left, right, *state)):
        raise ValueError('transform block outside recovered word/state domain')
    keys = state[:18][::-1] if reverse else state[:18]
    tables = [state[18 + index * 16:34 + index * 16] for index in range(4)]
    for key in keys[:16]:
        left ^= key
        digits = [reference_nibble(left, selector) for selector in range(4)]
        mixed = (((tables[0][digits[0]] + tables[1][digits[1]]) ^
                  tables[2][digits[2]]) + tables[3][digits[3]]) & 0xffff
        left, right = right ^ mixed, left
    return right ^ keys[17], left ^ keys[16]


def decode_recording_prefix_transform(words):
    for offset, word in enumerate(NIBBLE_WITNESS):
        if words.get(0x3f0e6 + offset) != word:
            raise ValueError('AA88 recording nibble selector changed or absent')
    try:
        digest = hashlib.sha256(b''.join(words[address].to_bytes(2, 'big')
                                        for address in range(0x3f0fd, 0x3f467))).hexdigest()
    except KeyError as error:
        raise ValueError('incomplete AA88 recording transform source') from error
    if digest != 'b257221beee04b41a239bb4627de4ef051788fcf753ac510785d4a59e707b5e4':
        raise ValueError('AA88 recording transform source changed')
    return {'entry': '03f0fd', 'last_word': '03f466', 'pinned_words': 874,
            'sha256_be_words': digest, 'nibble_entry': '03f0e6',
            'state_base': 'fc6c', 'state_words': 82,
            'observed_data_caller_return': '03ee95',
            'scope': 'Mapped forward block under captured state; not header generation, state initialization or valid full encoding'}


def decode_transform_copy(words, segment):
    base = {'aa88': 0x3e438, 'aa99': 0x2d01a}.get(segment)
    if base is None:
        raise ValueError('unsupported transform copy overlay')
    expected = (0x1303, 0x7102, 0x0012, 0xfa4d, (base + 14) & 0xffff,
                0xf495, 0x8813, 0xf310, 0x0001, 0x891a, 0xf495,
                0xf072, (base + 13) & 0xffff, 0xe589, 0xf495, 0xf495, 0xf4e4)
    if any(words.get(base + offset) != word for offset, word in enumerate(expected)):
        raise ValueError('original transform-copy source changed or absent')
    return {'entry': f'{base:06x}', 'pinned_words': len(expected),
            'operation': 'MVDD Xmem,Ymem copy, not fill',
            'cpl1_far_call_arguments': {'destination': 'A -> AR3', 'source': '*SP(2) -> AR2', 'count': '*SP(3) -> B'},
            'loop': 'nonzero count: BRC=count-1, RPTB over e589',
            'scope': 'Source grammar; not caller intent, retained argument ownership or allocation/key-length validity'}


BIT_WINDOW_WITNESS = (
    0xED00, 0xF6B6, 0xF6B7, 0xF7B8, 0xF6B9, 0x4A08, 0x7712, 0xA854,
    0x1182, 0xF310, 0x0010, 0xFA4E, 0x512E, 0x7713, 0xA858, 0x7714,
    0xB331, 0x7715, 0xB332, 0x1085, 0x0084, 0x8815, 0x5783, 0xF74F,
    0xF741, 0x1285, 0xF2A0, 0x4E83, 0xE901, 0x0184, 0x8184, 0xE910,
    0x0182, 0x8182, 0x1182, 0x0900, 0x8182, 0x8A10, 0xF300, 0xFFF0,
    0xF784, 0x890E, 0x5683, 0xF48F, 0xF450, 0x32F8, 0x0010, 0xE901,
    0xF782, 0xF310, 0x0001, 0xF280, 0xF030, 0xFFFF, 0xF4E4,
)


def decode_bit_window(words):
    for offset, expected in enumerate(BIT_WINDOW_WITNESS):
        if words.get(BIT_WINDOW_BASE + offset) != expected:
            raise ValueError('AA99 bit-reader source witness changed or absent')
    return {'segment': 'aa99', 'entry': '02510c', 'return': '025142',
            'pinned_words': len(BIT_WINDOW_WITNESS), 'remaining': '00a854',
            'reservoir_high_low': ['00a858', '00a859'], 'index': '00b331', 'base': '00b332',
            'refill_branch': '025117 BCD BGT', 'refill_when': 'remaining <= 16',
            'model_width_domain': [0, 16], 'model_remaining_domain': [0, 32],
            'scope': 'Pinned scalar source and bounded reference model; not all callers, retained operands, CPU acceptance or codec validity'}


def reference_bit_window(remaining, reservoir, index, next_word, width):
    """Independent MSB-window extraction hypothesis; not codec or CPU acceptance."""
    if not (0 <= remaining <= 32 and 0 <= reservoir <= 0xffffffff and
            0 <= index <= 0xffff and 0 <= next_word <= 0xffff and 0 <= width <= 16):
        raise ValueError('bit window outside recovered scalar domain')
    if remaining <= 16:
        reservoir = ((reservoir << 16) | next_word) & 0xffffffff
        index = (index + 1) & 0xffff
        remaining += 16
    remaining -= width
    return {'value': (reservoir >> remaining) & ((1 << width) - 1),
            'remaining': remaining, 'reservoir': reservoir, 'index': index}


FILTER_PARENT_WITNESSES = (
    (0x2A10C, (0x4A11, 0x4A16, 0x4A17, 0x4A07, 0x8811, 0x1006,
               0x8816, 0x1107, 0x8917, 0xF620, 0xF845, 0xA123)),
    (0x2A12C, (0xF982, 0xA139, 0x481D, 0xF030, 0xFFFE, 0xF495, 0xF495, 0x881D)),
    (0x2A139, (0x6FE1, 0x0001, 0x0D5A, 0x8910, 0x4BE1, 0x0004, 0xF495,
               0x4A10, 0x11E1, 0x0003, 0x8913, 0x6101, 0x0001, 0xF820,
               0xA22E, 0x6101, 0xFFFE, 0xFA30, 0xA152, 0xE800, 0xF495,
               0xF273, 0xA22E, 0x8001, 0xF495)),
    (0xC72A, (0x0002, 0xA10C)),
    (0x2A138, (0xF4E4,)),
    (0x2A152, (0x6101,)),
    (0x2A22E, (0x11E1,)),
    (0x2A2C6, (0x771A,)),
    (0x205D0, (0x7711, 0xAA4E, 0x7000, 0x0011, 0x10F8, 0xAA5C, 0xF982,
               0x9891, 0x7000, 0x0011, 0x10F8, 0xAA5D, 0xF982, 0x9891,
               0x1002, 0x8000, 0x8001, 0x10F8, 0xAA5C, 0xF982, 0x988E,
               0x1003, 0x8000, 0x8001, 0x10F8, 0xAA5D, 0xF982, 0x988E)),
    (0x2988E, (0xE909, 0xF882, 0x98B8)),
    (0x298B8, (0x8812, 0xF845, 0x98C4, 0xF741, 0x0182, 0x8912, 0xF495,
               0xF495, 0x5782, 0xF84D, 0x98C4, 0xF5E6, 0xF020, 0xFFFF, 0xF4E4)),
    (0x205A5, (0x10F8, 0xAA5A, 0xF982, 0x98A9, 0x8811, 0xF495, 0xF495,
               0x6C81, 0x05B3, 0x76F8, 0xAA56, 0x0000, 0xF073, 0x05F8,
               0x71E7, 0x0012, 0xBA9C, 0x10F8, 0xAA5E)),
    (0x298A9, (0xE90F, 0xF882, 0x98B8)),
    (0x2A315, (0x4A11, 0xEEFF, 0x8811, 0xF495, 0xF495, 0x10E1, 0x000D, 0xEE01, 0x8A11, 0xF4E4)),
    (0xC70E, (0x0002, 0xA315)),
)
FILTER_CALL_SITES = (0x2A23C, 0x2A246, 0x2A250, 0x2A25A, 0x2A264)
PLAYBACK_SWITCH_CALLS = (0x2018B, 0x201DE, 0x201EC, 0x2023D, 0x20375)


def playback_control_literal_census(words):
    expected = {
        0x0495: [0x2716A],
        0xAA59: [0x2049A, 0x27A0E, 0x27A2E],
        0xAA56: [0x204B4, 0x204D0, 0x204D3, 0x205AF, 0x27A05],
    }
    witnesses = (
        (0x27167, (0x0006, 0x804A, 0x0002, 0x0495, 0xD7B8, 0xD796, 0x0001, 0x054F, 0x0002)),
        (0x27A04, (0x76F8, 0xAA56, 0x0000)),
        (0x27A0D, (0x76F8, 0xAA59, 0xD796)),
        (0x20419, (0xF6BE, 0x4A06, 0x8812, 0x46F8, 0x007F, 0xF495,
                    0xF074, 0x02D7, 0x8A06, 0xF7BE, 0xF4E4)),
        (0x27079, (0x0006, 0xD7B0, 0x0002, 0x0419, 0x0000, 0x8044, 0x0001, 0x054F)),
        (0x20478, (0x10E4, 0x0004, 0xF844, 0x0483, 0x6D94, 0x1394,
                    0x1094, 0x7194, 0x0012, 0xF5E3, 0xFC00, 0x4918,
                    0x68F8, 0x0018, 0xFFFE, 0xF7BE, 0xF495, 0x4A0B,
                    0xF495, 0x4594, 0x1B94, 0x4B94, 0x1094, 0xF5E7,
                    0xF6BE, 0xEE01, 0x8A18, 0xF495, 0xFC00)),
        (0x20454, (0xF6BE, 0x8812, 0xF495, 0xF495, 0xF495, 0xF495,
                    0x6DEA, 0x0013, 0x4492, 0xF074, 0x01B3, 0xF7BE, 0xF4E4)),
        (0x2046B, (0xF6BE, 0x8812, 0xF495, 0xF495, 0xF495, 0xF495,
                    0x4482, 0xF274, 0x01B3, 0x6DEA, 0x0004, 0xF7BE, 0xF4E4)),
    )
    for base, values in witnesses:
        if any(words.get(base + i) != value for i, value in enumerate(values)):
            raise ValueError('AA99 playback control witness changed or absent')
    hits = {literal: sorted(a for a, value in words.items() if value == literal)
            for literal in expected}
    if hits != expected:
        raise ValueError('AA99 playback control literal census changed')
    switch_calls = sorted(address for address, opcode in words.items()
                          if opcode in (0xF074, 0xF274, 0xF982, 0xFB82)
                          and words.get(address + 1) == 0x0478)
    if switch_calls != list(PLAYBACK_SWITCH_CALLS):
        raise ValueError('AA99 context-switch immediate call census changed')
    return {'segment': 'aa99', 'mapped_words_scanned': len(words),
            'literal_hits': {f'{value:04x}': [f'{a:06x}' for a in addresses]
                             for value, addresses in hits.items()},
            'initialization_record': '027167', 'destination': '00804a',
            'initialized_values': ['0002', '0495', 'd7b8', 'd796', '0001', '054f'],
            'initial_control_area': '00d796',
            'context_switch_candidate': ['020478', '020494'],
            'immediate_switch_call_candidates': [f'{a:06x}' for a in switch_calls],
            'service_helpers_calling_0201b3': ['020454', '02046b'],
            'enqueue_wrapper': '020419', 'enqueue_wrapper_call': '02041f',
            'activation_descriptor_initialization': {'source': '027079', 'destination': '00d7b0',
                                                     'values': ['0002', '0419', '0000', '8044', '0001', '054f']},
            'scope': 'Exact literals and pinned initialization only; not instruction classification, '
                     'computed aliases, record interpreter, resume contract or decoded audio'}


def decode_filter_parent(words):
    for base, witness in (*FILTER_PARENT_WITNESSES,
                          *((site, (0xFB82, 0xA2C6)) for site in FILTER_CALL_SITES)):
        for offset, expected in enumerate(witness):
            if words.get(base + offset) != expected:
                raise ValueError(f'AA99 filter-parent witness changed or absent at {base + offset:06x}')
    hits = sorted(address for address, word in words.items() if word == 0xA2C6)
    if hits != [site + 1 for site in FILTER_CALL_SITES]:
        raise ValueError('AA99 filter-target literal census changed')
    return {'segment': 'aa99', 'mapped_words_scanned': len(words),
            'pinned_words': sum(len(w) for _, w in FILTER_PARENT_WITNESSES) + 10,
            'target': '02a2c6', 'literal_hits': len(hits),
            'delayed_call_sites': [f'{site:06x}' for site in FILTER_CALL_SITES],
            'call_form': 'FCALLD 02:a2c6, SPRU172C page 4-57',
            'parent_entry': '02a10c', 'internal_entry': '02a139',
            'internal_call': '02a12c', 'far_pointer_reference': '00c72a',
            'branch_destinations': ['02a22e', '02a152'],
            'dispatch': {'site': '0205d0', 'receiver_references': ['00aa5c', '00aa5d'],
                         'method9_entry': '02988e', 'resolver': '0298b8'},
            'upstream_selector': {'site': '0205ac', 'taken_entry': '0205b3', 'fallthrough': '0205ae',
                                  'receiver_reference': '00aa5a', 'method15_entry': '0298a9',
                                  'for_header_c6f0': {'getter': '02a315', 'field': 13,
                                                     'far_pointer_reference': '00c70e'}},
            'scope': 'Pinned source relationships and complete literal census within supplied AA99 words only; '
                     'not computed/indirect caller closure, branch decisions, subsystem identity or decoded audio'}


RECORDER_MAC_BASE = 0x3BA16
RECORDER_MAC_WITNESS = (0x7109, 0x0012, 0x10E2, 0x0001, 0x6582, 0x0009,
                        0x4816, 0xF600, 0x8812, 0xF495, 0xF7B8)


def decode_recorder_memory_mac(words):
    for offset, expected in enumerate(RECORDER_MAC_WITNESS):
        if words.get(RECORDER_MAC_BASE + offset) != expected:
            raise ValueError(f'recorder MAC witness changed or absent at {RECORDER_MAC_BASE + offset:06x}')
    return {'segment': 'aa88', 'pinned_words': len(RECORDER_MAC_WITNESS),
            'instruction': '03ba1a', 'opcode': '6582', 'literal': 9,
            'operation': 'MAC *AR2,#9,A,B', 't_publication': 'Read',
            'reference': 'SPRU172C MAC syntax 4, page 4-82',
            'scope': 'Original instruction provenance and syntax only; this static witness establishes neither staged execution nor recording acceptance'}


COPY_ADAPTER_WITNESSES = (
    (0x2A31F, (0x4A11, 0xEEFF, 0x8814, 0xF495, 0xF7B8, 0x56E4,
               0x0004, 0x54E4, 0x0002, 0xF844, 0xA369)),
    (0x2A32A, (0x10E4,0x0007,0x08E4,0x0008,0x8812,0x10E4,0x000B,
               0x08E4,0x000C,0x11F8,0x0012,0x09F8,0x0008,0xF84E,0xA33D,
               0x76E4,0x0009,0x0001,0xE721,0x11F8,0x0008,0x09F8,0x0012,
               0xF84E,0xA347,0x76E4,0x000D,0x0001,0x8811)),
    (0x2A347, (0x11E4, 0x0008, 0xF640, 0x00E4, 0x0006, 0x8813)),
    (0x2A34D, (0x4811,0xF600,0x80E4,0x0008)),
    (0x2A351, (0x11E4, 0x000C, 0xF640, 0x00E4, 0x000A, 0x8812)),
    (0x2A357, (0x4811,0xF600,0x80E4,0x000C)),
    (0x2A35B, (0x10F8, 0x0011, 0xF847, 0xA36D, 0x4811, 0xF010,
               0x0001, 0x881A, 0xF495, 0xF072, 0xA366, 0xE598, 0xF073, 0xA36D)),
)

DECODER_OUTPUT_WITNESSES = (
    (0x23D73, (0x710D,0x0011,0x7181,0xB332,0x76F8,0xB331,0x0000)),
    (0x23DE5, (0xF982,0x4610,0x800F,0xF073,0x3DEF,0xF7B8,0xF495,
               0xF020,0xFFFF,0x800F,0x10F8,0xB331,0x8010,0x710F,0x0011,0x6C81,0x3E59)),
    (0x23E09, (0x7105,0x0017,0x100C,0xF010,0x0001,0x8811,0x1006,0x800C,
               0xF020,0x0400,0x8011,0x710C,0x0012,0x7600,0x000E,0x5692,
               0x700C,0x0012,0xF982,0x48A6,0x8099,0x7600,0x000E,0x5697,
               0xF982,0x48A6,0x80E1,0x0400,0x7111,0x0012,0x6EEA,0xFFFF,0x3E14,0x7011,0x0012)),
)

PCM_CONVERSION_WITNESSES = (
    (0x248A6, (0xEEFC,0xF495,0x1106,0x7600,0,0xF784,0x7602,0,0x8101,0xF982,0x4E38,
               0xF7B8,0xF540,0xF768,0xF778,0xF84B,0x48C4,0xF540,0xF768,0xF778,0xF6B8,
               0xF495,0xF310,0x8000,0xF84B,0x48CF,0xF020,0x7FFF,0xF073,0x48CF,
               0xF540,0xF768,0xF778,0xF310,0x8001,0xF84A,0x48CF,0xF062,0xFFFF,
               0xF040,0x8001,0xEE04,0xF495,0xF4E4)),
    (0x24E38, (0x4A11,0xEEFD,0xF495,0x7107,0x0012,0x7106,0x0011,0x7108,0x0013,
               0x4911,0x01F8,0x0012,0x7000,0x0011,0x8101,0xF982,0x4D43,0x7001,
               0x0013,0x7000,0x0011,0xF982,0x4D43,0xEE03,0x8A11,0xF4E4)),
    (0x24D43, (0x4A11,0xEEFD,0xF495,0x1106,0x8102,0x7107,0x0012,0x4912,0x0902,
               0x8911,0x11F8,0x0012,0x7102,0x0012,0x09F8,0x0012,0xF84D,0x4D7F,
               0xF7B8,0x11F8,0x0011,0xF84F,0x4D60,0x7000,0x0011,0xF982,0xA7FB,
               0xF073,0x4D7F,0x6CE1,0x0001,0x4D6A,0xF000,1,0xF468,0xF478,
               0xF47F,0xF073,0x4D7F,0x11F8,0x0011,0xF310,0xFFE2,0xF84F,0x4D7E,
               0x4911,0xF793,0xF784,0x8100,0xF982,0xA807,0xF7B8,0xF000,1,
               0xF468,0xF478,0xF47F,0xF073,0x4D7F,0xE800,0xEE03,0x8A11,0xF4E4)),
    (0x2A807, (0xF7B8,0xF468,0xF478,0x6102,0x0010,0xFA30,0xA815,0x6002,0,
               0xF830,0xA819,0xF470,0x6902,0x0010,0x3202,0xF6E4,0xF482,0xF495,0xF4E4)),
)


def reference_pcm_conversion(raw):
    """Argument 14: signed DLD32, nearest rounding (ties +infinity), symmetric clamp."""
    if not isinstance(raw,int) or not 0 <= raw < 1 << 40 or raw >> 32 not in (0,255) or (raw >> 32 == 255 and not raw & 0x80000000):
        raise ValueError('expected a zero/sign-extended 32-bit DLD input')
    value = raw & 0xffffffff
    if value & 0x80000000:
        value -= 1 << 32
    shifted = (value+8192) >> 14
    return {'shifted':shifted,'word':max(-32767,min(32767,shifted)) & 0xffff}


def decode_pcm_conversion(words):
    for base,witness in PCM_CONVERSION_WITNESSES:
        for offset,expected in enumerate(witness):
            if words.get(base+offset)!=expected:
                raise ValueError(f'PCM conversion witness changed or absent at {base+offset:06x}')
    return {'segment':'aa99','pinned_words':sum(len(witness) for _,witness in PCM_CONVERSION_WITNESSES),
            'entry':'0248a6','argument':14,'shift_composition':'024e38 calls 024d43 for 0 to -14, then 0 to 0',
            'rounded_right_shift':'024d43 uses signed32 shift by 13, adds one, then shifts by one',
            'result':'clamp((signed32(raw)+8192)>>14, -32767, 32767)',
            'scope':'Argument-14 source contract only, not other shift arguments, decoder sample correctness or native audio acceptance'}


def decode_decoder_output(words):
    for base,witness in DECODER_OUTPUT_WITNESSES:
        for offset,expected in enumerate(witness):
            if words.get(base+offset)!=expected:
                raise ValueError(f'decoder output witness changed or absent at {base+offset:06x}')
    return {'segment':'aa99','pinned_words':sum(len(witness) for _,witness in DECODER_OUTPUT_WITNESSES),
            'input_setup':'023d73 initializes b332 from the receiver and resets b331',
            'processing_call':'023de5 -> 024610','processing_result':'caller SP+0x0f',
            'nonzero_result_exit':'023df4 -> 023e59',
            'conversion_loop':{'first':'023e14','count':1024,'helper':'0248a6',
                               'helper_stack0':14,'destinations':['*+AR1','*AR1(0x400)'],
                               'source_reads':['DLD *AR2+','DLD *AR7+']},
            'scope':'Pinned candidate processing/conversion boundary, not runtime call-to-packet ownership or correct decoder arithmetic'}


def reference_copy_adapter(source_extent, source_cursor, destination_extent, destination_cursor):
    """Equal-field branch in the nonnegative remaining-count domain only."""
    if any(not isinstance(value,int) or not 0 <= value <= 0x7fff
           for value in (source_extent,source_cursor,destination_extent,destination_cursor)) or source_cursor > source_extent or destination_cursor > destination_extent:
        raise ValueError('copy adapter requires nonnegative signed-word extents and bounded cursors')
    source_remaining = source_extent-source_cursor
    destination_remaining = destination_extent-destination_cursor
    count = min(source_remaining,destination_remaining)
    return {'count':count,'source_cursor_after':source_cursor+count,
            'destination_cursor_after':destination_cursor+count,
            'set_source_exhausted':source_remaining <= destination_remaining,
            'set_destination_full':destination_remaining <= source_remaining}


def decode_copy_adapter(words):
    for base, witness in COPY_ADAPTER_WITNESSES:
        for offset, expected in enumerate(witness):
            if words.get(base + offset) != expected:
                raise ValueError(f'copy adapter witness changed or absent at {base + offset:06x}')
    return {
        'segment': 'aa99', 'pinned_words': sum(len(w) for _, w in COPY_ADAPTER_WITNESSES),
        'entry': '02a31f', 'object_register': 'AR4',
        'double_fields_compared': [4, 2], 'unequal_branch': '02a369',
        'equal_path': {'source': 'object[6] + old(object[8])', 'source_register': 'AR3',
                       'destination': 'object[10] + old(object[12])', 'destination_register': 'AR2',
                       'count': 'min(object[7]-old(object[8]), object[11]-old(object[12]))',
                       'cursor_update': 'both cursors advance by count before MVDD',
                       'source_exhausted': 'set object[9]=1 when source remaining <= destination remaining',
                       'destination_full': 'set object[13]=1 when destination remaining <= source remaining',
                       'positive_count_required': True, 'body': '02a366',
                       'operation': 'MVDD *AR3+,*AR2+'},
        'scope': 'Pinned equal-field count, old-cursor addressing and advance-before-copy in the nonnegative remaining-count domain; no rate semantics, unequal-field helper, all-alias writer closure or decoded audio',
    }


DMA_PAUSE_WITNESSES = (
    ('aa22', 0x290B7, (0x7711, 0x0054, 0x6881, 0xFFF7, 0xE800, 0xF982,
                       0x97BE, 0x7711, 0x0054, 0x6981, 0x0008)),
    ('aa99', 0x27944, (0x7712, 0x0054, 0x6882, 0xFFF7, 0xE800, 0xF982,
                       0x7B92, 0x7712, 0x0054, 0x6982, 0x0008)),
)


def decode_dma_pause(words, rel_words):
    sources = {'aa22': words, 'aa99': rel_words}
    for segment, base, witness in DMA_PAUSE_WITNESSES:
        for offset, expected in enumerate(witness):
            if sources[segment].get(base + offset) != expected:
                raise ValueError(f'DMA pause witness changed or absent in {segment} at {base + offset:06x}')
    return {
        'pinned_words': 22, 'control_address': '0054', 'channel': 3,
        'clear_mask': 'fff7', 'set_mask': '0008',
        'sequences': [{'segment': segment, 'start': f'{base:06x}',
                       'paused_call': f'02{witness[6]:04x}'}
                      for segment, base, witness in DMA_PAUSE_WITNESSES],
        'scope': 'Direct DE3 pause/call/resume sequences only; no complete callee effects or PCM interpretation',
    }


MEASUREMENT_RESPONSE_BASE = 0x3B01D
MEASUREMENT_RESPONSE_WITNESS = (
    0x7608, 0x0002, 0x760A, 0x0068, 0x760B, 0x0001,
    0xF062, 0x0098, 0xF7B8, 0xF040, 0x9680, 0x54F8, 0xB64A,
    0xF847, 0xB02E, 0x760B, 0x001F, 0xF062, 0x0098, 0xF040,
    0x9680, 0x54F8, 0xB64C, 0xF847, 0xB038, 0x760B, 0x0020,
)


def measurement_result(left, right):
    """AA22's nonsaturating energy classification, not a success-code enum."""
    if any(type(value) is not int or not 0 <= value <= 0x7FFFFFFF for value in (left, right)):
        raise ValueError('measurement energies must be nonsaturating nonnegative 32-bit sums')
    return 0x20 if right < 10_000_000 else 0x1F if left < 10_000_000 else 1


def decode_measurement_response(words):
    for offset, expected in enumerate(MEASUREMENT_RESPONSE_WITNESS):
        if words.get(MEASUREMENT_RESPONSE_BASE + offset) != expected:
            raise ValueError(f'measurement response witness changed or absent at {MEASUREMENT_RESPONSE_BASE + offset:06x}')
    return {
        'segment': 'aa22', 'pinned_words': len(MEASUREMENT_RESPONSE_WITNESS),
        'selector': '68', 'default_result': 1, 'threshold': 10_000_000,
        'left': {'aggregate': 'b64a/b64b', 'below_threshold_result': 0x1F},
        'right': {'aggregate': 'b64c/b64d', 'below_threshold_result': 0x20,
                  'overrides_left': True},
        'comparison': 'RSBX C16; DSUB aggregate,A; ALEQ skips result store at equality or above',
        'scope': 'Pinned AA22 response for independently checked nonsaturating energy sums; no decoded music or analog audio claim',
    }


REL_CLEANUP_BASE = 0x2956F
REL_CLEANUP_WITNESS = (
    0x60F8, 0x373E, 0x0004, 0xF820, 0x959D, 0xF7B8, 0x7212, 0x3742,
    0x10F8, 0x0012, 0xF010, 0x0001, 0xF845, 0x958E, 0x6CE2, 0xFFFE,
    0x958E, 0x76F8, 0x373E, 0x0002, 0x4818, 0x7600, 0x0001, 0x7601,
    0x001A, 0xF000, 0x0014, 0xF983, 0x9005, 0xF073, 0x9591, 0x76F8,
    0x373E, 0x0005, 0xE801, 0xF982, 0x9189, 0x7600, 0x0001, 0xF020,
    0x3AEA, 0xF980, 0x2082, 0xE800, 0xF980, 0x2080,
)
REL_RELOAD_API = (0xF880, 0x3538)
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
REL_REPEAT_BASE = 0x2610C
REL_REPEAT_WITNESS = (
    0x4A11, 0xEEFF, 0xF7B8, 0x771A, 0x0006, 0x8811, 0xF272, 0x6120,
    0x7600, 0x0001, 0xFB82, 0x510C, 0xE801, 0xF495, 0xFA44, 0x611F,
    0xE901, 0x1000, 0x8091, 0x0100, 0x8100, 0x8181, 0xEE01, 0x8A11,
    0xF4E4,
)
REL_CALLEE_BASE = 0x2510C
REL_CALLEE_WITNESS = (
    0xED00, 0xF6B6, 0xF6B7, 0xF7B8, 0xF6B9, 0x4A08, 0x7712, 0xA854,
    0x1182, 0xF310, 0x0010, 0xFA4E, 0x512E, 0x7713, 0xA858, 0x7714,
    0xB331, 0x7715, 0xB332, 0x1085, 0x0084, 0x8815, 0x5783, 0xF74F,
    0xF741, 0x1285, 0xF2A0, 0x4E83, 0xE901, 0x0184, 0x8184, 0xE910,
    0x0182, 0x8182, 0x1182, 0x0900, 0x8182, 0x8A10, 0xF300, 0xFFF0,
    0xF784, 0x890E, 0x5683, 0xF48F, 0xF450, 0x32F8, 0x0010, 0xE901,
    0xF782, 0xF310, 0x0001, 0xF280, 0xF030, 0xFFFF, 0xF4E4,
)
REL_AR_STORE_BASE = 0x25FDC
REL_AR_STORE_WITNESS = (
    0x8814, 0x6FF8, 0x0015, 0x0C81, 0xF495, 0x11E4, 0xC22E,
    0x8192, 0x57E5, 0xC2AE, 0xFD30, 0xF784, 0x4F93,
)
REL_COPY_ENTRY_BASE = 0x24542
REL_COPY_ENTRY_WITNESS = (0x4A11, 0x4A16, 0x4A17, 0xEEF7, 0xF495,
                          0x710E, 0x0019, 0x710F, 0x0011)
REL_COPY_DEPENDENCY_BASE = 0x2459A
REL_COPY_DEPENDENCY_WITNESS = (0x7106, 0x0011, 0x7008, 0x0011, 0x1008, 0x0007)
REL_COPY_BASE = 0x245A0
REL_COPY_WITNESS = (
    0x8008, 0x4819, 0x6F08, 0x0C01, 0x8810, 0x1000, 0xF601,
    0x8811, 0x480E, 0x0806, 0xF500, 0xF010, 0x0001, 0x881A,
    0xF495, 0xF072, 0x45B2, 0x5691, 0x4E90,
)


def rel_copy_destination(stack8, bk):
    """Desk arithmetic for the pinned CPL=1 sequence, not an execution model."""
    if any(not isinstance(value, int) or not 0 <= value <= 0xFFFF for value in (stack8, bk)):
        raise ValueError('REL copy inputs must be 16-bit words')
    return (bk + 2 * stack8) & 0xFFFF


def decode_rel_copy_loop(words):
    for base, witness in ((REL_COPY_ENTRY_BASE, REL_COPY_ENTRY_WITNESS),
                          (REL_COPY_DEPENDENCY_BASE, REL_COPY_DEPENDENCY_WITNESS),
                          (REL_COPY_BASE, REL_COPY_WITNESS)):
        for offset, expected in enumerate(witness):
            address = base + offset
            if words.get(address) != expected:
                raise ValueError(f'REL copy witness changed or absent at {address:06x}')
    return {
        'segment': 'aa99', 'pinned_words': len(REL_COPY_ENTRY_WITNESS) + len(REL_COPY_DEPENDENCY_WITNESS) + len(REL_COPY_WITNESS),
        'entry_cpl_required': 1,
        'frame': {'entry': '024542', 'saved_registers': ['AR1', 'AR6', 'AR7'],
                  'local_words': 9, 'sp_delta_before_arguments': -12},
        'bk_writer': {'address': '024547', 'operation': 'MVDK *(SP+14),BK',
                      'source_is_mapped_t': False,
                      'caller_stack_offset_with_two_word_far_frame': 0},
        'destination': {
            'save_partial_index': 'STL A,*(SP+8) at 0245a0',
            'base_load': 'LDM BK,A at 0245a1',
            'arithmetic': 'ADD *(SP+8),1,A at 0245a2',
            'publish': 'STLM A,AR0 at 0245a4',
            'formula': '(BK + 2 * stack[SP+8]) & ffff',
            'short_direct_operand_is_mapped_al': False,
        },
        'dependency': {
            'source_load': 'MVDK *(SP+6),AR1 at 02459a',
            'copy': 'MVKD $0011,*(SP+8) at 02459c',
            'consumer': 'LD *(SP+8),A at 02459e',
            'following_add': 'ADD *(SP+7),A at 02459f',
            'copy_source_is_absolute_ar1': True,
            'scope': 'adjacent same-address RAM dependency; retained-copy forwarding has separate CPU acceptance',
        },
        'loop': {'setup_address': '0245af',
                 'setup': decode_block_repeat_setup(0xF072, 0x45B2, 0x245AF),
                 'instructions': ['DLD *AR1+,A', 'DST A,*AR0+'],
                 'words_copied_per_iteration': 2,
                 'source_increment_per_iteration': 2,
                 'destination_increment_per_iteration': 2},
        'examples': [
            {'stack8': 'ac78', 'bk': f'{bk:04x}',
             'destination': f'{rel_copy_destination(0xAC78, bk):04x}'}
            for bk in (0x7000, 0xE800)
        ],
        'scope': '34 pinned logical-source words and CPL=1 address arithmetic; '
                 'not input validity, physical aliasing or repeat execution acceptance',
    }


def decode_shifted_store(words, address):
    """SPRU172C STL/STH syntax 4 fields, not register execution/timing."""
    opcode = words.get(address)
    if opcode is None or opcode & 0xFF00 != 0x6F00 or opcode & 0xFF > 0xF8:
        raise ValueError(f'expected shifted Smem store opcode at {address:06x}')
    mode = opcode & 0xFF
    extended = mode >= 0xE0
    operation_address = address + 1 + extended
    extension = words.get(operation_address)
    if extension is None or extension & 0xFEE0 not in (0x0C60, 0x0C80):
        raise ValueError(f'expected STL/STH shift extension at {operation_address:06x}')
    literal = words.get(address + 1) if extended else None
    if extended and literal is None:
        raise ValueError(f'missing address extension at {address + 1:06x}')
    shift = extension & 31
    return {
        'address': f'{address:06x}', 'opcode': f'{opcode:04x}',
        'operation': 'STH' if extension & 0xFEE0 == 0x0C60 else 'STL',
        'source': 'B' if extension & 0x100 else 'A',
        'shift': shift - 32 if shift & 16 else shift,
        'address_mode': f'{mode:02x}',
        'address_extension': f'{literal:04x}' if extended else None,
        'absolute_destination': f'{literal:04x}' if mode == 0xF8 else None,
        'operation_extension_address': f'{operation_address:06x}',
        'operation_extension': f'{extension:04x}',
        'words': 3 if extended else 2,
        'source_reference': 'SPRU172C STL/STH syntax 4; SPRU131G tables 7-5 and 7-7',
        'scope': 'instruction fields only; no mapped-register publication, DAGEN conflict, '
                 'pipeline execution or audio acceptance',
    }


def decode_rel_ar_store(words):
    for offset, expected in enumerate(REL_AR_STORE_WITNESS):
        address = REL_AR_STORE_BASE + offset
        if words.get(address) != expected:
            raise ValueError(f'REL AR store witness changed or absent at {address:06x}')
    return {
        'segment': 'aa99', 'pinned_words': len(REL_AR_STORE_WITNESS),
        'preceding_store': {'address': '025fdc', 'operation': 'STLM A,AR4'},
        'store': decode_shifted_store(words, 0x25FDD),
        'destination_register': 'AR5',
        'following_instructions': [
            {'address': '025fe0', 'operation': 'NOP', 'words': 1},
            {'address': '025fe1', 'operation': 'LD *+AR4(-15826),B', 'words': 2},
            {'address': '025fe3', 'operation': 'STL B,*AR2+', 'words': 1},
            {'address': '025fe4', 'operation': 'DLD *+AR5(-15698),B', 'words': 2},
        ],
        'words_before_first_ar5_consumer': 4,
        'ar5_consumer_category': 'I (long-offset indirect DLD)',
        'required_store_latency_cycles': 2,
        'scope': '13 pinned logical-source words; static local spacing only, not a '
                 'whole-function census or executed latency/conflict acceptance',
    }


def decode_rel_repeat_call(words):
    """Pinned logical AA99 instructions, not a generic call/loop census."""
    for base, witness in ((REL_REPEAT_BASE, REL_REPEAT_WITNESS),
                          (REL_CALLEE_BASE, REL_CALLEE_WITNESS)):
        for offset, expected in enumerate(witness):
            address = base + offset
            if words.get(address) != expected:
                raise ValueError(f'REL call witness changed or absent at {address:06x}')
    setup_address, call_address = 0x26112, 0x26116
    opcode = words[call_address]
    return {
        'segment': 'aa99', 'pinned_words': len(REL_REPEAT_WITNESS) + len(REL_CALLEE_WITNESS),
        'loop': {
            'setup_address': f'{setup_address:06x}',
            'setup': decode_block_repeat_setup(words[setup_address], words[setup_address + 1], setup_address),
            'count_initializer': {'address': '02610f', 'destination': 'BRC', 'value': 6},
            'nominal_body_executions': 7,
        },
        'call': {
            'address': f'{call_address:06x}', 'opcode': f'{opcode:04x}',
            'target': f'{((opcode & 0x7f) << 16) | words[call_address + 1]:06x}',
            'return_address': f'{call_address + 4:06x}', 'stack_words': 2,
            'delay_words': ['e801', 'f495'], 'argument_a': 1,
        },
        'callee': {
            'first_word': '02510c', 'last_word': '025142', 'pinned_words': len(REL_CALLEE_WITNESS),
            'entry_cpl_required': 1,
            'branch': {'address': '025117', 'condition': 'BGT', 'target': '02512e',
                       'delay_words': ['7713', 'a858'], 'delay_operation': 'STM #a858,AR3',
                       'paths': 2, 'backward_edges': 0},
            'calls': 0, 'repeat_setups': 0, 'repeat_control_writes': [],
            'braf_writes': 0, 'cpl_writes': 0,
            'fixed_ram_writes': [
                {'instruction': '025127', 'destination': 'a858', 'words': 2},
                {'instruction': '02512a', 'destination': 'b331', 'words': 1},
                {'instruction': '02512d', 'destination': 'a854', 'words': 1},
                {'instruction': '025130', 'destination': 'a854', 'words': 1},
            ],
            'local_stack': ['PSHM AL at 025111', 'POPM AR0 at 025131'],
            'return': {'address': '025142', 'operation': 'FRET'},
        },
        'scope': '80 pinned logical-source words and two callee paths; no physical mapping, '
                 'far-call/block-repeat legality, execution completion or audio acceptance',
    }


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


def rel_cleanup_route(lifecycle, control):
    """Pinned AA99 branch arithmetic; not a codec result or EOF classification."""
    if not 0 <= lifecycle <= 0xFFFF or not 0 <= control <= 0xFFFF:
        raise ValueError('lifecycle and control must be 16-bit words')
    if lifecycle != 4:
        return {'cleanup': False, 'state': lifecycle, 'extra_call': None, 'loader_index': None}
    # SPRU172C BANZ example 3 tests effective Sind, not the unchanged AR2.
    branch_to_state5 = control == 1 or ((control - 2) & 0xFFFF) != 0
    return {'cleanup': True, 'state': 5 if branch_to_state5 else 2,
            'extra_call': None if branch_to_state5 else '039005', 'loader_index': 0}


def decode_rel_cleanup(words, bootstrap_words):
    for source, base, witness in ((words, REL_CLEANUP_BASE, REL_CLEANUP_WITNESS),
                                  (bootstrap_words, 0x2080, REL_RELOAD_API)):
        for offset, expected in enumerate(witness):
            if source.get(base + offset) != expected:
                raise ValueError(f'REL cleanup/API witness changed or absent at {base + offset:06x}')
    states = Counter(rel_cleanup_route(4, control)['state'] for control in range(0x10000))
    return {
        'segment': 'aa99', 'pinned_words': len(REL_CLEANUP_WITNESS), 'aa55_api_words': 2,
        'state_gate': {'address': '02956f', 'state_address': '373e', 'required': 4,
                       'skip': 'NTC -> 02959d'},
        'control': {'address': '029575', 'operation': 'MVDM $3742,AR2',
                    'direction': 'data_to_mmr', 'is_result_publication': False},
        'branch': {'address': '02957d', 'operation': 'BANZ 958e,*AR2(-2)',
                   'predicate': '((AR2 - 2) & 0xffff) != 0', 'ar2_modified': False,
                   'precheck': 'AR2 == 1 -> 02958e'},
        'active_control_partition': {'tested': 0x10000, 'state5': states[5],
                                     'state2': states[2], 'state2_control': 2},
        'state2_extra_call': {'target': '039005', 'caller_sp0': 1,
                              'caller_sp1': '001a', 'a': 'caller_SP + 0x14'},
        'examples': {f'{control:04x}': rel_cleanup_route(4, control)
                     for control in (0, 1, 2, 3, 0xFFFF)},
        'reload': {'address': '02959b', 'descriptor': 0, 'api': '002080',
                   'aa55_target': '003538', 'selected_image': 'MCUSI16'},
        'scope': 'Pinned branch and API contract; no EOF/error meaning, '
                 'worker-completion cause, decoded PCM or analog audio acceptance',
    }


def contract(source):
    if hashlib.sha256(source).hexdigest() != SOURCE_SHA256:
        raise ValueError('expected original R060 InitData source')
    words = uploaded_word_image(bytes.fromhex(source.decode('ascii')), 'aa22')
    result = decode_words(words)
    result['measurement_response'] = decode_measurement_response(words)
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
    rel_words = uploaded_word_image(bytes.fromhex(source.decode('ascii')), 'aa99')
    result['raw_output_copies'] = decode_raw_output_copies(rel_words)
    result['filter_parent'] = decode_filter_parent(rel_words)
    result['bit_reader'] = decode_bit_window(rel_words)
    result['nibble_selector'] = decode_nibble_selector(rel_words)
    result['prefix_transform'] = decode_prefix_transform(rel_words)
    result['byte_count_prefix'] = decode_byte_count_prefix(rel_words)
    result['playback_polynomial_caller'] = decode_poly_caller(rel_words)
    result['playback_square_loop'] = decode_square_loop(rel_words)
    result['playback_register_product_caller'] = decode_register_product_caller(rel_words)
    result['processing_latch_gate'] = decode_service_gate(rel_words)
    result['post_publication_flag_branch'] = decode_post_flag_branch(rel_words)
    result['post_flag_context'] = decode_flag_context(rel_words)
    result['playback_transform_copy'] = decode_transform_copy(rel_words, 'aa99')
    result['playback_control_literals'] = playback_control_literal_census(rel_words)
    result['copy_adapter'] = decode_copy_adapter(rel_words)
    result['decoder_output'] = decode_decoder_output(rel_words)
    result['pcm_conversion'] = decode_pcm_conversion(rel_words)
    recorder_words = uploaded_word_image(bytes.fromhex(source.decode('ascii')), 'aa88')
    result['recording_escape_store_prefix'] = decode_escape_store_prefix(recorder_words)
    result['recording_prefix_transform'] = decode_recording_prefix_transform(recorder_words)
    result['recording_header_writer'] = decode_recording_header_writer(recorder_words)
    result['recording_window_arguments'] = decode_recording_window_arguments(recorder_words)
    result['recording_transform_copy'] = decode_transform_copy(recorder_words, 'aa88')
    result['recorder_memory_mac'] = decode_recorder_memory_mac(recorder_words)
    result['dma_pause'] = decode_dma_pause(words, rel_words)
    result['rel_repeat_call'] = decode_rel_repeat_call(rel_words)
    result['rel_ar_store'] = decode_rel_ar_store(rel_words)
    result['rel_copy_loop'] = decode_rel_copy_loop(rel_words)
    result['rel_cleanup'] = decode_rel_cleanup(rel_words, bootstrap_words)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--recording-header-log', type=Path,
                        help='check the bounded first-eight-header argument observation')
    parser.add_argument('--playback-poly-log', type=Path,
                        help='check one bounded native three-step polynomial bus observation')
    args = parser.parse_args()
    try:
        result = contract(args.source.read_bytes())
        if args.recording_header_log:
            result['recording_header_observation'] = recording_header_observations(args.recording_header_log.read_text())
        if args.playback_poly_log:
            result['playback_polynomial_observation'] = playback_poly_observation(args.playback_poly_log.read_text())
    except (OSError, ValueError) as error:
        print(f'MU4 context contract: {error}', file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
