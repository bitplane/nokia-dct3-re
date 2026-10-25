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


def recording_header_observations(text, frames=8, *, session=False):
    if frames not in range(1,513 if session else 9):
        raise ValueError('header observation exceeds its bounded frame domain')
    marker = 'mu4_record_session_argument' if session else 'mu4_record_header_argument'
    last = 15 if session else 2
    overlay = ('mu4_native_recorder_overlay: PASS marker=aa88 records=4622 words=73766 '
               'checked_ram_words=73762 excluded_register_words=4 before_execution=1')
    if text.count(overlay) != 1 or text.find(marker+':') < text.index(overlay):
        raise ValueError('header observation lacks its preceding verified AA88 upload')
    pattern = (marker+r': field=(\d+) ordinal=(\d+) caller=([0-9a-f]{6}) '
               r'high=([0-9a-f]{4}) low=([0-9a-f]{4}) width=(\d+) receiver=([0-9a-f]{4}) '
               r'sp=([0-9a-f]{4}) t=([0-9.]+) context_pc=([0-9a-f]{6}) '
               r'boundary=data_read ownership=unvalidated observer_writes=0')
    rows = re.findall(pattern, text)
    if len(rows) != 15 * frames or text.count(marker+':') != len(rows):
        raise ValueError('incomplete, duplicate or malformed header arguments')
    result, previous = [], 0.0
    for frame in range(frames):
        values, receiver = [], None
        for field, row in enumerate(rows[frame*15:(frame+1)*15]):
            index, ordinal, caller, high, low, width, pointer, sp, time, pc = row
            when = float(time)
            if (int(index) != field or int(ordinal) != frame + 1 or
                    int(caller,16) != 0x3a93a + 10*field or int(width) != RECORD_HEADER_WIDTHS[field] or
                    not 0x3acc8 <= int(pc,16) <= 0x3accf or not 0.8 <= when <= last or when <= previous or
                    (receiver is not None and receiver != pointer)):
                raise ValueError('header argument identity, order, receiver or scope mismatch')
            receiver, previous = pointer, when
            values.append((int(high,16) << 16) | int(low,16))
        result.append({'ordinal': frame+1, 'values': values,
                       'header_hex': reference_recording_header(values).hex(), 'receiver': receiver})
    if session:
        scope = 'mu4_record_session_scope: first=0.8 last=15 fields=15 max_frames=512 boundary=data_read context_pc=not_owner observer_writes=0'
        closed = re.findall(r'mu4_record_session_closed: arguments=(\d+) counts=([0-9,]+) capped=0 t=([0-9.]+) observer_writes=0',text)
        terminal = f'mu4_record_session_terminal: closed=1 arguments={15*frames} capped=0 illegal=0 observer_writes=0'
        if (text.count(scope) != 1 or len(closed) != 1 or text.count('mu4_record_session_closed:') != 1
                or int(closed[0][0]) != 15*frames or closed[0][1] != ','.join([str(frames)]*15)
                or not 15 < float(closed[0][2]) < 16 or text.count(terminal) != 1
                or text.count('mu4_record_session_terminal:') != 1
                or not text.index(scope) < text.index(marker+':') <= text.rfind(marker+':') < text.index('mu4_record_session_closed:') < text.index(terminal)
                or '\0' in text or 'LUA ERROR' in text):
            raise ValueError('missing, capped, faulted or incomplete encoder session observation')
    else:
        closed = re.findall(r'mu4_record_header_taps_closed: t=([0-9.]+) observer_writes=0', text)
        if len(closed) != 1 or text.count('mu4_record_header_taps_closed:') != 1 or not 2 < float(closed[0]) < 3:
            raise ValueError('missing or invalid header observation closure')
    return {'frames': result, 'argument_records': len(rows),
            'scope': 'Data-read-selected caller contexts and scalar header model; not retained CPU arguments, encoded payload or native audio'}


def recording_session_observations(text, reference=None, *, complete_reference_prefix=False):
    if complete_reference_prefix and not reference:
        raise ValueError('complete-prefix comparison requires nonempty observed bytes')
    counts = re.findall(r'mu4_record_session_closed: arguments=\d+ counts=([0-9,]+)',text)
    if len(counts) != 1:
        raise ValueError('missing encoder session counts')
    fields = counts[0].split(',')
    if len(fields) != 15 or len(set(fields)) != 1:
        raise ValueError('encoder session field counts disagree')
    result = recording_header_observations(text,int(fields[0]),session=True)
    frames = result['frames']
    headers = [bytes.fromhex(frame['header_hex']) for frame in frames]
    offset, matched = 0, 0
    for frame,header in zip(frames,headers):
        length = frame['values'][12]
        if length < 7:
            raise ValueError('encoder declared an incomplete fixed header')
        if reference is not None and not (complete_reference_prefix and offset >= len(reference)):
            if reference[offset:offset+7] != header:
                raise ValueError(f'encoder header {frame["ordinal"]} differs from reference at byte {offset}')
            if complete_reference_prefix and offset + length > len(reference):
                raise ValueError('observed prefix ends inside a declared packet')
            matched += 1
        offset += length
    report = dict(publications=len(frames),argument_records=result['argument_records'],
                  declared_packet_bytes=offset,first_header=headers[0].hex(),last_header=headers[-1].hex(),
                  headers_sha256=hashlib.sha256(b''.join(headers)).hexdigest(),
                  scope='Complete bounded header-argument sequence; not committed encoded payload, intended sample count or audio completion')
    if reference is not None:
        if complete_reference_prefix and len(reference) > offset:
            raise ValueError('observed prefix extends beyond encoder publications')
        report['reference'] = dict(bytes=len(reference),matched_headers=matched,
                                   sha256=hashlib.sha256(reference).hexdigest(),
                                   declared_tail_shortfall=max(0,offset-len(reference)),
                                   trailing_bytes=max(0,len(reference)-offset),payload_validated=False)
        if complete_reference_prefix:
            report['reference']['unmatched_publications'] = len(frames)-matched
            report['reference']['comparison'] = 'Complete packet prefix only; not whole recording'
    return report


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
OUTPUT_PROCESSING_BASE = 0x2A2C6
OUTPUT_PROCESSING_WITNESS = (
    0x771a,0x003f,0x7710,0x0001,0xf072,0xa2e9,0xe900,0x941e,
    0x3087,0x8c83,0x2892,0xa78d,0xb081,0xa701,0xf45f,0xf75f,
    0xa743,0xb00f,0xa7e4,0xa743,0xb003,0xa724,0xf610,0xf442,
    0xf584,0x8184,0x8385,0xa583,0xf604,0x2584,0xf614,0x6f87,
    0x0c0f,0x2187,0xf603,0x8297,0xf4e4,
)
OUTPUT_SCALE_WITNESSES = (
    (0x29fa6,(0xf6b9,0xf7b8,0xf6b6,0x491d,0xf340,0x0021,0xf495,0xf495,0x891d)),
    (0x29fc6,(0x771a,0x003f)),
    (0x29fea,(0xf072,0x9fee,0xa4b0,0x6f91,0x0c64,0x6c8f,0x9fc6)),
)


def decode_output_scale(words):
    for base,witness in OUTPUT_SCALE_WITNESSES:
        if any(words.get(base+i) != value for i,value in enumerate(witness)):
            raise ValueError('AA99 signed output-scaling setup/kernel changed or absent')
    return dict(segment='aa99',entry='029fa2',kernel='029fea..029fee',
                pinned_words=sum(len(witness) for _,witness in OUTPUT_SCALE_WITNESSES),
                iterations_per_block=64,sample='MPY *AR5+,*AR2,A',
                output='STH A,4,*AR1+',FRCT=0,SXM=1,SST=1,
                reference='clamp_s16((signed16(sample)*signed16(coefficient)) >> 12)',
                scope='Pinned mode setup and kernel; coefficient-update policy and all callers not decoded')


def reference_output_scale(sample,coefficient):
    if not 0 <= sample <= 0xffff or not 0 <= coefficient <= 0xffff:
        raise ValueError('output scale operands must be 16-bit bus words')
    sample = sample-0x10000 if sample & 0x8000 else sample
    coefficient = coefficient-0x10000 if coefficient & 0x8000 else coefficient
    return max(-32768,min(32767,(sample*coefficient) >> 12)) & 0xffff


def decode_output_processing(words):
    for offset, expected in enumerate(OUTPUT_PROCESSING_WITNESS):
        if words.get(OUTPUT_PROCESSING_BASE+offset) != expected:
            raise ValueError('AA99 output-processing kernel changed or absent')
    for site in FILTER_CALL_SITES:
        if (words.get(site),words.get(site+1)) != (0xfb82,0xa2c6):
            raise ValueError('AA99 output-processing parent call changed or absent')
    for site in FILTER_CALL_SITES[1:]:
        if (words.get(site+2),words.get(site+3)) != (0x6def,0xffc0):
            raise ValueError('AA99 output-processing sample rewind changed or absent')
    return dict(segment='aa99',entry='02a2c6',return_instruction='02a2ea',
                pinned_words=len(OUTPUT_PROCESSING_WITNESS)+18,
                block_repeat_end='02a2e9',repeat_counter=63,iterations=64,
                sample_read='02a2ce LD *AR7,T',sample_write='02a2e9 STH A,*AR7+',
                passes_per_parent=5,between_pass_sample_rewind=-64,
                arithmetic='Fixed-point multiply/accumulate with persistent state writes through AR3/AR4/AR5',
                scope='Pinned kernel, loop and parent call geometry; not independent arithmetic, '
                      'coefficient meaning, runtime pass ownership or analog output')
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

DECODER_DISPATCH_WITNESSES = (
    (0xC6A0, (0x0002,0x3D5D)),
    (0x29879, (0xE90D,0xF882,0x98B8)),
    (0x298B8, (0x8812,0xF845,0x98C4,0xF741,0x0182,0x8912,0xF495,
               0xF495,0x5782,0xF84D,0x98C4,0xF5E6,0xF020,0xFFFF,0xF4E4)),
    (0x2936D, (0x10F8,0x1310,0xF845,0x9375,0x7711,0,0xF073,0x9377,0x7711,0x0400)),
    (0x293A2, (0x10F8,0x15E9,0xF982,0x96AC,0x10F8,0x15E9,0xF982,0x973C,0x8812,
               0xF495,0xE710,0xF5AA,0xF830,0x940B,0x7211,0x15D9,0x4811,0x1AE6,0x0016,0xF845,0x940B)),
    (0x293C6, (0x10E6,0x0012,0x8001,0x7000,0x15E9,0x10F8,0x15EB,0xF982,0x9879,0x8811)),
    (0x23D5D, (0x4A11,0x4A16,0x4A17,0xEEED,0x8816,0x1019,0x800C,0x1018,0x800D,
               0x10E6,2,0x8006,0x10E6,3,0x8005,0x100C,0x8004,0x100D,0xF982,0x973C)),
    (0x2973C, (0x4A11,0xEEFF,0x8811,0xF495,0xF495,0x10E1,1,0x0881,0xEE01,0x8A11,0xF4E4)),
    (0x23E2C, (0x10E6,1,0xF982,0x9F2B)),
    (0x29F2B, (0xE90C,0xF882,0x98B8)),
)

PLAYBACK_INFO_QUERY_WITNESSES = (
    (0x20973,(0xeefe,0x76f8,0x130f,0,0xf020,0x1307,0x8000,0x10f8,0x15ea,0xf982,0x9882,0xee02,0xf495,0xf4e4)),
    (0x29882,(0xe90b,0xf882,0x98b8)),
    (0x27823,(0x76f8,0x15eb,0xaaac,0x76f8,0x15ea,0x9cf8)),
    (0x2744b,(1,0x9cf8,0xc738)),
    (0xc738,(0xc738,)),
    (0xc744,(2,0x9c84)),
    (0xc74e,(0,0)),
    (0x298b8,(0x8812,0xf845,0x98c4,0xf741,0x0182,0x8912,0xf495,0xf495,0x5782,0xf84d,0x98c4,0xf5e6,0xf020,0xffff)),
)

DECODER_METHOD_PREFIX_BASE = 0xc686
DECODER_RETURN_BASE = 0x293cf
WORKER_RESERVATION_WITNESSES = (
    (0x293b0,(0x7211,0x15d9,0x4811,0x1ae6,0x16,0xf845,0x940b,
              0x10f8,0x15d8,0xf844,0x93be,0x76f8,0x15d8,1,0x6c81,0x93c6,
              0x4816,0xf982,0x44a,0x76f8,0x15d9,1)),
    (0x2044a,(0xf6be,0x8812,0xf495,0xf495,0xf274,0x182,0x6dea,0x16,0xf7be,0xf4e4)),
    (0x20182,(0x6b82,0xffff,0x108a,0xfa45,0x18f,0x718a,0x13,
              0x4a12,0xe734,0xf074,0x478,0x8a12,0xf495)),
    (0x29321,(0x7211,0x15d9)),
    (0x29334,(0x76f8,0x15d9,1)),
    (0x2935c,(0x76f8,0x15d9,0)),
    (0x293e1,(0x76f8,0x15d9,0)),
    (0x201c7,(0x7192,0x13,0x6d92,0x6b83,1)),
    (0x27058,(9,0xd797,0,0,0x0800,0xaa85,0xd7b0,0,0,0xd7ac,0x054f)),
    (SERVICE_GATE_BASE,SERVICE_GATE_WITNESS),
)


def decode_worker_reservation(words):
    for base,witness in WORKER_RESERVATION_WITNESSES:
        if any(words.get(base+offset)!=value for offset,value in enumerate(witness)):
            raise ValueError('worker reservation source changed or absent')
    hits = sorted(address for address,value in words.items() if value == 0x15d9)
    if hits != [0x204c6,0x29322,0x29335,0x2935d,0x293b1,0x293c4,0x293e2]:
        raise ValueError('worker15d9 literal census changed')
    counter_hits = sorted(address for address,value in words.items() if value == 0xd7ac)
    if counter_hits != [0x27061]:
        raise ValueError('worker counter pointer literal census changed')
    return dict(pinned_words=sum(len(w) for _,w in WORKER_RESERVATION_WITNESSES),
        mapped_words_scanned=len(words),literal_references=[f'{a:06x}' for a in hits],
        direct_literal_writes={'029334':1,'02935c':0,'0293c3':1,'0293e1':0},
        gate='15d9 OR context[22] must be nonzero',
        acquisition='If15d9 is zero after the gate, call02044a(context), then write15d9=1',
        helper=dict(wrapper='02044a..020453; delayed MAR places AR2 at context+22 before020182',
                    prefix='decrement word[AR2], load updated count while decrementing AR2',
                    callback='nonzero updated count calls020478 before the common02018f continuation',
                    scope='Pinned prefix only; callback/continuation effects and physical queue capacity remain open'),
        retention='A nonzero15d9 bypasses a fresh count acquisition; the decoded zero-result path clears it',
        increment='0201ca ADDM #1,*AR3; runtime target ownership must be established separately',
        increment_pointer=dict(load='0201c7 MVDK *AR2+,AR3;0201c9 MAR *AR2+',
            literal_references=[f'{a:06x}' for a in counter_hits],
            initialization_candidate=dict(source='027058',count=9,destination='d797',
                values=['0000','0000','0800','aa85','d7b0','0000','0000','d7ac','054f'],
                pointer_destination='d79e',pointer_source='027061'),
            scope='Single mapped literal in initialization data; runtime descriptor source address and initialization interpretation remain unproved'),
        scope='Local counted-context reservation contract and exact literal census; not all computed writers, full helper call effects, downstream ownership, physical units or EOF/drain completion')


CONTEXT_LANE_CALLS = {
    0x44a:(0x204a6,0x27a99,0x29332,0x293c1),
    0x454:(0x205f3,0x27a9c,0x2935a,0x293ed),
    0x461:(0x20508,0x27a9f,0x28fee),
    0x46b:(0x204bc,0x20581,0x2059c,0x28feb),
}
CONTEXT_LANE_WITNESSES = (
    WORKER_RESERVATION_WITNESSES[1],
    (0x20454,(0xf6be,0x8812,0xf495,0xf495,0xf495,0xf495,0x6dea,0x13,
              0x4492,0xf074,0x1b3,0xf7be,0xf4e4)),
    (0x20461,(0xf6be,0x8812,0xf495,0xf495,0xf274,0x182,0x6dea,6,0xf7be,0xf4e4)),
    (0x2046b,(0xf6be,0x8812,0xf495,0xf495,0xf495,0xf495,0x4482,
              0xf274,0x1b3,0x6dea,4,0xf7be,0xf4e4)),
    (0x201b3,(0xf7b8,0x718a,0x13,0x6d8a,0x1a92,0xf540,0x8093,0x8293,
              0x1283,0x809a,0x6dea,3,0x1292,0xf845,0x1c7,0x4a12,
              0x6d8a,0xf074,0x198,0x8a12,0x7192,0x13,0x6d92,0x6b83,1,0xe723)),
    (0x28fe6,(0x10f8,0xaaa2,0xf845,0x8fed,0x4811,0xf982,0x46b,
              0x4811,0xf982,0x461,0x76f8,0xaaa2,1,0x76f8,0xaaa4,0,
              0x76f8,0xaaa3,0)),
) + tuple((site,(0xf982,target)) for target,sites in CONTEXT_LANE_CALLS.items() for site in sites)


def decode_context_lanes(words):
    for base,witness in CONTEXT_LANE_WITNESSES:
        if any(words.get(base+i)!=value for i,value in enumerate(witness)):
            raise ValueError('context lane source changed or absent')
    calls = {target:tuple(sorted(address for address,opcode in words.items()
             if opcode in (0xf074,0xf274,0xf982,0xfb82) and words.get(address+1)==target))
             for target in CONTEXT_LANE_CALLS}
    if calls != CONTEXT_LANE_CALLS:
        raise ValueError('context lane immediate-call census changed')
    return dict(pinned_words=sum(len(witness) for _,witness in CONTEXT_LANE_WITNESSES),
        mapped_words_scanned=len(words),
        immediate_call_candidates={f'{target:06x}':[f'{site:06x}' for site in sites]
                                   for target,sites in calls.items()},
        lanes=[dict(count_offset=6,acquire_wrapper='020461',publish_wrapper='020454',
                    publish_entry_offset=20,link_offset=24),
               dict(count_offset=22,acquire_wrapper='02044a',publish_wrapper='02046b',
                    publish_entry_offset=4,link_offset=8)],
        helper='0201b3 reaches the0201c7 link load at entry_AR2+4; optional020198 call preserves AR2 on the stack',
        paired_caller=dict(entry='028fe6',condition='aaa2 != 0 calls02046b(AR1)',
            unconditional='020461(AR1)',after=['aaa2 <- 1','aaa4 <- 0','aaa3 <- 0'],
            scope='Local source branch, not runtime context ownership or flag lifecycle'),
        precondition='Local address derivation assumes called helpers return normally; callbacks and full memory effects are not modeled',
        scope='Two source-defined context lanes and exact syntactic immediate-call candidates; not all computed callers, physical rings, sample units, capacity, runtime wrapper ownership, scheduling or EOF policy')


def reference_context_lane(base,lane):
    """Sixteen-bit local address walk, conditional on normal helper return."""
    if type(base) is not int or not 0<=base<=0xffff or type(lane) is not int or lane not in (0,1):
        raise ValueError('context lane requires an unsigned base and lane0/1')
    entry = (base+(20 if lane==0 else 4)) & 0xffff
    ar2 = entry
    # MVDK postdecrement, MAR decrement, OR postincrement, STL preincrement,
    # MAR +3, then LDU postincrement before the common link load.
    for delta in (-1,-1,1,1,3,1):
        ar2 = (ar2+delta) & 0xffff
    return dict(count_address=(base+(6 if lane==0 else 22)) & 0xffff,
                publication_entry=entry,link_address=ar2)


EXCHANGE_PUBLICATION_WITNESSES = (
    (0x20545,(0x10f8,0xaa5a,0xf982,0x989d,0x10f8,0xaa5b,0xf982,0x989d,
        0x10f8,0xaa5a,0xf982,0x98a6,0xf495,0x4808,0xf845,0x586,
        0x10f8,0x1310,0xf845,0x580,0x10f8,0x15e9,0xf982,0x96ac,
        0x10f8,0x15e9,0x7716,0,0xf982,0x973c,0x8812,0xf495,0xf495,
        0x6c82,0x56a,0x7716,1,0x11e1,0x16,0xe800,0xf84c,0x570,0xe801,
        0x4916,0xf2a0,0x10f8,8,0xf845,0x580,0x10f8,0x15e7,0xf982,0x969e,
        0x76f8,0x1310,0,0x76f8,0x12fa,1,0x4811,0xf982,0x46b,0x76f8,0xaa55,0)),
    (0x2989d,(0xe90b,0xf882,0x98b8,0xe90c,0xf882,0x98b8,
               0xe90d,0xf882,0x98b8,0xe90e,0xf882,0x98b8)),
    (0xc70c,(2,0xa30b)),
    (0x2a30b,(0x4a11,0xeeff,0x8811,0xf495,0xf495,0x10e1,9,0xee01,0x8a11,0xf4e4)),
    (0x2973c,(0x4a11,0xeeff,0x8811,0xf495,0xf495,0x10e1,1,0x0881,0xee01,0x8a11,0xf4e4)),
)


def decode_exchange_publication(words):
    for base,witness in EXCHANGE_PUBLICATION_WITNESSES:
        if any(words.get(base+i)!=value for i,value in enumerate(witness)):
            raise ValueError('exchange publication source changed or absent')
    return dict(pinned_words=sum(len(witness) for _,witness in EXCHANGE_PUBLICATION_WITNESSES),
        entry='020545',process='method11 on aa5a and aa5b, then method14 on aa5a',
        publish='nonzero method14 low-word result reaches020581 ->02046b(AR1), then aa55=0; zero skips to020586',
        selected_table_condition=dict(table='c6f0',method14_pointer='c70c ->02a30b',
            getter='object[9], the source-exhausted field under the copy-adapter contract',
            scope='Conditional table interpretation; late aa5a runtime binding is not proved'),
        flag1310='zero bypasses the refresh/reset subsection, not publication',
        nonzero_flag='refresh15e9 through0296ac, query end-cursor via02973c; if that result is zero OR AR1[22] is zero, call02969e(15e7), clear1310 and set12fa=1',
        scope='Local branch and explicit stores conditional on normal helper returns; not helper memory effects, runtime branch selection, EOF flag semantics, physical units or drain completion')


def reference_exchange_publication(status,flag,remaining,count):
    """Explicit branch/store selection from the observed scalar operands only."""
    if any(type(value) is not int or not 0<=value<=0xffff for value in (status,flag,remaining,count)):
        raise ValueError('exchange publication requires unsigned words')
    publish = status!=0
    refresh = publish and flag!=0
    reset = refresh and (remaining==0 or count==0)
    return dict(publish=publish,refresh_input=refresh,reset_call=reset,
                clear_1310=reset,set_12fa=reset,clear_aa55=publish)


def reference_worker_reservation(held,count):
    """Gate and immediate decrement only, before unclosed callback effects."""
    if type(held) is not int or held not in (0,1) or type(count) is not int or not 0 <= count <= 0xffff:
        raise ValueError('reservation model requires literal flag0/1 and unsigned count')
    eligible = bool(held or count)
    acquire = eligible and not held
    return dict(eligible=eligible,acquire=bool(acquire),
                count_after_decrement=count-int(acquire),
                flag_after_acquisition=1 if acquire else held)


DECODER_RETURN_WITNESS = (
    0x8811,0x71e6,0x12,0xba9c,0xf020,0x15ee,0x8000,0x10f8,0x15eb,0xf982,0x9873,
    0x10f8,0x15f0,0x6ff8,0x15f0,0xc81,0x6c81,0x93f1,0x76f8,0x15d9,0,
    0x76f8,0x15f1,1,0x10f8,0x15ef,0x6fe6,0x13,0xc81,0x4816,0xf982,0x454,
    0xf073,0x93fe,0x6ce1,0xfffe,0x93fe,0x10f8,0x15ea,0xf982,0x9885,
    0x10f8,0x15eb,0xf982,0x9876,0xf982,0x61e,0x10f8,0x15e9,0xf982,0x96ac,
    0xf073,0x940b)
DECODER_RETURN_WRAPPERS = ((0x29873,(0xe909,0xf882,0x98b8)),
                           (0x29876,(0xe90b,0xf882,0x98b8)),
                           (0x29885,(0xe90c,0xf882,0x98b8)))


def decode_decoder_return_dispatch(words):
    for base,witness in ((DECODER_RETURN_BASE,DECODER_RETURN_WITNESS),
                         *DECODER_RETURN_WRAPPERS):
        if any(words.get(base+offset)!=value for offset,value in enumerate(witness)):
            raise ValueError('decoder return dispatcher or wrapper changed')
    return dict(entry='0293cf',last_word='029403',pinned_words=62,
        retained_return='AR1 <- A; selected metadata method9 preserves AR1 under decoder_info_helpers ABI',
        common_before_branch=['method9 on object15eb with record15ee', '15f0 <- low16(15f0 << 1)'],
        zero=dict(branch='0293df falls through',writes=['15d9 <- 0','15f1 <- 1',
                  'context[19] <- low16(record15ef << 1)'],call='020454',then='0293fe'),
        two=dict(branch='0293f1 tests low16(AR1-2) == 0',
                 calls=['method12 on object15ea','method11 on object15eb','02061e']),
        other=dict(branch='0293f1 tests low16(AR1-2) != 0',then='0293fe',
                   includes='ffff; no zero-result stores or result2 control calls'),
        common_tail='refresh input object15e9 through0296ac, then branch02940b',
        scope='Static local return routing with retained-AR1 ABI; not native worker invocation, all callee effects, retry scheduling, EOF/drain policy or complete playback')


def reference_decoder_return_route(result):
    if type(result) is not int or not 0 <= result <= 0xffff:
        raise ValueError('decoder result must be an unsigned word')
    return 'result_zero_group' if result == 0 else 'result_two_group' if result == 2 else 'refresh_only'


DECODER_METHOD_PREFIX = (
    0xc686,0,0,0,0,0,0,0,0,0,0,0,2,0x3d27,0,0,0,0,
    2,0x3e64,2,0x3ef8,2,0x3d41,2,0x3d5a,2,0x3d5d)
DECODER_POST_FRAME_BASE = 0x23d5a
DECODER_POST_FRAME = (0xf495,0xf495,0xf4e4)
DECODER_INFO_PREFIX_BASE = 0x23e64
DECODER_INFO_PREFIX = (0x4a11,0xeefb,0xf495,0x7108,0x0014,0x8811,
                      0x76e4,1,0x0400,0x10e1,1,0x80e4,10)
DECODER_INFO_RESTORE_BASE = 0x23ef8
DECODER_INFO_RESTORE = (0x4a11,0xeeff,0xf495,0x7104,0x0012,0x8811,
                       0x10e2,10,0x80e1,1,0xee01,0x8a11,0xf4e4)
DECODER_INFO_BODY = DECODER_INFO_PREFIX + (
    0x7712,0,0x10f8,0xba4f,0xf845,0x3e7d,0x10f8,0xba51,0xf844,0x3e7d,0x7712,1,
    0x70e4,3,0x0012,0x6c82,0x3e99,0xf6b8,0xf495,0xf020,0xac44,0x4ee4,6,
    0xf062,1,0xf040,0xf400,0x4ee4,4,0x76e4,2,0x0485,0x76e4,8,2,
    0x76e4,9,0,0xf073,0x3ef5,0x62f8,0xa89e,6,0x8812,0xf495,0xf495,
    0x56e2,0xc152,0x4ee4,6,0x10f8,0xba4e,0xf845,0x3eb1,0x57f8,0xa874,
    0xf7b8,0xf640,0xf468,0xf478,0xf845,0x3eb1,0x4fe4,4,0x71e1,4,0x0012,
    0x6c82,0x3ebb,0x76e4,3,0,0xf073,0x3ee4,0xf7b8,0x10f8,0x0012,0x4e00,
    0x56e4,6,0xf980,0x2096,0xf7b8,0xf468,0xf478,0xf57b,0xf3f6,0xf3f0,0xf600,
    0xf47a,0x4e02,0x7712,0xc6a2,0x7713,0x000e,0x5602,0x5702,0x58e2,2,
    0x5582,0xf468,0xf768,0xf478,0xf778,0xf520,0xf84b,0x3ee1,0x6dea,2,
    0x6ceb,0xffff,0x3ed0,0x5682,0x4ee4,4,0xe860,0x4e00,0x56e4,4,
    0xf982,0x9bdd,0xf000,1,0x80e4,2,0x70e4,8,0xa89c,0x10e1,8,
    0x80e4,9,0xee05,0x8a11,0xf4e4)
DECODER_INFO_LEVEL_BASE = 0xc6a2
DECODER_INFO_LEVELS = (8000,16000,24000,32000,40000,48000,56000,64000,
                       80000,128000,160000,192000,256000,320000,384000,0x7fffffff)
DECODER_INFO_HELPER_VECTOR = (0xf880,0x3518)
DECODER_INFO_HELPER_BODY = (0xf495,0x4a0b,0x4a0c,0x4a0d,0x4a0e,0xeefc,
    0xf495,0x4e00,0x310b,0x440a,0x3501,0x240b,0xf76f,0xf601,
    0xee04,0x8a0e,0x8a0d,0x8a0c,0x8a0b,0xf4e4)
DECODER_INFO_DIVIDE_BASE = 0x29bdd
DECODER_INFO_DIVIDE = (
    0xf495,0x4a0b,0x4a0c,0x4a0d,0xeefd,0xf7b8,0xf468,0xf478,0x4508,0xf1c0,
    0x8300,0xf485,0x5708,0xf785,0x4f08,0xf520,0xf84d,0x9bfd,0xf84e,0x9c01,
    0xf982,0x9c62,0x6100,0x8000,0xf820,0x9bf8,0xf484,0xee03,0x8a0d,0x8a0c,
    0x8a0b,0xf4e4,0xf273,0x9bf3,0xe801,0xf495,0xf273,0x9bf8,0xf420,0xf495)
DECODER_INFO_DIVIDE_WORKER_BASE = 0x29c62
DECODER_INFO_DIVIDE_WORKER = (
    0xf495,0x4a11,0xeeff,0xf6b8,0xf273,0x9c6b,0x7711,0x001f,0x6d89,
    0xf0e1,0xf808,0x9c6a,0xe901,0x550c,0xf84b,0x9c76,0xf273,0x9c77,
    0xf040,1,0x530c,0x6c89,0x9c7c,0xee01,0x8a11,0xf4e4,0xf3e1,0xf0e1,
    0xf808,0x9c6f,0xf273,0x9c6f,0xf340,1)


def decode_decoder_info_helpers(words,bootstrap):
    body=decode_decoder_info_body(words,bootstrap)
    for base,witness in ((DECODER_INFO_DIVIDE_BASE,DECODER_INFO_DIVIDE),
                         (DECODER_INFO_DIVIDE_WORKER_BASE,DECODER_INFO_DIVIDE_WORKER)):
        if any(words.get(base+offset)!=value for offset,value in enumerate(witness)):
            raise ValueError('decoder info division wrapper/worker changed')
    return dict(method=9,pinned_words=body['pinned_words']+74,
        preconditions=['CPL=1 stack-relative direct operands',
                       'valid disjoint caller stack and metadata record in ordinary RAM'],
        helper3518=dict(entry='003518',last_word='00352b',words=20,
            operations=['preserve four MMRs; FRAME -4; save A double word to stack',
                        'MPYA/MACA/MPYU cross products; combine through B shift and ADD',
                        'FRAME +4; restore MMRs; FRET'],
            memory='caller/local stack and MMRs only; no further calls'),
        helper29bdd=dict(entry='029bdd',last_word='029c04',words=40,
            operations=['normalize incoming A to signed32 and retain operand-sign XOR',
                        'ABS both arguments; write absolute divisor to caller stack',
                        'equal-operand result1; larger-divisor result0; otherwise call029c62',
                        'conditionally negate quotient; restore local frame/MMRs; FRET'],
            only_call='029bf1 -> 029c62'),
        worker29c62=dict(entry='029c62',last_word='029c83',words=34,
            operations=['preserve AR1; clear SXM; initialize loop counter31 in delayed branch',
                        'shift A until carry, decrementing AR1',
                        'subtract/add divisor from caller stack and shift quotient/remainder',
                        'restore frame/AR1; FRET'],
            memory='stack-relative DADD/DSUB at offset12; no calls, ports or payload pointers'),
        conclusion='Under the declared ABI, method9 and its closed local call tree update metadata/stack/MMRs only; no input-descriptor advancement, decoded-sample production or deferred-output drain',
        scope='Static control/memory-access closure, not native method9 invocation, exact arithmetic over all operands/statuses, physical timing or closure over every other EOF control')


def decode_decoder_info_body(words,bootstrap):
    levels=tuple(part for value in DECODER_INFO_LEVELS for part in (value>>16,value&0xffff))
    for image,base,witness in (
            (words,DECODER_INFO_PREFIX_BASE,DECODER_INFO_BODY),
            (words,DECODER_INFO_LEVEL_BASE,levels),
            (bootstrap,0x2096,DECODER_INFO_HELPER_VECTOR),
            (bootstrap,0x3518,DECODER_INFO_HELPER_BODY)):
        if any(image.get(base+offset)!=value for offset,value in enumerate(witness)):
            raise ValueError('decoder info body, numeric table or bootstrap helper changed')
    return dict(method=9,entry='023e64',last_word='023ef7',body_words=len(DECODER_INFO_BODY),
        pinned_words=len(DECODER_INFO_BODY)+len(levels)+2+len(DECODER_INFO_HELPER_BODY),
        local_record_writes=[1,2,3,4,5,6,7,8,9,10],
        local_object_reads=[1,4,8],
        fallback=dict(condition='BA4F == 0 or BA51 != 0',
                      record_fields={'1':0x400,'2':0x485,'3':0,'4..5':128000,
                                     '6..7':44100,'8':2,'9':0,'10':'object[1]'}),
        active=dict(record3='BA4F != 0 and BA51 == 0; later cleared if object[4] == 0',
                    record6='MPY A89E,#6 -> AR2; DLD *AR2(C152); offset depends on incoming multiplier status (FRCT included)',
                    optional_record4='A874 double word when BA4E and its signed low32 projection are nonzero',
                    numeric_table=dict(base='00c6a2',values=list(DECODER_INFO_LEVELS),
                                       loop='AR2 starts C6A2, AR3 starts14; compare adjacent double words, advance by2'),
                    tail='record[2] <- helper029bdd(record[4..5],96)+1; record[8] <- A89C; record[9] <- object[8]'),
        calls=[dict(site='023ec1',target='002096',source='AA55 trampoline2096 -> 003518',
                    witness_words=22),dict(site='023ee8',target='029bdd',source='AA99')],
        scope='Complete local body/table and first helper source identity; metadata-oriented local flow, not native execution, helper arithmetic closure, whole-call write closure or absence of all EOF/drain controls')


def decode_decoder_method_prefix(words):
    for base,witness in ((DECODER_METHOD_PREFIX_BASE,DECODER_METHOD_PREFIX),
                         (DECODER_POST_FRAME_BASE,DECODER_POST_FRAME),
                         (DECODER_INFO_PREFIX_BASE,DECODER_INFO_PREFIX),
                         (DECODER_INFO_RESTORE_BASE,DECODER_INFO_RESTORE)):
        if any(words.get(base+offset)!=value for offset,value in enumerate(witness)):
            raise ValueError('decoder method prefix or post-frame no-op changed')
    methods={str(index):f'{words[DECODER_METHOD_PREFIX_BASE+2*index]<<16 | words[DECODER_METHOD_PREFIX_BASE+2*index+1]:06x}'
             for index in range(1,14)}
    return dict(table='00c686',pinned_words=sum(map(len,(DECODER_METHOD_PREFIX,DECODER_POST_FRAME,
                                                     DECODER_INFO_PREFIX,DECODER_INFO_RESTORE))),
                methods=methods,post_frame=dict(method=12,entry='023d5a',
                    instructions=['NOP','NOP','FRET'],
                    data_effects='return-stack consumption only; no payload/object access, sample production or buffer advancement'),
                info_prefix=dict(method=9,entry='023e64',object='A -> AR1',record='caller SP[0] -> AR4',
                                 operations='record[1] <- 0400; record[10] <- object[1]',
                                 scope='First13-word sub-contract; complete local body is decoder_info_body'),
                info_restore=dict(method=10,entry='023ef8',object='A -> AR1',record='caller SP[0] -> AR2',
                                  operations='object[1] <- record[10]',
                                  scope='Complete13-word body; no call, sample loop or EOF predicate'),
                scope='Original table prefix through method13, complete methods10/12 and method9 prefix; not a proven table bound, all other method bodies or absence of all EOF/drain controls')

PLAYBACK_BUFFER_REFRESH_BASE = 0x296ac
PLAYBACK_BUFFER_REFRESH_WITNESS = (
    0x4a11,0x4a16,0x4a17,0xeefd,0x8816,0xf495,0xf495,0x71e6,
    0x0008,0x0017,0x71e6,0x0009,0x0011,0x4811,0xf845,0x96e1,
    0x71e1,0x0001,0x0011,0x71e6,0x0005,0x0013,0xe710,0xf6ab,
    0xf820,0x96e1,0x4911,0x4813,0xf620,0x8002,0x4913,0x10e6,
    0x0006,0x71e6,0x0005,0x0012,0xf620,0x7000,0x0012,0x8001,
    0xfb82,0x99cc,0xf495,0x4811,0x1102,0x10e6,0x0006,0xf620,
    0x80e6,0x0006,0x70e6,0x0005,0x0011,0x4817,0xf845,0x9737,
    0x71e6,0x0001,0x0012,0xf6b8,0x4812,0x08e7,0x0005,0xf844,
    0x96ff,0x7186,0x0010,0xf7aa,0xf830,0x96f6,0x56e7,0x000a,
    0x4ee6,0x000a,0x71e7,0x0006,0x0012,0x70e7,0x0005,0x0012,
    0x70e6,0x0001,0x0012,0x71e6,0x0007,0x0011,0x71e6,0x0006,
    0x0014,0x7186,0x0013,0xe737,0x4914,0x4817,0xf620,0x8817,
    0xf7b8,0x10f8,0x0017,0x08f8,0x0011,0xf847,0x9737,0x4911,
    0x4817,0xf620,0x8817,0x4814,0x00f8,0x0011,0x8811,0x4913,
    0x4812,0x7186,0x0014,0xf620,0x8812,0x7000,0x0014,0x7001,
    0x0012,0xfb82,0x99cc,0xf495,0x4811,0xf495,0x70e6,0x0003,
    0x0011,0x7086,0x0011,0x4917,0x10e6,0x0001,0xf620,0x80e6,
    0x0001,0x80e6,0x0004,0xee03,0x8a17,0x8a16,0x8a11,0xf4e4,
)


def decode_playback_buffer_refresh(words):
    for offset,value in enumerate(PLAYBACK_BUFFER_REFRESH_WITNESS):
        if words.get(PLAYBACK_BUFFER_REFRESH_BASE+offset) != value:
            raise ValueError('playback buffer refresh witness changed or absent')
    copy = PLAYBACK_PENDING_CONTROL[0x299cc]
    if any(words.get(0x299cc+i) != value for i,value in enumerate(copy)):
        raise ValueError('playback refresh copy-helper ABI changed or absent')
    return dict(entry='0296ac',last_word='02973b',pinned_words=144+len(copy),
                receiver='R in A -> AR6; U=R[8], D=R[9]',
                copy_abi='0299cc: destination A, source caller SP[0], count caller SP[1]',
                downstream=dict(guard='D != 0 and unsigned R[5] > D[1]',
                                copy='R[5] -> D[1], count R[6]-R[5]',
                                updates='R[6] -= R[5]-D[1]; R[5] = D[1]'),
                upstream=dict(guard='U != 0',
                              synchronization='if R[1] == U[5]: optionally copy U[10..11], then R[1] = U[6] and U[5] = U[6]',
                              compact_guard='SXM=1: sign_extend16((R[0]-R[6]) & ffff) - sign_extend16(R[7]) > 0',
                              copy='R[0] -> R[6]+R[7], count updated R[1]-R[0]',
                              updates='R[0] and R[3] become R[6]+R[7]; subtract excess from R[1] and copy it to R[4]'),
                payload_operations='Two conditional memmove calls at 0296d4 and 029725; no payload zero-fill',
                scope='Complete local refresh and copy-helper source; not live descriptor ownership, all upstream writers, physical storage padding or EOF/drain policy')


def decode_playback_info_query(words):
    for base,witness in PLAYBACK_INFO_QUERY_WITNESSES:
        for offset,value in enumerate(witness):
            if words.get(base+offset) != value:
                raise ValueError(f'playback information query witness changed at {base+offset:06x}')
    refs = sorted(address for address,value in words.items() if value == 0x1310)
    return dict(pinned_words=sum(len(w) for _,w in PLAYBACK_INFO_QUERY_WITNESSES),
                query='020973 clears 130f, passes destination 1307 to method 11 through 15ea',
                selected_object='027823 sets 15eb=aaac and 15ea=9cf8',
                initial_table='original C-init 02744b sets object 9cf8 table to c738',
                method_11='c738 + 2*11 = c74e: zero far pointer',
                missing_method='0298b8 returns ffff for zero far pointer; it does not call a metadata writer',
                flag_literals=[f'{address:06x}' for address in refs],
                scope='Selected initial table and local query/dispatcher spans; not table immutability, indirect writer closure, metadata layout or an EOF operation')


def decode_decoder_dispatch(words):
    for base,witness in DECODER_DISPATCH_WITNESSES:
        if any(words.get(base+offset) != value for offset,value in enumerate(witness)):
            raise ValueError(f'decoder dispatch witness changed or absent at {base:06x}')
    adjacent = [address for address in words if address+1 in words]
    far_refs = sorted(address for address in adjacent if (words[address],words[address+1]) == (2,0x3d5d))
    direct = sorted(address for address in adjacent if words[address] in (0xf982,0xfb82,0xf882) and words[address+1] == 0x9879)
    return {'segment':'aa99','pinned_words':sum(len(witness) for _,witness in DECODER_DISPATCH_WITNESSES),
        'method':13,'selected_table_pointer':'00c6a0','body':'023d5d',
        'wrapper':'029879','common_dispatch':'0298b8','selected_callsite':'0293cd',
        'receiver_reference':'15eb','input_object_reference':'15e9',
        'availability_helper':{'entry':'02973c','operation':'low16(object[1] - object[0])'},
        'local_gate':{'compare':'0293ad','branch':'0293ae','skip_target':'02940b',
            'condition':'unsigned AR2 < AR0; CMPR CC=1,AR2',
            'threshold':'1310 != 0 selects zero; otherwise 0400',
            'activity':'15d9 OR context[0x16] must be nonzero at 0293b5'},
        'post_frame':{'call':'023e2e','wrapper':'029f2b','virtual_method':12},
        'census':{'mapped_words':len(words),'adjacent_pairs':len(adjacent),
            'encoded_far_reference_candidates':[f'{address:06x}' for address in far_refs],
            'recognized_direct_wrapper_candidates':[f'{address:06x}' for address in direct],
            'scope':'Contiguous word pairs and FCALL/FCALLD/FB forms only; not code/data classification or complete indirect-call closure'},
        'scope':'Pinned local software dispatch gates; no physical counter units, runtime operand ownership or session-termination cause'}


def reference_decoder_local_gate(cursor, limit, threshold_flag, activity, context_activity):
    values = (cursor,limit,threshold_flag,activity,context_activity)
    if any(not isinstance(value,int) or not 0 <= value <= 0xffff for value in values):
        raise ValueError('decoder gate requires five unsigned words')
    available = (limit-cursor)&0xffff
    threshold = 0 if threshold_flag else 0x400
    route = ('insufficient_input' if available < threshold else
             'inactive' if not (activity|context_activity) else 'call_method_13')
    return {'available':available,'threshold':threshold,'route':route,
            'scope':'Local gate after buffer refresh only; counter units and earlier lifecycle branches are not inferred'}

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


MONO_OUTPUT_BASE = 0x23df6
MONO_OUTPUT_WITNESS = (0x60f8, 0xa89c, 1, 0xf820, 0x3dfd, 0x1006, 0x8005)


def decode_mono_output(words):
    for offset, expected in enumerate(MONO_OUTPUT_WITNESS):
        if words.get(MONO_OUTPUT_BASE + offset) != expected:
            raise ValueError('original mono output-selection witness changed')
    return dict(channel_count='a89c', mono_value=1, source_stack_word=6,
                duplicated_stack_word=5, pinned_words=len(MONO_OUTPUT_WITNESS),
                scope='Mono aliases the second conversion source to the first after the return-pointer observation; not two independently decoded channels')


DECODER_FRAMING_BASE = 0x23D73
DECODER_INPUT_DESCRIPTOR_WITNESSES = (
    (0x2973c,(0x4a11,0xeeff,0x8811,0xf495,0xf495,0x10e1,1,0x0881,0xee01,0x8a11,0xf4e4)),
    (0x23d6e,(0x100d,0xf982,0x973c,0x80f8,0xb330)),
    (0x23def,(0x10f8,0xb331,0x8010)),
    (0x23e59,(0x710d,0x0011,0x1010,0x0081,0x8081,0x100f,0xee13,0x8a17,0x8a16,0x8a11,0xf4e4)),
)
DECODER_INPUT_RESERVE_BASE = 0x24425
DECODER_INPUT_RESERVE_WITNESS = (
    0x6ff8,0xb331,0x0d00,0x81f8,0xb331,0x7210,0xb330,0x8912,
    0x6de8,0xfff8,0xf6aa,0xf820,0x4438,0xf520,0x81f8,0xb331,
    0xe800,0xf073,0x443c)

POCP_COUNT_WITNESSES = {
    0x2adb2: (0xeefc,0xf495,0x8002,0xe810,0x4e00,0x1002,0xf982,0xad94,
              0xee04,0xe810,0xf495,0xf495,0xf4e4),
    0x2aea5: (0x715d,0x0011,0x7710,0x0200,0x7181,0x0011,0xf5a9,0xf820,
              0xaeba,0x715d,0x0011,0x7681,0x0200,0xf7b8,0xf495,0xf062,
              0x8004,0xf273,0xaf83,0xf040,0x0006),
    0x2aeff: (0xe8ff,0x715d,0x0011,0x1381,0xf280,0x715c,0x0011,0x80e1,
              8,0x715d,0x0011,0x1281,0xf1f8,0xe8ff,0xf280,0x715c,
              0x0011,0x80e1,9,0x715d,0x0011,0x1281,0xf1f0,0xe8ff,
              0xf280,0x715c,0x0011,0x80e1,10,0x715d,0x0011,0x1281,
              0xf1f0,0xf3f8,0xe8ff,0x715c,0x0011,0xf280,0x80e1,11),
    0x2af27: (0x105c,0xfb82,0xadb2,0xf000,0x0010,0xf540,0x4e04,0xe8ff,
              0xf280,0x715c,0x0011,0xf6b8,0x80e1,12,0x5604,0xf1f8,
              0xe8ff,0xf280,0x715c,0x0011,0x80e1,13,0x5604,0xf1f0,
              0xe8ff,0x715c,0x0011,0xf280,0x80e1,14,0x5604,0xf1f0,
              0xf3f8,0xe8ff,0xf280,0xf7b8,0x715c,0x0011,0x80e1,15),
}


def pocp_count_words(marker):
    if marker not in ('aa22','aa99'):
        raise ValueError('unsupported POCP writer overlay')
    delta = 0xa3c if marker == 'aa22' else 0
    expected = {}
    for base,witness in POCP_COUNT_WITNESSES.items():
        for offset,value in enumerate(witness):
            # These are the two absolute branch targets in the bounded prefix.
            if base == 0x2aea5 and offset in (8,18):
                value -= delta
            if (base == 0x2adb2 and offset == 7) or (base == 0x2af27 and offset == 2):
                value -= delta
            expected[base-delta+offset] = value
    return expected


RECORDER_STOP_FRONTEND = {
    0x285dc: (0x76f8,0x3750,0,0x60f8,0x3762,8,0xf820,0x85e7,0xe80a,0xf073,
              0x85e8,0xe809,0x80f8,0x3762,0x7105,0x3763,0xf983,0x84fc,0xf983,0x86ec),
    0x384fc: (0xeefe,0x76f8,0xedc3,0,0xe800,0x7600,0,0x7601,1,0xf983,0x92ae,0xee02,0xf495,0xf4e4),
    0x386ec: (0xeefe,0xf020,0x0500,0x7600,0x0080,0xf983,0x88ad,0x7600,9,0xf020,0x0a00,
              0xf983,0x88ad,0xee02,0xf495,0xf4e4),
    0x388ad: (0x4a11,0xeeff,0xf495,0x1104,0x7711,0x0048,0x7681,1,0x61e1,1,2,
              0xf820,0x88b5,0xf1a0,0x7712,0x0043,0x8182,0xf020,0x0218,0xf983,0x9333,
              0x61e1,1,2,0xf820,0x88c2,0xee01,0x8a11,0xf4e4),
    0x392ae: (0x4a11,0xeefd,0xf495,0x1106,0x7107,0x0012,0x7711,0xfd85,0x7681,3,
              0x80e1,1,0x81e1,2,0x70e1,3,0x0012,0x4811,0x7600,4,0xf983,0x95e6,
              0xee03,0x8a11,0xf4e4),
    0x39443: (0x7711,0xfd85,0x6081,0x8009,0xf830,0x944c,0x4811,0xf073,0x947f,
              0x60f8,0xfd86,1,0xf820,0x9476,0x76f8,0x3750,0,0xf7b8,0x7211,0x3762,
              0x10f8,0x0011,0xf010,8,0xf845,0x9465,0x6ce1,0xfff9,0x9468,0x76f8,
              0x3762,0x0010,0xf073,0x9468,0x76f8,0x3762,0x0011,0x76f8,0x373e,2,
              0xf983,0x86ec,0x7600,1,0xf020,0x3aea,0xf980,0x2082,0xe800,0xf980,0x2080),
}


def decode_recorder_stop_frontend(words):
    for base, witness in RECORDER_STOP_FRONTEND.items():
        if any(words.get(base+offset) != value for offset, value in enumerate(witness)):
            raise ValueError('recorder stop frontend source changed or absent')
    return dict(pinned_words=sum(map(len, RECORDER_STOP_FRONTEND.values())),
        control_case='0285dc: clear 3750; continuation 0008 becomes 000a, otherwise 0009; call 0384fc then 0386ec',
        processing_request=dict(builder='0392ae', words=[3,0,0,1], buffer='fd85..fd88',
                                send='0395e6 with four words; downstream processing not closed'),
        codec_control=dict(helper='0386ec', words=['0580','0a09'],
            transport='0388ad ORs register/data arguments, polls 0049 bit 1 and writes encoded word to 0043',
            meaning='AIC23-family headphone mute/zero-cross simultaneous update; DAC mute and ADC high-pass disable',
            scope='Control words and related B-revision register definitions; not analog behavior or a file flush'),
        completion_consumer=dict(case='039443', predicate='fd85 == 8009 and fd86 == 1',
            action='clear mode 3750; update continuation; state 373e <- 2; codec mute; API 2082 then loader 2080',
            scope='Selected consumer block only; producer and successful recording extent not established'),
        scope='Complete stop helper stubs plus selected dispatch/completion blocks; not closure over the processing worker or file-flush policy')


RECORDER_STOP_MAILBOX = {
    0x395ac: (0x8812,0xf495,0x7713,0x0080,0xec0f,0xe598,0xf495,0xf495,0xf4e4),
    0x395b5: (0x4a11,0xeeff,0xf495,0x7104,0x0014,0x1105,0x8813,0xf300,0x000f,
              0x8911,0xe80f,0x771a,0x000f,0xf072,0x95d7,0xf7b8,0x11f8,0x0008,
              0x09f8,0x0014,0xf84b,0x95ce,0xe900,0xf073,0x95d4,0x4913,0xf500,
              0x8912,0xf495,0xf495,0x1182,0x8181,0x6d89,0xf010,1,0xee01,0x8a11,0xf4e4),
    0x395e6: (0x4a11,0x4a16,0xeefe,0xf495,0x7106,0x0016,0x8811,0xf073,0x95f1,
              0xf982,0x1f2f,0xf982,0x1f24,0x61f8,0x0080,0x8000,0xf820,0x95ef,
              0xf020,0x0080,0x7000,0x0016,0x8001,0x4811,0xf983,0x95b5,0x56f8,
              0xefe6,0xf845,0x9605,0xf4e7,0xf982,0x1f2f,0xee02,0x8a16,0x8a11,0xf4e4),
    0x3962f: (0x4ef8,0xefe6,0xe801,0xf495,0xf495,0xf4e4),
    0x708a: (0xf062,3,0xf040,0x8c13,0xf983,0x962f,0xf495,0xf495,0xf4e4),
    0x2064: (0x4a1e,0xf495,0xf883,0x8ad2),
    0x38c13: (0x76f8,0x05cd,1,0xf7d9,0xf495,0xf495,0xf4e4),
    0x38b0d: (0x7102,0x0012,0x10f8,0x0012,0xf010,0x0012,0xf846,0x8b2c,
              0x10f8,0x0012,0xf010,0x0012,0xf845,0x8b74,0x7710,2,0xf4aa,0xf830,
              0x8bd8,0x7710,3,0xf4aa,0xf830,0x8bbf),
    0x38bbf: (0x76f8,0x05ea,0,0x76f8,0x05ce,0,0x1003,0xe900,0xf844,0x8bca,
              0xe901,0x81f8,0x05eb,0x10f8,0x05eb,0xf845,0x8bd3,0x76f8,0x05f3,1,
              0x76f8,0x05f0,0,0xf073,0x8be4),
    0x38be4: (0x6902,0x8000,0x4818,0x7000,0x0011,0xf000,2,0xf982,0x9736),
    0x38a66: (0x10f8,0x05f3,0xf844,0x8a9b),
    0x38a9b: (0x10f8,0x05f1,0xf844,0x8aa4,0x76f8,0x05f2,1,0xf073,0x8acf,
              0x7602,0x0016,0x7603,0,0x76f8,0x05f3,0,0x7711,2,0xf073,0x8ac6),
    0x389ef: (0x10f8,0x05f0,0xf844,0x8a09,0x10f8,0x05eb,0xf844,0x89fe,0xf7b8,
              0xf020,0x02f8,0x08f8,0x05ed,0xf842,0x8a09,0x76f8,0x05ed,0,0x76f8,
              0x05ee,0,0x76f8,0x05eb,0,0xf073,0x8a13),
    0x38f52: (0x10f8,0x05eb,0xf844,0x8ff7),
    0x38ff7: (0xe800,0xf983,0x89cf,0xf020,0x05e6,0xf983,0x9766,0x10f8,0x05f4,0xf983,0xe30c),
}


def decode_recorder_stop_mailbox(words):
    for base, witness in RECORDER_STOP_MAILBOX.items():
        if any(words.get(base+offset) != value for offset, value in enumerate(witness)):
            raise ValueError('recorder stop mailbox source changed or absent')
    return dict(pinned_words=sum(map(len, RECORDER_STOP_MAILBOX.values())),
        publication='0395e6 waits for 0080 bit15, writes a zero-extended 16-word record through 0395b5 in descending address order, then calls nonzero EFE6',
        initialization='AA88 00708a selects 038c13 through setter 03962f; this is an initialization selection, not lifetime pointer immutability',
        notification='038c13 sets 05cd=1 and INTR 25; vector 2064 branches to 038ad2',
        selector3='038bbf clears 05ea/05ce/05f0; stores (argument == 0) at 05eb; when 05eb!=0 sets 05f3=1',
        immediate_reply='038be4 ORs selector with 8000; selector 3 acknowledgement is 8003, distinct from asynchronous 8009',
        deferred='038a66 tests 05f3; selected 038a9b tail waits on 05f1 or prepares selector 16/argument 0 and clears 05f3',
        buffer_reset='When 05f0==0 and 05eb!=0, 0389ef..038a08 clears 05ed,05ee,05eb; buffer units and file-flush ownership unclassified',
        processing_gate='038f52 branches on 05eb to 038ff7: helper 0389cf(0), reset helper 039766(05e6), cleanup helper 03e30c(05f4)',
        scope='Selected original-source mailbox/stop paths; no live callback identity, final-flush policy, asynchronous completion producer or intended sample extent proof')


METADATA_EXTENT_WITNESSES = {
    'aa22': {
        0x28394: (0x7712,0x1784,0xec31,0x7692,0),
        0x28420: (0xe800,0x4e00,0x1031,0x8002,0x4811,0xf980,0x2092,0xf6b8,0xf495,
                  0x5632,0xf0ff,0x4e32,0xf0e0,0xf845,0x8445,0x7716,0,0x1031,0x7000,
                  0x0017,0x7601,1,0x8002,0x4811,0xf983,0xc2c2,0xf7b8,0x6d96,0x11f8,
                  0x0016,0xf6b8,0xf495,0x5632,0xf3e0,0xf520,0xf84b,0x8431,0x102e),
        0x28455: (0xe800,0x4e00,0x1031,0x8002,0x4811,0xf980,0x2092,0x7618,0x4944,
                  0x7619,0x3303,0x761a,0,0x761b,0,0x1030,0x801c,0x7600,5,0x4818,
                  0xf000,0x0018,0xf982,0x86db,0x4818,0xf000,0x0018,0x8000,0x1031,
                  0x8002,0x7601,5,0x4811,0xf983,0xc2c2,0x1031,0x8002,0x7000,0x0017,
                  0x7601,0x0032,0x4811,0xf983,0xc2c2,0xf073,0x8513),
        0x3c3f6: (0x4817,0xf0e1,0xf495,0x4808,0x50e1,0x0012,0x4ee1,0x0012,0xe800),
        0x3c421: (0x6b04,0x000e,0x1104,0xf200,1,0x8004,0x4816,0xf600,0x8812,
                  0x10e1,0x0013,0x8082,0xf6b8,0x56e1,0x0012,0xf1f0,0x4816,0x0004,
                  0x8812,0xf495,0xf495,0x8182),
        0x38eab: (0xf020,0x1444,0x8011,0x61f8,0x1451,0x0010,0xf830,0x8efb,0xf7b8,
                  0x10f8,0x0011,0xf010,1,0xf845,0x8ee6,0x6c81,0x8efb,0x100c,0x8000,
                  0x7002,0x0016,0x100b,0x7604,0,0x8001,0x100a,0x8003,0x1011,0xf982,0x8385),
    },
    'aa55': {
        0x33ea: (0x4a11,0x4a16,0xeefa,0x8811,0x560a,0x4e02,0x710c,0x0016,0x10e1,
                 0x0011,0x80e1,0x0016,0x80e1,0x0014,0x76e1,0x0015,0,0xf6b8),
    },
}


def decode_metadata_overwrite_extent(resident_words, bootstrap_words):
    for marker, words in (('aa22',resident_words),('aa55',bootstrap_words)):
        for base,witness in METADATA_EXTENT_WITNESSES[marker].items():
            if any(words.get(base+offset) != value for offset,value in enumerate(witness)):
                raise ValueError('metadata overwrite extent source changed or absent')
    return dict(pinned_words=sum(len(witness) for spans in METADATA_EXTENT_WITNESSES.values() for witness in spans.values()),
        record='038eab selects directory record 1444 before updater 028385; byte extent at record+12/+13',
        seeks='0033ea resets cluster/offset fields +14/+15/+16, not extent +12/+13',
        rewrite='028420 seeks zero then old_tag_bytes/2 single-word zero writes; 028455 seeks zero then writes five header words and fifty tag words',
        accounting='03c3f6 doubles processed AR7 word count and unconditionally DADDs it into record+12/+13; no position maximum in this tail',
        directory='03c421 serializes record+13 then high half of record+12/+13 into the on-disk directory buffer',
        scope='Selected source-pinned ID3 rewrite, seek-prefix and writer/accounting blocks; no claim that the extension is desirable, a hardware defect, or valid codec padding')


def reference_metadata_size_updates(file_bytes, old_tag_bytes):
    """Admitted even ID3 payload and nonoverflowing positive-file source model."""
    if (not isinstance(file_bytes,int) or not isinstance(old_tag_bytes,int) or
            not 50 <= old_tag_bytes <= 0x7ffe or old_tag_bytes % 2 or
            not old_tag_bytes+10 <= file_bytes <= 0x7fffffff-old_tag_bytes-110):
        raise ValueError('metadata size model requires an even admitted tag and nonoverflowing file extent')
    result, size = [], file_bytes
    for count in [1]*(old_tag_bytes//2)+[5,50]:
        size += 2*count
        result.append(dict(words=count,bytes=2*count,extent=size))
    return result


def decode_pocp_buffer_count(rel_words, resident_words):
    for marker,words in (('aa99',rel_words),('aa22',resident_words)):
        if any(words.get(address) != value for address,value in pocp_count_words(marker).items()):
            raise ValueError(f'{marker} POCP count writer changed or absent')
    return dict(pinned_words=228,
                source='pointer in caller argument at local SP[5d]; dereferenced 16-bit count',
                minimum='unsigned count < 0200: store required 0200 through that pointer and return error',
                serialization='mask and shift the count into output byte-valued words 8..11, least significant first',
                next_field='normal return from 02adb2 is constant 16; caller serializes it least significant first into output words 12..15',
                source_spans=dict(aa99=['02aea5..02aeb9','02aeff..02af26','02adb2..02adbe','02af27..02af4e'],
                                  aa22=['02a469..02a47d','02a4c3..02a4ea','02a376..02a382','02a4eb..02a512']),
                scope='Four source-pinned writer/helper spans in each overlay; not total track samples, custom-body interpretation or completed recording')


def decode_decoder_input_reserve(words):
    for offset,value in enumerate(DECODER_INPUT_RESERVE_WITNESS):
        if words.get(DECODER_INPUT_RESERVE_BASE+offset) != value:
            raise ValueError('decoder input reserve witness changed or absent')
    return dict(entry='024425',pinned_words=19,
                predicate='unsigned candidate index > ((b330 - 8) & ffff)',
                rejected='subtract the skipped-word count back out of b331, return zero',
                accepted='branch to 024438, consume residual bits through 02510c',
                scope='Local skip helper reserves eight words for extents >= 8; not a general EOF or short-buffer protocol')


def decode_decoder_input_descriptor(words):
    for base,witness in DECODER_INPUT_DESCRIPTOR_WITNESSES:
        for offset,value in enumerate(witness):
            if words.get(base+offset) != value:
                raise ValueError(f'decoder input descriptor witness changed at {base+offset:06x}')
    return dict(pinned_words=sum(len(w) for _,w in DECODER_INPUT_DESCRIPTOR_WITNESSES),
                descriptor='two word pointers: cursor, exclusive end',
                extent='02973c returns end minus cursor; wrapper stores at b330',
                advancement='023def snapshots fetched-word index b331; 023e59 adds it to descriptor cursor',
                ownership='caller supplies window; decoder owns reservoir and advances public cursor',
                scope='Local source spans plus isolated successful-call observations; not original storage refill lifecycle, short-input recovery or EOF')


DECODER_FRAMING_WITNESS = (
    0x710d,0x0011,0x7181,0xb332,0x76f8,0xb331,0x0000,
    0x10f8,0xba50,0xf844,0x3da0,0x76f8,0xba4f,0x0001,
    0x76f8,0xba4e,0x0000,0x7181,0x0011,0x6081,0x4144,
    0xf820,0x3da0,0x60e1,0x0001,0x4946,0xf820,0x3da0,
    0x76f8,0xba4e,0x0001,0x76f8,0xba4f,0x0000,
    0x10f8,0xba44,0xf844,0x3da0,0xf982,0x41f2,
    0xf982,0x542b,0x76f8,0xba44,0x0001)

DECODER_ALIGNMENT_BASE = 0x2542B
DECODER_ALIGNMENT_WITNESS = (
    0xeefe,0x10f8,0xa854,0xf7b8,0xf540,0x8000,0xe807,0x11f8,
    0x000b,0xf493,0xf77e,0xf773,0x0100,0xf280,0x1100,0xf520,
    0xf640,0xf982,0x510c,0xee02,0xf495,0xf4e4)

DECODER_RESET_BASE = 0x23D41
DECODER_RESET_WITNESS = (
    0x4a11,0xeeff,0x8811,0xf982,0x45d7,0xf982,0x56cf,
    0x76f8,0xba50,0,0x76f8,0xba44,0,0x76e1,4,0,
    0x76e1,5,0,0xe800,0x4ee1,6,0xee01,0x8a11,0xf4e4)
DECODER_BIT_RESET_BASE = 0x256CF
DECODER_BIT_RESET_WITNESS = (0xe800,0x4ef8,0xa858,0x76f8,0xa854,0,
                           0x76f8,0xa85a,0,0xf495,0xf495,0xf4e4)
DECODER_POINTER_RECORD_BASE = 0x273DB
DECODER_POINTER_RECORD_WITNESS = (
    2,0xb333,0xaaec,0xaaec,2,0xb335,0xac36,0xacc1,
    2,0xb337,0xad4c,0xae3e,2,0xb339,0xab64,0xabcd)
DECODER_CHANNEL_RESET_BASE = 0x245D7
DECODER_CHANNEL_RESET_WITNESS = (
    0x4a11,0xeeff,0x76f8,0xa8c7,0,0x76f8,0xa85e,1,
    0x76f8,0xa85f,0xffff,0x7711,0xa89c,0x76e1,1,1,
    0x76e1,2,3,0x76f8,0xb333,0xaaec,0x4811,0xf000,3,
    0x8811,0xf495,0x7712,0xb335,0x771a,1,0xf072,0x4600,
    0x7681,0,0x7192,0x13,0x76e3,3,0,0x6de9,6,
    0x7711,0xaf2f,0x771a,0x1ff,0xf072,0x460b,
    0x7699,0,0x76e1,0x200,0,0xe800,0xee01,0x8a11,0xf4e4)
DECODER_CONSTRUCTOR_BASE = 0x23D27
DECODER_CONSTRUCTOR_WITNESS = (
    0x4a11,0xeeff,0xf495,0x7104,0x12,0x7106,0x13,0x8811,
    0x10e3,1,0x80e1,1,0x10e2,9,0x80e1,2,0x10e2,14,
    0x80e1,3,0x4811,0xf982,0x3d41,0xee01,0x8a11,0xf4e4)
DECODER_CONFIGURATION_WITNESSES = (
    (0xd43b, (2,0,10,0,2,1,0xaaac,10,0,2,1,0xe800,10,0,2,1,0xf000)),
    (0x2786b, (0xf020,0xaaac,0x7600,0xd43d,0x7601,0,0x7602,0xd43b,0xf982,0x9870)),
    (0x29870, (0xe906,0xf882,0x98b8)),
)


CATALOGUE_MEDIA_WITNESSES = (
    (0xbff3, (0x52,0x45,0x4c,0,0x4c,0x53,0x45,0)),
    (0x39494, (0x1028,0x8000,0xf020,0xbff3,0xf982,0xe36f,
               0x10f8,8,0xf845,0x94a9,0x1028,0x8000,0xf020,0xbff7,
               0xf982,0xe36f,0x8812,0xf495,0xf495,0x6c82,0x9570)),
)


DECODER_SYNC_BASE = 0x24457
DECODER_SYNC_WITNESS = (0xe80f,0xf982,0x510c,0xf7b8,0x11f8,0x11,0xf300,0xf,
                        0x4f02,0xf468,0xf478,0xf010,0x7ffc,0xf845,0x4486)


DECODER_MAPPED_ALU_BASE = 0x2558F
DECODER_MAPPED_ALU_WITNESS = (0x6ff8,0x16,0x0d00,0x8912,
                            0x6ff8,0x19,0x0d00,0x8911,0x7314,0x0e)


def decode_decoder_mapped_alu(words):
    for offset, expected in enumerate(DECODER_MAPPED_ALU_WITNESS):
        if words.get(DECODER_MAPPED_ALU_BASE+offset) != expected:
            raise ValueError('AA99 mapped-register arithmetic witness changed or absent')
    return dict(entry='02558f', pinned_words=10,
                first='ADD *($0016),0,A,B; STLM B,AR2',
                second='ADD *($0019),0,A,B; STLM B,AR1',
                register_names={'0016':'AR6','0019':'BK'},
                scope='Original arithmetic operands and pointer publication; not admitted staged BK ownership or decoded audio')


def native_adts_sync_word(word):
    if not isinstance(word, int) or not 0 <= word <= 0xffff:
        raise ValueError('expected unsigned first ADTS word')
    return word >> 1 == 0x7ffc


def decode_decoder_sync(words):
    for offset, expected in enumerate(DECODER_SYNC_WITNESS):
        if words.get(DECODER_SYNC_BASE+offset) != expected:
            raise ValueError('AA99 ADTS sync predicate changed or absent')
    return dict(entry='024457', pinned_words=15, reader_width=15, expected='7ffc',
                accepted_first_words=['fff8','fff9'],
                interpretation='MPEG-2 ADTS ID=1 and layer=0; protection bit is not in this comparison',
                scope='Local sync predicate only; not MPEG-4 payload-tool absence or complete ADTS validation')


def decode_catalogue_media_admission(words):
    for base, witness in CATALOGUE_MEDIA_WITNESSES:
        for offset, expected in enumerate(witness):
            if words.get(base+offset) != expected:
                raise ValueError('AA22 catalogue media-admission witness changed or absent')
    return dict(segment='aa22', pinned_words=29, entry='039494',
                extension_source='local SP+0x28, derived directory descriptor +9',
                comparisons=[dict(literal='bff3', text='REL'),dict(literal='bff7', text='LSE')],
                compare_helper='02e36f', rejection='039570: advance directory enumeration',
                scope='Pinned scanner extension filter; not LSE container validity, AAC decoder capability or every media-opening route')


def decode_decoder_configuration(words):
    for base, witness in DECODER_CONFIGURATION_WITNESSES:
        for offset, expected in enumerate(witness):
            if words.get(base+offset) != expected:
                raise ValueError('AA99 original decoder configuration changed or absent')
    return dict(pinned_words=30, object='aaac',
                original_upload_configuration=['d43b..d44b'],
                caller='02786b -> wrapper 029870 -> virtual method 6',
                stack_arguments=['d43d', '0000', 'd43b'],
                resulting_object_fields={'aaad':'0000','aaae':'e800','aaaf':'f000'},
                scope='Original configuration bytes and call setup; not all caller branches, input refill or complete isolated decoder execution')


def decode_decoder_constructor(words):
    for offset, expected in enumerate(DECODER_CONSTRUCTOR_WITNESS):
        if words.get(DECODER_CONSTRUCTOR_BASE + offset) != expected:
            raise ValueError('AA99 decoder constructor witness changed or absent')
    for address, target in ((0xc692,0x3d27),(0xc69c,0x3d41)):
        if (words.get(address),words.get(address+1)) != (2,target):
            raise ValueError('AA99 decoder constructor/reset table pointer changed or absent')
    return dict(entry='023d27', pinned_words=30,
                method_table='c686', constructor_method=6, reset_method=11,
                object='A -> AR1', configuration_pointers=['local SP+4 -> AR2', 'local SP+6 -> AR3'],
                copies=['object[1] = AR3[1]', 'object[2] = AR2[9]', 'object[3] = AR2[14]'],
                tail='call original 023d41 with object in A',
                scope='Pinned local constructor ABI and table entries; configuration field meaning and upstream producer remain open')


def decode_decoder_channel_reset(words):
    for offset, expected in enumerate(DECODER_CHANNEL_RESET_WITNESS):
        if words.get(DECODER_CHANNEL_RESET_BASE + offset) != expected:
            raise ValueError('AA99 channel reset witness changed or absent')
    return dict(entry='0245d7', pinned_words=57,
                defaults={'a8c7':'0000','a85e':'0001','a85f':'ffff',
                          'a89d':'0001','a89e':'0003','b333':'aaec'},
                channel_iterations=2, channel_stride=6,
                channel_clears=['a89f','a8a5','[b335]+3','[b336]+3'],
                buffer=dict(first='af30', last='b32f', words=1024, value=0),
                expected_data_writes=1034,
                scope='Complete local source footprint with initialized pointers; not runtime retirement or all codec-state construction')


def decode_decoder_pointer_records(words):
    for offset, expected in enumerate(DECODER_POINTER_RECORD_WITNESS):
        if words.get(DECODER_POINTER_RECORD_BASE + offset) != expected:
            raise ValueError('AA99 decoder pointer initialization records changed or absent')
    rows = []
    for offset in range(0, len(DECODER_POINTER_RECORD_WITNESS), 4):
        count, destination, *values = DECODER_POINTER_RECORD_WITNESS[offset:offset+4]
        rows.append(dict(source=f'{DECODER_POINTER_RECORD_BASE+offset:06x}',
                         count=count, destination=f'{destination:04x}',
                         values=[f'{value:04x}' for value in values]))
    return dict(segment='aa99', pinned_words=16, records=rows,
                scope='Original count/destination/data records; not all indirect writers or object lifetime ownership')


def decode_decoder_reset(words):
    for offset, expected in enumerate(DECODER_RESET_WITNESS):
        if words.get(DECODER_RESET_BASE + offset) != expected:
            raise ValueError('AA99 decoder reset witness changed or absent')
    for offset, expected in enumerate(DECODER_BIT_RESET_WITNESS):
        if words.get(DECODER_BIT_RESET_BASE + offset) != expected:
            raise ValueError('AA99 bit-reader reset witness changed or absent')
    return dict(entry='023d41', pinned_words=len(DECODER_RESET_WITNESS),
                receiver='A -> AR1', calls=['0245d7', '0256cf'],
                clears=['ba50', 'ba44', 'receiver+4', 'receiver+5',
                        'doubleword receiver+6'],
                bit_reader_reset=dict(entry='0256cf', pinned_words=12,
                                      clears=['doubleword a858', 'a854', 'a85a']),
                scope='Complete local reset and bit-reader reset; first callee effects, receiver extent and executable external-input ABI remain open')


def decode_decoder_alignment(words):
    for offset, expected in enumerate(DECODER_ALIGNMENT_WITNESS):
        if words.get(DECODER_ALIGNMENT_BASE + offset) != expected:
            raise ValueError('AA99 decoder alignment witness changed or absent')
    return dict(entry='02542b', pinned_words=len(DECODER_ALIGNMENT_WITNESS),
                input='a854 remaining-bit count',
                positive_domain='0..31: consume remaining & 7 bits through 02510c',
                scope='Local byte-alignment helper, not a codec initializer or negative-count contract')


def decode_decoder_framing(words):
    for offset, expected in enumerate(DECODER_FRAMING_WITNESS):
        if words.get(DECODER_FRAMING_BASE + offset) != expected:
            raise ValueError('AA99 decoder framing witness changed or absent')
    return dict(segment='aa99', entry='023d73', pinned_words=len(DECODER_FRAMING_WITNESS),
                input_base='receiver dereference -> b332; b331 reset to zero',
                bypass='ba50 nonzero skips detection to 023da0',
                default_flags={'ba4f': 1, 'ba4e': 0},
                adif_signature=['4144', '4946'],
                adif_flags={'ba4f': 0, 'ba4e': 1},
                adif_initialization='if ba44 is zero: call 0241f2, call 02542b, set ba44=1',
                scope='Pinned local framing branch; not complete ADIF support, initializer ABI or external REL packaging')


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
PLAYBACK_CLEANUP_INGRESS = {
    0x27887: (0x76F8, 0xAA1A, 0x0001),
    0x29438: (0x1004,0x11F8,0x0008,0xF310,0x0011,0xF84D,0x9471,
              0x11F8,0x0008,0xF310,0x0016,0xF84D,0x9458,0x10F8,
              0x0008,0xF010,0x0017,0xF844,0x95AF,0x7105,0xAA1A,
              0x7604,0x8017,0x4818,0x7600,0x0001,0xF000,0x0004,
              0xF982,0x929C,0xF073,0x95AF),
    0x29471: (0x1005,0x8066,0x7107,0x0011,0x7108,0x0016,0x7604,
              0x8011,0x7109,0xAA1E,0x7600,0x0001,0x4818,0xF000,
              0x0004,0xF982,0x929C,0x76F8,0xAA1B,0x0000),
    0x294A7: (0x56F8,0x371A,0x4E6A,0xF0FF,0x4E6C,0x4EF8,0xAA22),
    0x294BB: (0x566C,0xF7B8,0x11F8,0x0011,0x53F8,0xA9F6,0xF3E0,
              0xF520,0xF84A,0x94EA,0xF6B8,0xF495,0x566A,0x54F8,
              0xA9F6,0xF847,0x94FB),
    0x294EA: (0x566C,0x08F8,0xA9F7,0x8811,0x76F8,0xAA1C,0x0001,
              0x566A,0x4EF8,0xA9F6,0xF073,0x94FB,0x566A,0x4EF8,
              0xA9F6,0x7711,0x0000,0x7712,0x0001),
    0x294FD: (0x60F8,0xAA1A,0x0001,0xF830,0x956F),
}


def decode_playback_cleanup_ingress(words):
    for base, witness in PLAYBACK_CLEANUP_INGRESS.items():
        for offset, expected in enumerate(witness):
            if words.get(base+offset) != expected:
                raise ValueError(f'cleanup ingress witness changed at {base+offset:06x}')
    references = [f'{address:06x}' for address, value in sorted(words.items()) if value == 0xaa1a]
    return {'pinned_words': sum(map(len, PLAYBACK_CLEANUP_INGRESS.values())),
            'field': 'aa1a', 'mapped_literal_references': references,
            'initial_store_pattern': {'address': '027887', 'value': 1},
            'input17': {'dispatch': '029445', 'store': '02944b',
                        'operation': 'MVDK local-frame word 5 -> aa1a',
                        'reply_word': '8017', 'continuation': '0295af'},
            'input11': {'entry': '029471', 'reply_word': '8011',
                        'gate': '0294fd', 'predicate': 'aa1a == 1',
                        'taken_target': '02956f',
                        'scope': 'intervening processing paths are not modeled'},
            'length_path': {'source_doubleword': '371a', 'local_full': 'SP+006a',
                            'local_logical_half': 'SP+006c', 'counter_doubleword': 'a9f6',
                            'compare': '0294c3 BGEQ -> 0294ea',
                            'taken_updates': 'AR1 = low16(local_half - a9f7); aa1c = 1; counter = local_full',
                            'join': '0294fb -> 0294fd',
                            'scope': 'Pinned dataflow; counter units and complete producer contract unvalidated'},
            'scope': 'Pinned dispatch/store/compare patterns; no runtime message ownership, '
                     'all-writer closure, EOF meaning or decoder failure claim'}


PLAYBACK_INPUT_UNITS_AA55 = {
    0x2086: (0xF880,0x32D3), 0x2090: (0xF880,0x32FA),
    0x2ED8: (0x880E,0xF495,0xF066,0x002C,0xF000,0x36B0,0x8811,
             0xF495,0xF495,0x56E1,0x0012,0x4E0A),
    0x32EE: (0xF072,0x32F4,0x1386,0xF678,0xF3E8,0xF1A0,0x8196),
    0x3349: (0x1102,0x7001,0x0016,0x6F00,0x0D81,0xF980,0x208A,
             0x1103,0x0104,0x8912,0xF495,0xF495,0x8082,0x6B02,
             0x0001,0x10E1,0x0015,0xF000,0x0001,0x8812,0x80E1,
             0x0015,0x1004,0xF000,0x0001,0x8004,0x6CEF,0xFFFF,0x3321),
}
PLAYBACK_INPUT_UNITS_AA99 = {
    0x2788A: (0xE800,0x4EF8,0xA9F6),
    0x292CC: (0x4A11,0xEEFD,0xF540,0x7107,0x0011,0x1006,0x7108,
             0x0012,0x7001,0x0012,0x7000,0x0011,0xF5E7,0xEE03,0x8A11,0xF4E4),
    0x29473: (0x7107,0x0011,0x7108,0x0016),
    0x294A7: (0x56F8,0x371A,0x4E6A,0xF0FF,0x4E6C,0x4EF8,0xAA22),
    0x295CD: (0x4A11,0xEEFB,0xF495,0x7109,0x0011,0x1108,0x8000,
             0x7002,0x0011,0x8101,0xF020,0x3708,0xF980,0x2090,
             0xEE05,0x8A11,0xF4E4,0x4A11,0xEEFB,0xF495,0x7109,
             0x0011,0x1108,0x8000,0x7002,0x0011,0x8101,0xF020,
             0x3708,0xF980,0x2086,0xEE05,0x8A11,0xF4E4),
}


def decode_playback_input_units(words, bootstrap_words):
    for image, spans in ((words, PLAYBACK_INPUT_UNITS_AA99),
                         (bootstrap_words, PLAYBACK_INPUT_UNITS_AA55)):
        for base, witness in spans.items():
            for offset, expected in enumerate(witness):
                if image.get(base+offset) != expected:
                    raise ValueError(f'input units witness changed at {base+offset:06x}')
    descriptor = 0x36b0 + 2*0x2c
    return {'pinned_words': sum(len(witness) for spans in
                (PLAYBACK_INPUT_UNITS_AA55, PLAYBACK_INPUT_UNITS_AA99) for witness in spans.values()),
            'descriptor': {'base': '36b0', 'stride_words': 0x2c, 'index': 2,
                           'address': f'{descriptor:04x}', 'size_pair': f'{descriptor+0x12:04x}'},
            'input11': {'count': 'local frame word 7 -> AR1 -> callback word-count argument',
                        'adapter': '0292cc', 'callbacks': ['0295cd', '0295de'],
                        'reader_apis': ['002090', '002086']},
            'reader': {'loop': '003349..003365', 'destination_stride_words': 1,
                       'byte_offset_scale': 2, 'swapping_api': '002086'},
            'limit': {'source': '371a descriptor size pair', 'operation': 'logical shift right 1',
                      'units': '16-bit file words for an even byte extent'},
            'counter_initialization': '02788a: zero -> a9f6 doubleword',
            'scope': 'Pinned source argument flow and descriptor arithmetic; not every runtime '
                     'read/seek retirement, all counter writers, odd-byte tails or decoder drain correctness'}


PLAYBACK_REFILL_CONTROL = {
    0x2065A: (0x6102,0x8000,0xF820,0x073D,0x7211,0x12FB,0xF495,
             0x6C81,0x0727,0x7211,0x12FC,0xF495,0x6C81,0x071C),
    0x20704: (0x80F8,0x15E2,0x7004,0x0017,0x8106,0x7602,0x0011,
             0x7007,0x15E1,0x7711,0x0006,0xF073,0x0733),
    0x2071C: (0x7003,0x0011,0x7602,0x0017,0x76F8,0x12FC,0x0000,
             0x7711,0x0002,0xF073,0x0733,0x7003,0x0011,0x7602,
             0x0016,0x76F8,0x15E0,0x0000,0x76F8,0x12FB,0x0000,
             0x7711,0x0002,0x4816,0xF982,0x0443,0x4818,0x7000,
             0x0011,0xF000,0x0002,0xF982,0x9629),
    0x29264: (0x8812,0xF495,0x7713,0x0090,0xEC0F,0xE598,0xF495,0xF495,0xF4E4),
    0x29502: (0xF7B8,0x10F8,0x0011,0x4E68,0x10F8,0x0012,0xF845,
             0x953E,0x56F8,0x3726,0x4EF8,0xA9F6,0x76F8,0xAA1C,
             0x0000,0x7602,0x3AEA,0x4E00,0xF020,0x3708,0xF980,0x2092),
    0x2953E: (0x5668,0x50F8,0xA9F6,0x4E68,0x7604,0x0012,0x7005,
             0x0011,0x10F8,0x0016,0xF844,0x954C,0x7716,0xFFFF,
             0x7006,0x0016,0x7007,0xAA1D,0x106E,0xF000,0x007A,
             0x7601,0x0008,0x8000,0x4818,0xF000,0x0008,0xF982,
             0x99CC,0x7010,0xAA1C,0xF6B8,0xF495,0x5668,0x8011,
             0xF0F0,0x8012,0x7600,0x000F,0x4818,0xF000,0x0004,
             0xF982,0x92A7,0x76F8,0xAA1D,0x0000,0xF073,0x95A8),
}


def reference_refill_control(reply, pending16, pending17):
    """Control-only sender prefix; ordinary input-11 refill gates are excluded."""
    if any(not 0 <= word <= 0xffff for word in (reply, pending16, pending17)):
        raise ValueError('refill control words must be 16-bit')
    if not reply & 0x8000:
        return None
    if pending16:
        return {'input': 0x16, 'payload': pending16}
    if pending17:
        return {'input': 0x17, 'payload': pending17}
    return None


PLAYBACK_PENDING_CONTROL = {
    0x22064: (0x4a1e,0xf495,0xf882,0x0742),
    0x20742: (0xf495,0x4a08,0x4a09,0x4a0a,0x4a0b),
    0x20981: (0x76f8,0x12f8,1,0xf7d9,0xf495,0xf495,0xf4e4),
    0x278cd: (0xf062,2,0xf040,0x0981,0xf982,0x92f0,0xf495,0xf495,0xf4e4),
    0x292f0: (0x4ef8,0xaa7e,0xe801,0xf495,0xf495,0xf4e4),
    0x292a7: (0x4a11,0x4a16,0xeefe,0xf495,0x7106,0x0016,0x8811,0xf073,
              0x92b2,0xf982,0x042f,0xf982,0x0424,0x61f8,0x0080,0x8000,
              0xf820,0x92b0,0xf020,0x0080,0x7000,0x0016,0x8001,0x4811,
              0xf982,0x9276,0x56f8,0xaa7e,0xf845,0x92c6,0xf4e7,0xf982,
              0x042f,0xee02,0x8a16,0x8a11,0xf4e4),
    0x20769: (0x4a08,0xeeed,0x7211,0x12f7,0xf495,0x6c81,0x0952,
              0x76f8,0x12f7,1,0x76f8,0x12f8,0,0x4818,0xf000,2,
              0xf982,0x960c,0x7716,1,0x7717,0,0xf7b8,0x7102,0x0011),
    0x207bc: (0x10f8,0x0011,0xf010,0x0017,0xf846,0x07d9,
              0x10f8,0x0011,0xf010,0x0017,0xf845,0x07f5,
              0x7710,0x0014,0xf4a9,0xf830,0x0825,
              0x7710,0x0015,0xf4a9,0xf830,0x0804,
              0x7710,0x0016,0xf4a9,0xf830,0x07fa,0xf073,0x0866),
    0x207f5: (0x76f8,0x12fc,1,0xf073,0x093d,
              0x76f8,0x12fa,1,0x76f8,0x15db,1,0x7103,0x12fb,0xf073,0x093d),
    0x29458: (0xe781,0x6de9,5,0x1081,0x7604,0x8016,0xf982,0x91d3,
              0xf020,0x3708,0x7601,9,0x8000,0x4811,0xf982,0x99cc,
              0x4818,0x7600,0x000a,0xf000,4,0xf982,0x929c,0xf073,0x95af),
    0x291d3: (0xf495,0xf495,0xf4e4),
    0x299cc: (0x8813,0x4a11,0xeeff,0xe730,0x7104,0x0014,0xf6ac,0xfa30,
              0x99ef,0x1105,0xe732,0x4814,0x00f8,0x000b,0x8811,0x4813,
              0x00f8,0x000b,0x490b,0xfa4d,0x99f9,0xf495,0x8812,0xf310,
              1,0x891a,0xf495,0xf072,0x99ec,0x10e9,0xffff,0x80ea,
              0xffff,0xf073,0x99f9,0x490b,0xf84d,0x99f9,0xf310,1,
              0x891a,0xf495,0xf072,0x99f8,0xe5a8,0xee01,0x4813,0x8a11,
              0xf495,0xf4e4),
}


def decode_playback_pending_control(words):
    for base, witness in PLAYBACK_PENDING_CONTROL.items():
        for offset, expected in enumerate(witness):
            if words.get(base + offset) != expected:
                raise ValueError(f'pending-control witness changed at {base + offset:06x}')
    return dict(pinned_words=sum(map(len, PLAYBACK_PENDING_CONTROL.values())),
                controller=dict(entry='020742', frame_setup='020769', request='CPL frame word 2 -> AR1',
                                input17='0207f5: pending 12fc <- 1',
                                input16='0207fa: 12fa <- 1; 15db <- 1; 12fb <- frame word 3'),
                receiver16=dict(entry='029458', reply='8016',
                                argument='frame word 5 -> A; CALL 0291d3',
                                source_hook='0291d3: NOP; NOP; FRET',
                                continuation='LD #3708,A; frame[1] <- 9; frame[0] <- A; A <- AR1 (frame+5); CALL 0299cc; send through 02929c',
                                copy='0299cc: nine words from fixed 3708..3710 to reply frame words 5..13',
                                copy_abi='destination A -> AR3/AR0; source caller frame[0] -> AR4; count caller frame[1]',
                                copy_direction='source AR4 > destination AR0: forward; otherwise backward; zero count skips body'),
                notification=dict(initializer='0278cd: callback 02:0981 -> setter 0292f0',
                                  pointer='aa7e', publisher='0292a7: copy to mailbox 0080; call nonzero aa7e',
                                  callback='020981: 12f8 <- 1; INTR 25',
                                  vector='022064: PSHM XPC; NOP; FB 02:0742',
                                  pointer_literal_references=[f'{address:06x}' for address, value in sorted(words.items()) if value == 0xaa7e]),
                scope='Pinned source fragments, not live message ownership, runtime execution or '
                      'hook immutability, an EOF/drain command or an all-producer closure')


PLAYBACK_PENDING16_PRODUCERS = {
    0x20780:(0x7102,0x0011,0x10f8,0x0011,0xf010,0x0013,0xf846,0x07bc,
        0x10f8,0x0011,0xf010,0x0013,0xf845,0x083c,0x10f8,0x0011,0xf010,4,
        0xf846,0x07ab,0x10f8,0x0011,0xf010,4,0xf845,0x08ad,
        0x7710,1,0xf4a9,0xf830,0x0907,0x7710,2,0xf4a9,0xf830,0x08f0,
        0x7710,3,0xf4a9,0xf830,0x08ce,0xf073,0x0866),
    0x208ce:(0xe800,0x80f8,0x15da,0x80f8,0x12f9,0xe900,0x1003,0xf844,
        0x08d8,0xe901,0x81f8,0x15db,0x10f8,0x12fd,0xf845,0x08e8,
        0x10f8,0x15db,0xf845,0x08e8,0x71f8,0x15de,0x15db,
        0x76f8,0x12fb,3,0x6004,1,0xf820,0x093d,0x7717,1,0xf073,0x093d),
    0x20907:(0xe800,0x80f8,0x15da,0x80f8,0x12f9,0x76f8,0x15db,1,
        0x76f8,0x15e1,0,0x76f8,0x12fc,0,0x76f8,0x12fb,3,
        0x76f8,0x15e0,0,0x7711,0xaa4c,0x76e1,2,0,0x76e1,3,0,
        0x76e1,4,0,0x76e1,5,0,0x76e1,6,0,0x76f8,0xaa53,0x001e,
        0x76f8,0x12fd,0,0x7602,0x8000,0x4818,0xf000,2,0x7600,1,
        0xf982,0x929c,0x7602,0x8001),
    0x2065e:(0x7211,0x12fb),
    0x2072e:(0x76f8,0x12fb,0),
    0x20800:(0x7103,0x12fb),
    0x278ba:(0x76f8,0x12fb,0),
}


def decode_playback_pending16_producers(images):
    if set(images)!= {'aa55','aa22','aa99','aa88'}:
        raise ValueError('pending16 census requires all four original mapped images')
    words=images['aa99']
    for base,witness in PLAYBACK_PENDING16_PRODUCERS.items():
        if any(words.get(base+offset)!=value for offset,value in enumerate(witness)):
            raise ValueError('pending16 producer/dispatcher source changed')
    expected={'aa55':[],'aa22':[],'aa88':[],
              'aa99':[0x2065f,0x2072f,0x20801,0x208e6,0x20916,0x278bb]}
    hits={tag:sorted(address for address,value in image.items() if value==0x12fb)
          for tag,image in images.items()}
    if hits!=expected:
        raise ValueError('pending16 raw literal inventory changed; reclassify added/missing hits')
    return dict(mapped_words_scanned={tag:len(image) for tag,image in images.items()},
        raw_literal_hits={tag:[f'{address:06x}' for address in addresses] for tag,addresses in hits.items()},
        pinned_words=sum(map(len,PLAYBACK_PENDING16_PRODUCERS.values())),
        classification=dict(read=['02065e'],clear=['02072e','0278ba'],
                            frame3_copy=['020800'],constant3=['0208e5','020915'],unclassified=[]),
        routes={'1':dict(entry='020907',pending='12fb <- 3 unconditionally',
                         other='clear12fc/15e0/15e1/12fd; initialize15db=1 and AA4C fields; publish reply'),
                '3':dict(entry='0208ce',pending='12fb <- 3 iff 12fd != 0 and caller frame[3] == 0',
                         other='clear15da/12f9; 15db <- (frame[3] == 0), then selected15de; frame[4]==1 sets AR7'),
                '16':dict(entry='0207fa',pending='12fb <- caller frame[3]')},
        scope='Raw mapped-word census plus pinned local routes; not a code/data split, computed-alias/bulk-writer closure, incoming command producer or runtime/EOF acceptance')


PLAYBACK_INPUT3_SENDERS = {
    0x2905d:(0x4a11,0xeefd,0xf495,0x1106,0x7107,0x0012,0x7711,0xba56,
        0x7681,3,0x80e1,1,0x81e1,2,0x70e1,3,0x0012,0x4811,0x7600,4,
        0xf982,0x92a7,0xee03,0x8a11,0xf4e4),
    0x38ba8:(0x10f8,0x3767,0x08e6,2,0xf844,0x8bba,0x10f8,0x3766,
        0x08f8,0x1187,0xf844,0x8bba,0xf7b8,0xe828,0x08f8,0x1188,0xf843,0x8f85),
    0x38bf1:(0xf110,0x0029,0xf062,2,0x11f8,0x000b,0xf040,0x3d09,
        0xf600,0x7ef8,0x0008,0xf4e2),
    0x23d10:(0x8d03,),
    0x38d03:(0x7105,0x3763,0x60f8,0x1189,2,0xf830,0x8d72,
        0x60f8,0x1189,3,0xf830,0x8d4b,0x60f8,0x1189,4,0xf830,0x8d3b,
        0x60f8,0x1189,0x0010,0xf830,0x8d2a),
    0x38d3b:(0x76f8,0x3762,3,0x76f8,0x3750,0,0xe800,0x4ef8,0x375e,
        0xe801,0x4ef8,0x3760,0xf982,0x8498,0xf073,0x8f85,0xf982,0x846f),
    0x2846f:(0x4a11,0xeefd,0x60f8,0xa825,1,0xf820,0x8494,
        0x76f8,0xa825,2,0xe801,0xf982,0x9189),
    0x2847c:(0x7600,0,0x7601,1,0xe801,0xf982,0x905d),
    0x28498:(0xeeaa,0x76f8,0xa825,0,0x76f8,0x375c,0,0xe800,
        0x7600,0,0x7601,1,0xf982,0x905d),
    0x28591:(0xeefe,0x76f8,0xa825,0,0xe800,0x7600,0,0x7601,1,
        0xf982,0x905d,0xee02,0xf495,0xf4e4),
}


def decode_playback_input3_senders(images):
    pending=decode_playback_pending16_producers(images)
    words=images['aa99']
    for base,witness in PLAYBACK_INPUT3_SENDERS.items():
        if any(words.get(base+offset)!=value for offset,value in enumerate(witness)):
            raise ValueError('playback input3 sender/serial selector source changed')
    raw={tag:sorted(address for address,value in image.items() if value==0x905d)
         for tag,image in images.items()}
    if raw!={'aa55':[],'aa22':[],'aa88':[],
             'aa99':[0x28482,0x284a5,0x2859b]}:
        raise ValueError('input3 builder raw reference inventory changed')
    return dict(pinned_words=pending['pinned_words']+sum(map(len,PLAYBACK_INPUT3_SENDERS.values())),
        mapped_words_scanned=pending['mapped_words_scanned'],
        builder=dict(entry='02905d',record='BA56',input=3,
            arguments='record[1] <- A; record[2] <- caller SP[0]; record[3] <- caller SP[1]',
            send='029071 -> 0292a7, count4'),
        direct_calls=['028481','0284a4','02859a'],
        raw_target_words={tag:[f'{address:06x}' for address in addresses] for tag,addresses in raw.items()},
        serial=dict(selector=0x30,table='023d09',index=7,entry='023d10',target='038d03',
            parameter3=dict(call='038d4b -> 02846f',builder_call='028481',arguments=[1,0,1],
                            gate='A825 == 1; set A825 <- 2 before builder',
                            pending_effect='frame3 is1, so the known input3 constant3 predicate is false'),
            parameter4=dict(call='038d47 -> 028498',builder_call='0284a4',arguments=[0,0,1],
                            pending_effect='frame3 is0; active12fd selects pending3'),
            duplicate_guard='matching previous class3766/selector3767 with selector>28 exits before dispatch'),
        additional_call=dict(site='02859a',entry='028591',arguments=[0,0,1],
                             owner='not established by this census'),
        scope='Pinned source sender/selector route and raw references across all four images; not live serial admission, complete external command grammar, all indirect callers or organic EOF acceptance')


def decode_playback_refill_control(words):
    for base, witness in PLAYBACK_REFILL_CONTROL.items():
        for offset, expected in enumerate(witness):
            if words.get(base+offset) != expected:
                raise ValueError(f'refill control witness changed at {base+offset:06x}')
    return {'pinned_words': sum(map(len, PLAYBACK_REFILL_CONTROL.values())),
            'sender': {'entry': '020643', 'reply_high_bit_gate': '02065a',
                       'priority': ['12fb nonzero -> input16', '12fc nonzero -> input17'],
                       'input11_builder': '020709', 'input17_builder': '02071c',
                       'input16_builder': '020727', 'send': '02073b -> 029629',
                       'input17_payload': 'nonzero pending word in AR1, not literal zero'},
            'receiver_copy': {'entry': '029264', 'source_data_words': '0090..009f',
                              'words': 16, 'direction': 'RAM mailbox -> local frame'},
            'alternate_end': {'entry': '029502 when aa1a != 1',
                              'conditional_seek': 'AR2 != 0: counter <- 3726; aa1c <- 0; seek 2092',
                              'read_callback': '02952f -> 0292cc',
                              'completion_builder': '029542: input12',
                              'completion_send': '029568 -> 0292a7'},
            'scope': 'Pinned software sender and alternate end path, not all producers or pending-word '
                     'writers; input17 through this prefix cannot clear aa1a to zero; '
                     'alternate seek/read path is not an established drain-only mode; no runtime forcing'}


PLAYBACK_REFILL_COMPLETION = {
    0x207B5: (0x7710,0x0012,0xF4A9,0xF830,0x0862),
    0x20862: (0x10F8,0x15E0,0xF844,0x086A,0x7602,0x8000,0xF073,
             0x093D,0x7103,0x15DF,0x10F8,0x15E7,0x7000,0x15DF,
             0xF982,0x976D,0x1210,0xF1EF,0x120F,0xF3E1,0xF2A0,
             0x4EF8,0xAAD0,0x10F8,0x15E7,0xF982,0x9752,0xF495,
             0x4808,0x58F8,0xAAD0,0x4EF8,0xAAD0,0x4E00,0x10F8,
             0x15E7,0xF982,0x978C,0x76F8,0x15E0,0x0000),
    0x2093D: (0x6902,0x8000,0x4818,0x7000,0x0016,0xF000,0x0002,
             0xF982,0x961E,0x10F8,0x0017,0xF845,0x094D),
    0x2960C: (0x8812,0xF495,0x7713,0x0080,0xEC0F,0xE598,0xF495,0xF495,0xF4E4),
    0x2976D: (0x4A11,0xEEFF,0x8811,0xF495,0x1004,0x00E1,0x0006,
             0x80E1,0x0006,0xEE01,0x8A11,0xF4E4),
}

PLAYBACK_FINAL_INPUT_WITNESSES = {
    0x294cc: (0x10f8,0x3733,0xf845,0x94d6,0xf062,2,0xf040,0x95cd,
              0xf073,0x94da,0xf062,2,0xf040,0x95de,0x1166,0x7001,
              0x0011,0x7602,0x3aea,0x8100,0xf982,0x92cc,0x8812,0xf020,
              0x36b0,0x806e,0x6c82,0x94fd),
    0x29500: (0xf830,0x956f,0xf7b8,0x10f8,0x0011,0x4e68,0x10f8,0x0012,0xf845,0x953e),
    0x2950a: (0x56f8,0x3726,0x4ef8,0xa9f6,0x76f8,0xaa1c,0,0x7602,
              0x3aea,0x4e00,0xf020,0x3708,0xf980,0x2092,0x7166,0x0012,
              0x10f8,0x3733,0xf845,0x9524,0xf062,2,0xf040,0x95cd,
              0xf073,0x9528,0xf062,2,0xf040,0x95de,0xf120,0x3aea,
              0x7000,0x0012,0x7001,0x0011,0x8102,0xf982,0x92cc,0xf120,
              0x36b0,0x816e,0x10f8,8,0xf845,0x953e,0x7600,0,
              0xf020,0x05a5,0xf982,0x91c2),
    0x29563: (0x7600,0x000f,0x4818,0xf000,4,0xf982,0x92a7,0x76f8,0xaa1d,0,0xf073,0x95a8),
    0x2959d: (0x7604,0x0016,0x7605,9,0x4818,0x7600,2,0xf000,4,0xf982,0x92a7),
}


def playback_final_input_words():
    spans = list(PLAYBACK_FINAL_INPUT_WITNESSES.items())
    spans += [(base,PLAYBACK_CLEANUP_INGRESS[base]) for base in (0x294bb,0x294ea,0x294fd)]
    spans += [(0x2953e,PLAYBACK_REFILL_CONTROL[0x2953e]),(REL_CLEANUP_BASE,REL_CLEANUP_WITNESS)]
    expected = {}
    for base,witness in spans:
        for offset,value in enumerate(witness):
            address = base+offset
            if address in expected and expected[address] != value:
                raise ValueError('conflicting final-input source witnesses')
            expected[address] = value
    return expected


def decode_playback_final_input(words):
    expected = playback_final_input_words()
    if any(words.get(address) != value for address,value in expected.items()):
        raise ValueError('playback final-input source changed or absent')
    return dict(pinned_words=len(expected),
                terminal_compare='0294c3: counter plus signed incoming count >= logical half extent',
                short_input='0294ea: AR1 <- low16(local half - counter low); aa1c <- 1; counter <- local full extent',
                join='0294fb sets AR2=1; 0294fd tests aa1a == 1',
                one_pass=dict(branch='029500 -> 02956f',
                              bypassed=['normal reader callback 0294e0','ordinary input-12 completion 029563..029568'],
                              cleanup='state 4 follows existing 02956f cleanup and loader call 02959b',
                              notification_in_source='02959d: local frame[4]=16, frame[5]=9; send two words through 0292a7',
                              ordering='notification follows loader call in the source; reaching loader is not proof that notification executes in the same session'),
                alternate=dict(branch='aa1a != 1 -> 029502',
                               behavior='nonzero AR2 restores counter from 3726, clears aa1c, calls API 002092 and reader callback 02952f before ordinary completion',
                               scope='Not evidence of a decoder-drain command or permission to change aa1a'),
                scope='Source-selected final-input control flow; not intended track sample count, all live message delivery, or correct whole-file/audio completion')


def compare_playback_revision_words(reference,candidate):
    """Bounded R060/R061 AA99 comparison, not product/runtime equivalence."""
    checkers = (decode_playback_cleanup_ingress,decode_worker_reservation,
                decode_context_lanes,decode_exchange_publication,decode_decoder_return_dispatch)
    unchanged = {}
    for checker in checkers:
        left,right = checker(reference),checker(candidate)
        if ({key:value for key,value in left.items() if key!='mapped_words_scanned'} !=
                {key:value for key,value in right.items() if key!='mapped_words_scanned'}):
            raise ValueError('revision contract data or census changed')
        unchanged[checker.__name__] = left['pinned_words']
    expected = playback_final_input_words()
    decode_playback_final_input(reference)
    if any(address not in candidate for address in expected):
        raise ValueError('revision final-input source incomplete')
    differences = {address:(reference[address],candidate[address]) for address in expected
                   if reference[address]!=candidate[address]}
    if differences != {0x2958b:(0x9005,0x9019)}:
        raise ValueError('revision final-input difference changed')
    return dict(reference_mapped_words=len(reference),candidate_mapped_words=len(candidate),
        unchanged_contract_checks=unchanged,final_input_words=len(expected),
        final_input_difference=dict(address='02958b',reference='9005',candidate='9019',
            meaning='Operand of far call in control2 cleanup branch; target semantics not compared'),
        scope='Original AA99 bounded source comparison and exact immediate-call/literal censuses; '
              'not other images, all indirect writers, optional callee equivalence, R061 execution or full drain behavior')


def reference_refill_completion(pending, supplied, field6):
    """Only the pending test and low-word cursor update, not the full receiver."""
    if any(not 0 <= word <= 0xffff for word in (pending, supplied, field6)):
        raise ValueError('completion operands must be 16-bit')
    if pending == 0:
        return {'handled': False, 'field6': field6, 'pending': 0, 'reply': 0x8000}
    return {'handled': True, 'field6': (field6+supplied) & 0xffff,
            'pending': 0, 'last_supplied': supplied, 'reply': 0x8012}


def decode_playback_refill_completion(words):
    for base, witness in PLAYBACK_REFILL_COMPLETION.items():
        for offset, expected in enumerate(witness):
            if words.get(base+offset) != expected:
                raise ValueError(f'refill completion witness changed at {base+offset:06x}')
    return {'pinned_words': sum(map(len, PLAYBACK_REFILL_COMPLETION.values())),
            'input': '0012', 'dispatch': '0207b5 -> 020862',
            'pending': '15e0', 'inactive': 'local reply <- 8000; skip buffer update',
            'active': {'supplied': 'local frame word 3 -> 15df',
                       'buffer': 'object referenced by 15e7',
                       'advance': '02976d: object[6] += supplied modulo 16 bits',
                       'pending_clear': '020888',
                       'other_effects': 'aad0 and object metadata updated; not modeled by cursor reference'},
            'acknowledgement': '02093d sets high bit; 020944 -> 02961e',
            'mailbox_copy': '02960c copies 16 words from data 0080 to local frame',
            'scope': 'Pinned software completion path, not all callers or an EOF/drain protocol; '
                     'no completion injection, native tail decoding or audio acceptance'}


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


RESIDENT_IDLE3 = {
    0x28914: (0x4a00,0x4a01,0x7700,0x0004,0xf495,0x7758,0x0000,
              0x61f8,0x0058,0x0001,0xf830,0x891b,0x7758,0x0000,
              0xf5e1,0xf495,0xf495,0x8a01,0x8a00,0xf495,0xf495,0xf4e4),
    0x3ac07: (0x7711,0x0000,0x6981,0x0004,0xf982,0x8914,
              0xf020,0x07d0,0xf983,0xacdf),
}

RESIDENT_WAKE_TAIL = {
    0x3ac03: (0x10f8,0xb582,0xf844,0xac11),
    0x3ac0d: (0xf020,0x07d0,0xf983,0xacdf,0x76f8,0xb558,0x0000),
    0x3acdf: (0x4a11,0xeefd,0xf495,0x7600,0x000a,0xf980,0x2098,
              0x8811,0x7210,0xb582,0xf495,0xf6a9,0xf820,0xacef,
              0x7311,0xb582,0xee03,0x8a11,0xf4e4),
    0x3a8c9: (0x10f8,0xb582,0xf845,0xa8d0,0x6bf8,0xb582,0xffff,
              0x60f8,0xbb94,0x0002,0xf820,0xa8da,0x6bf8,0xb582,0x0001),
}
RESIDENT_WAKE_DIVIDE = {
    0x2098: (0xf880,0x352c),
    0x352c: (0xf6b8,0x4808,0xec0f,0x1e02,0xf4e4),
}


def decode_resident_wake_tail(words,bootstrap):
    for image,spans in ((words,RESIDENT_WAKE_TAIL),(bootstrap,RESIDENT_WAKE_DIVIDE)):
        for base, witness in spans.items():
            for offset, expected in enumerate(witness):
                if image.get(base+offset) != expected:
                    raise ValueError(f'resident wake tail changed at {base+offset:06x}')
    return dict(segment='aa22+aa55', pinned_words=52,
                idle_gate={'load':'03ac03','branch':'03ac05: BC ANEQ,03ac11',
                           'effect':'nonzero b582 bypasses resident deep-idle call'},
                caller={'entry':'03ac0d','argument':'07d0','call':'03ac0f','target':'03acdf',
                        'following_store':{'pc':'03ac11','address':'b558','value':'0000'}},
                helper={'entry':'03acdf','last_word':'03acf1','stack_literal':'000a',
                        'nested_call':'00:2098','read_counter':'03ace7: MVDM b582,AR0',
                        'compare':'03acea: CMPR GT,AR1; BC NTC,03acef',
                        'conditional_update':'03aced: MVMD AR1,b582; written only when AR1 > AR0',
                        'effect':'raise b582 to at least the nested helper result; no backward branch'},
                divide={'stub':'002098: B 00352c',
                        'body':'RSBX SXM; LDM AL,A; RPT #15; SUBC SP(2),A; FRET',
                        'addressing':'ST1.CPL=1 ABI; native wake context ST1=6113',
                        'quotient':'low16 of unsigned low16(A) / SP(2)',
                        'caller_result':'07d0 / 000a = 00c8'},
                decrement={'entry':'03a8c9','zero_skip':'03a8cb: BC AEQ,03a8d0',
                           'subtract':'03a8cd: ADDM ffff,b582',
                           'inhibit':'03a8d0: BITF bb94,2; BC NTC,03a8da',
                           'add_back':'03a8d5: ADDM 1,b582',
                           'scope':'source prefix only; worker cadence and flag ownership unqualified'},
                scope='Pinned caller/helper/divide grammar; counter units, '
                      'physical delay and official command semantics unqualified')


def resident_wake_countdown(counter,argument):
    if any(type(value) is not int or not 0<=value<=0xffff for value in (counter,argument)):
        raise ValueError('countdown model requires unsigned 16-bit inputs')
    return max(counter,argument//10)


def decode_resident_idle3(images):
    if set(images)!={'aa55','aa22','aa99','aa88'}:
        raise ValueError('resident IDLE3 census requires all four images')
    words=images['aa22']
    for base,witness in RESIDENT_IDLE3.items():
        for offset,expected in enumerate(witness):
            if words.get(base+offset)!=expected:
                raise ValueError(f'resident IDLE3 source changed at {base+offset:06x}')
    calls={marker:[f'{address:06x}' for address,value in sorted(image.items())
                   if value==0xf982 and image.get(address+1)==0x8914]
           for marker,image in images.items()}
    expected={'aa55':[],'aa22':['03ac0b'],'aa99':[],'aa88':[]}
    if calls!=expected:raise ValueError('resident IDLE3 literal far-call pattern census changed')
    mask=words[0x28917]
    return dict(segment='aa22',pinned_words=sum(map(len,RESIDENT_IDLE3.values())),
                routine=['028914','028929'],idle={'address':'028922','word':'f5e1','mode':3},
                mask={'store':'028916','value':mask,'enabled_bits':[bit for bit in range(16) if mask & (1<<bit)]},
                clock=[{'address':'028919','register':'CLKMD','value':0},
                       {'address':'02891b','operation':'BITF CLKMD,1; BC TC,02891b'},
                       {'address':'028920','register':'CLKMD','value':0}],
                tail={'ifr':'POPM IFR writes saved word through W1C acknowledgement, not a pending-bit restore',
                      'imr':'POPM IMR restores caller mask','return':'FRET'},
                caller={'address':'03ac0b','prelude':'AR1=0; ORM 4,*AR1 enables IMR bit2'},
                mapped_words={marker:len(image) for marker,image in images.items()},
                literal_far_calls=calls,
                scope='Complete 22-word routine and 10-word caller span; raw adjacent far-call patterns only, '
                      'not code/data classification, indirect-call closure, MSP430 timing or automatic EOF')


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


RESIDENT_RESTART_WITNESS = {
    0x3a92b: (0x7212,0x373e,0xf7b8,0x4812,0x11f8,8,0xf310,6,0xf84e,0xa94f),
    0x3a935: (0x11f8,8,0xf310,5,0xf84a,0xa95b),
    0x3a95b: (0x10f8,0x12,0xf010,5,0xf845,0xa987),
    0x3a987: (0x60f8,0x3762,4,0xf830,0xa98f,0x76f8,0x3762,6,
               0x60f8,0x3740,3,0xf830,0xa9aa,0xe809,0xf982,0x921f,
               0x6bf8,0x3765,1,0x44f8,0x3765,0x40f8,0x3768,0x82f8,0x12,
               0xf495,0xf495,0x6ce2,0xffff,0xa9af,0x76f8,0x3765,1,
               0xf073,0xa9af,0xf983,0x999f,0xe80d,0xf982,0x921f,
               0x68f8,0x3732,0xffef,0x76f8,0x373e,7,0xf073,0xaacc),
}


def resident_cleanup_restart(continuation, mode, current, total):
    """State-5 branch only; valid track indices, not track-media acceptance."""
    if (any(not 0 <= value <= 0xffff for value in (continuation, mode, current, total))
            or not 1 <= current <= total):
        raise ValueError('restart model requires word fields and a valid track index')
    next_track = (current + 1) & 0xffff
    # STH of the shifted subtraction, then BANZ *AR2(-1), without modifying AR2.
    if ((next_track - total - 1) & 0xffff) == 0:
        next_track = 1
    return {'continuation': 4 if continuation == 4 else 6,
            'track': current if mode == 3 else next_track,
            'index_advanced': mode != 3, 'lifecycle': 7,
            'notification': 0x0d if mode == 3 else 9,
            'extra_call': '03999f' if mode == 3 else None}


def decode_resident_cleanup_restart(words):
    for address, witness in RESIDENT_RESTART_WITNESS.items():
        for offset, expected in enumerate(witness):
            if words.get(address + offset) != expected:
                raise ValueError(f'resident restart witness changed at {address+offset:06x}')
    return {'segment': 'aa22', 'pinned_words': sum(map(len, RESIDENT_RESTART_WITNESS.values())),
            'lifecycle_field': '373e', 'selected_lifecycle': 5,
            'entry': '03a987', 'mode_field': '3740', 'nonadvance_mode': 3,
            'continuation_field': '3762', 'preserved_continuation': 4,
            'other_continuation': 6, 'track_index_field': '3765', 'track_count_field': '3768',
            'advance': 'increment index; wrap to 1 when increment equals track count + 1',
            'tail': 'clear 3732 bit 4; lifecycle <- 7; branch 03aacc',
            'single_track_example': resident_cleanup_restart(2, 0, 1, 1),
            'scope': 'Pinned resident state-5 control path and valid-index arithmetic; '
                     'not runtime mode ownership, repeat-setting semantics, tail disposition or audio completion'}


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


def playback_lifecycle_literal_census(words, fields=(0x373e, 0x3742)):
    """Enumerate raw references; instruction-shaped words are not code ownership."""
    targets = set(fields)
    if not targets or any(not 0 <= value <= 0xffff for value in targets):
        raise ValueError('literal census targets must be nonempty 16-bit fields')
    references = []
    for address, value in sorted(words.items()):
        if value not in targets:
            continue
        opcode = words.get(address - 1)
        row = {'operand_address': f'{address:06x}', 'field': f'{value:04x}',
               'preceding_word': None if opcode is None else f'{opcode:04x}',
               'classification': 'unclassified_literal'}
        if opcode == 0x76f8 and address + 1 in words:
            row.update(classification='absolute_immediate_store_pattern',
                       instruction_address=f'{address-1:06x}',
                       immediate=f'{words[address+1]:04x}')
        elif opcode == 0x60f8 and address + 1 in words:
            row.update(classification='absolute_immediate_compare_pattern',
                       immediate=f'{words[address+1]:04x}')
        elif opcode == 0x10f8:
            row['classification'] = 'absolute_load_a_pattern'
        elif opcode is not None and opcode & 0xff00 == 0x7100:
            row.update(classification='mvdk_absolute_destination_pattern',
                       instruction_address=f'{address-1:06x}', source_smem=f'{opcode&0xff:02x}')
        references.append(row)
    return {'mapped_words': len(words), 'literal_references': len(references),
            'classes': dict(Counter(row['classification'] for row in references)),
            'references': references,
            'scope': 'All mapped literal occurrences of '+ '/'.join(f'{value:04x}' for value in sorted(targets)) + '; raw patterns only, '
                     'not instruction-boundary validation, runtime writers, indirect stores '
                     'or an all-alias ownership/absence proof'}


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
    result['catalogue_media_admission'] = decode_catalogue_media_admission(words)
    result['worker_entry'] = decode_worker_entry(words)
    result['worker_return'] = decode_worker_return(words)
    result['worker_copy'] = decode_worker_copy(words)
    result['worker_logic'] = decode_worker_logic(words)
    result['full_worker'] = decode_full_worker(words)
    result['timer_wrapper'] = decode_timer_wrapper(words)
    result['idle_lifecycle'] = decode_idle_contract(words)
    result['resident_cleanup_restart'] = decode_resident_cleanup_restart(words)
    bootstrap_words = uploaded_word_image(bytes.fromhex(source.decode('ascii')), 'aa55')
    result['bootstrap_repeat'] = decode_bootstrap_repeat(bootstrap_words)
    result['boot_memory_shift'] = decode_boot_memory_shift(bootstrap_words)
    result['boot_block_repeat'] = decode_boot_block_repeat(bootstrap_words)
    rel_words = uploaded_word_image(bytes.fromhex(source.decode('ascii')), 'aa99')
    result['playback_lifecycle_literals'] = {
        'aa55': playback_lifecycle_literal_census(bootstrap_words),
        'aa99': playback_lifecycle_literal_census(rel_words),
    }
    result['playback_refill_policy_literals'] = {
        marker: playback_lifecycle_literal_census(image, fields=(0xaa1a, 0x12fc))
        for marker, image in (('aa55', bootstrap_words), ('aa22', words), ('aa99', rel_words))
    }
    result['playback_cleanup_ingress'] = decode_playback_cleanup_ingress(rel_words)
    result['playback_input_units'] = decode_playback_input_units(rel_words, bootstrap_words)
    result['playback_refill_control'] = decode_playback_refill_control(rel_words)
    result['playback_pending_control'] = decode_playback_pending_control(rel_words)
    result['playback_refill_completion'] = decode_playback_refill_completion(rel_words)
    result['filter_parent'] = decode_filter_parent(rel_words)
    result['output_processing'] = decode_output_processing(rel_words)
    result['output_scale'] = decode_output_scale(rel_words)
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
    result['decoder_framing'] = decode_decoder_framing(rel_words)
    result['decoder_input_descriptor'] = decode_decoder_input_descriptor(rel_words)
    result['decoder_input_reserve'] = decode_decoder_input_reserve(rel_words)
    result['playback_info_query'] = decode_playback_info_query(rel_words)
    result['playback_buffer_refresh'] = decode_playback_buffer_refresh(rel_words)
    result['playback_final_input'] = decode_playback_final_input(rel_words)
    result['pocp_buffer_count'] = decode_pocp_buffer_count(rel_words,words)
    result['metadata_overwrite_extent'] = decode_metadata_overwrite_extent(words,bootstrap_words)
    result['decoder_alignment'] = decode_decoder_alignment(rel_words)
    result['decoder_reset'] = decode_decoder_reset(rel_words)
    result['decoder_pointer_records'] = decode_decoder_pointer_records(rel_words)
    result['decoder_channel_reset'] = decode_decoder_channel_reset(rel_words)
    result['decoder_constructor'] = decode_decoder_constructor(rel_words)
    result['decoder_configuration'] = decode_decoder_configuration(rel_words)
    result['decoder_sync'] = decode_decoder_sync(rel_words)
    result['decoder_mapped_alu'] = decode_decoder_mapped_alu(rel_words)
    result['decoder_dispatch'] = decode_decoder_dispatch(rel_words)
    result['decoder_return_dispatch'] = decode_decoder_return_dispatch(rel_words)
    result['worker_reservation'] = decode_worker_reservation(rel_words)
    result['context_lanes'] = decode_context_lanes(rel_words)
    result['exchange_publication'] = decode_exchange_publication(rel_words)
    result['decoder_method_prefix'] = decode_decoder_method_prefix(rel_words)
    result['decoder_info_body'] = decode_decoder_info_body(rel_words,bootstrap_words)
    result['decoder_info_helpers'] = decode_decoder_info_helpers(rel_words,bootstrap_words)
    result['pcm_conversion'] = decode_pcm_conversion(rel_words)
    recorder_words = uploaded_word_image(bytes.fromhex(source.decode('ascii')), 'aa88')
    result['playback_pending16_producers'] = decode_playback_pending16_producers(
        dict(aa55=bootstrap_words,aa22=words,aa99=rel_words,aa88=recorder_words))
    result['playback_input3_senders'] = decode_playback_input3_senders(
        dict(aa55=bootstrap_words,aa22=words,aa99=rel_words,aa88=recorder_words))
    result['resident_idle3'] = decode_resident_idle3(
        dict(aa55=bootstrap_words,aa22=words,aa99=rel_words,aa88=recorder_words))
    result['resident_wake_tail'] = decode_resident_wake_tail(words,bootstrap_words)
    result['recorder_stop_frontend'] = decode_recorder_stop_frontend(recorder_words)
    result['recorder_stop_mailbox'] = decode_recorder_stop_mailbox(recorder_words)
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
    parser.add_argument('--compare-playback-revision',type=Path,
                        help='compare bounded playback contracts against the original R061 image; not runtime support')
    parser.add_argument('--recording-header-log', type=Path,
                        help='check the bounded first-eight-header argument observation')
    parser.add_argument('--playback-poly-log', type=Path,
                        help='check one bounded native three-step polynomial bus observation')
    parser.add_argument('--recording-session-log',type=Path,help='check the complete bounded encoder header-argument sequence')
    parser.add_argument('--recording-session-reference',type=Path,help='compare headers against separately supplied AAC bytes; no payload validation')
    parser.add_argument('--recording-session-reference-prefix',action='store_true',
                        help='compare only a complete observed packet prefix, report unmatched publications')
    args = parser.parse_args()
    if args.recording_session_reference and not args.recording_session_log:
        parser.error('--recording-session-reference requires --recording-session-log')
    if args.recording_session_reference_prefix and not args.recording_session_reference:
        parser.error('--recording-session-reference-prefix requires --recording-session-reference')
    try:
        source = args.source.read_bytes()
        result = contract(source)
        if args.compare_playback_revision:
            candidate = args.compare_playback_revision.read_bytes()
            candidate_hash = hashlib.sha256(candidate).hexdigest()
            if candidate_hash!='fad9a4d8c340c91fc06355ba18b42cdff9f2682ed48abb88903344d281b89d99':
                raise ValueError('playback revision comparison requires the original R061 source')
            comparison = compare_playback_revision_words(
                uploaded_word_image(bytes.fromhex(source.decode('ascii')),'aa99'),
                uploaded_word_image(bytes.fromhex(candidate.decode('ascii')),'aa99'))
            comparison.update(reference_sha256=hashlib.sha256(source).hexdigest(),candidate_sha256=candidate_hash)
            result['playback_revision_comparison'] = comparison
        if args.recording_header_log:
            result['recording_header_observation'] = recording_header_observations(args.recording_header_log.read_text())
        if args.playback_poly_log:
            result['playback_polynomial_observation'] = playback_poly_observation(args.playback_poly_log.read_text())
        if args.recording_session_log:
            reference = args.recording_session_reference.read_bytes() if args.recording_session_reference else None
            result['recording_session_observation'] = recording_session_observations(
                args.recording_session_log.read_text(),reference,
                complete_reference_prefix=args.recording_session_reference_prefix)
    except (OSError, ValueError) as error:
        print(f'MU4 context contract: {error}', file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
