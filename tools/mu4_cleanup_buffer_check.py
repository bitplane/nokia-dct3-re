"""Inspect a passive cleanup RAM snapshot; ADTS candidates are not decoded audio."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

if __package__:
    from .mu4_context_contract import reference_refill_completion
    from .mu4_playback_frame_check import rows
    from .mu4_recorder_media_inventory import adts_inventory
else:
    from mu4_context_contract import reference_refill_completion
    from mu4_playback_frame_check import rows
    from mu4_recorder_media_inventory import adts_inventory

FIRST, WORDS = 0x80, 0xff80


def check_completions(text):
    scopes = rows(text, 'mu4_refill_completion_scope')
    events = rows(text, 'mu4_refill_completion')
    ends = rows(text, 'mu4_refill_completion_terminal')
    stops = rows(text, 'mu4_refill_completion_stop')
    parents = rows(text, 'mu4_playback_parent_terminal')
    if (len(scopes) != 1 or scopes[0] != {'identity': 'debugger_Execute_hook',
            'operands': 'context_only', 'observer_writes': '0', 'audio_acceptance': '0'}
            or any(len(items) != 1 for items in (ends, stops, parents))):
        raise ValueError('incomplete completion scope/terminal')
    end, stop, parent = ends[0], stops[0], parents[0]
    if (not 0 < len(events) == int(end['events']) < int(end['cap'])
            or end['audio_acceptance'] != '0' or stop['audio_acceptance'] != '0'
            or stop['illegal'] != '0' or float(stop['t']) < 9.86
            or any(parent.get(key) != value for key, value in {'illegal': '0',
                'first_session_complete': '1', 'overlay_verified': '1', 'opcode_mismatch': '0'}.items())):
        raise ValueError('capped or unfinished completion session')
    overlay = 'mu4_native_playback_overlay: PASS marker=aa99 records=3127 words=49843 checked_ram_words=49839 excluded_register_words=4 before_execution=1'
    residents = [line for line in text.splitlines() if 'mu4_playback_execute_entry: address=026d62' in line]
    if (text.count(overlay) != 1 or len(residents) < 2
            or not text.index(residents[0]) < text.index('mu4_refill_completion:')
            or text.index(overlay) > text.index('mu4_refill_completion:')
            or text.rindex('mu4_refill_completion:') > text.index(residents[1])):
        raise ValueError('completion outside verified first AA99 session')
    pending, previous, handled, inactive = None, -1, 0, 0
    controls, counts = [], []
    addresses = {'entry': '020862', 'updated': '020893', 'control19': '0207ea'}
    for index, event in enumerate(events, 1):
        kind, cycles = event['kind'], int(event['cycles'])
        if (int(event['ordinal']) != index or cycles <= previous or addresses.get(kind) != event['address']
                or event.get('identity') != 'debugger_Execute_hook' or event.get('operands') != 'context_only'):
            raise ValueError('completion identity/order mismatch')
        previous = cycles
        for key in ('pending', 'supplied', 'target', 'field6'):
            if not 0 <= int(event[key], 16) <= 0xffff:
                raise ValueError('completion context outside word domain')
        if not 0x80 <= int(event['target'], 16) <= 0xfff9:
            raise ValueError('completion object outside RAM scope')
        if kind == 'entry':
            if pending is not None:
                raise ValueError('overlapping completion entries')
            if int(event['pending'], 16):
                pending = event
            else:
                inactive += 1
        elif kind == 'updated':
            if pending is None:
                raise ValueError('completion update without active entry')
            model = reference_refill_completion(*(int(pending[key], 16) for key in ('pending', 'supplied', 'field6')))
            if (event['target'] != pending['target'] or event['supplied'] != pending['supplied']
                    or int(event['pending'], 16) != model['pending'] or int(event['field6'], 16) != model['field6']):
                raise ValueError('completion cursor or pending clear differs from source model')
            counts.append(model['last_supplied'])
            handled += 1
            pending = None
        else:
            if pending is not None:
                raise ValueError('control19 overlaps an unfinished completion')
            controls.append({'cycles': cycles, 'argument': int(event['supplied'], 16),
                             'pending_refill_context': int(event['pending'], 16)})
    if pending is not None or not handled or not controls:
        raise ValueError('missing completion return or lifecycle control')
    return {'events': len(events), 'handled_completions': handled, 'inactive_entries': inactive,
            'supplied_word_sum': sum(counts), 'control19': controls,
            'scope': 'Paired debugger Execute-hook snapshots agree with source cursor/pending model; '
                     'not retained operands, file-read retirement, native EOF draining or audio acceptance'}


def inspect_ram(data, pointer, cursor, limit):
    if len(data) != WORDS*2:
        raise ValueError('cleanup snapshot must cover 0080..ffff as LE16 words')
    def word(address):
        if not FIRST <= address <= 0xffff:
            raise ValueError('snapshot reference outside captured RAM')
        return struct.unpack_from('<H', data, 2*(address-FIRST))[0]
    if not FIRST <= pointer <= 0xfff3 or word(0x15e9) != pointer:
        raise ValueError('input object reference mismatch')
    if (word(pointer), word(pointer+1)) != (cursor, limit):
        raise ValueError('input cursor/limit snapshot mismatch')
    if not FIRST <= cursor <= limit <= 0xffff:
        raise ValueError('wrapped or unsupported input extent')
    low = data[2*(cursor-FIRST):2*(limit-FIRST)]
    high = bytearray(low)
    high[0::2], high[1::2] = low[1::2], low[0::2]
    views = {}
    for name, source in (('high_byte_first', bytes(high)), ('low_byte_first', low)):
        inventory = adts_inventory(source)
        views[name] = {key: inventory[key] for key in
                       ('file_bytes', 'sha256', 'candidate_count', 'first_candidate_chain',
                        'chain_stop', 'chain_stop_reason')}
        views[name]['candidate_headers'] = inventory['candidates']
    file_buffer = word(0x15e7)
    file_object = None
    if FIRST <= file_buffer <= 0xfff3:
        file_object = {'address': f'{file_buffer:04x}',
                       'words': [f'{word(file_buffer+i):04x}' for i in range(12)],
                       'field0_1_difference': (word(file_buffer+1)-word(file_buffer)) & 0xffff}
    return {'ram_sha256': hashlib.sha256(data).hexdigest(),
            'object': f'{pointer:04x}', 'object_words': [f'{word(pointer+i):04x}' for i in range(12)],
            'lifecycle': f'{word(0x373e):04x}', 'file_buffer_ref': f'{word(0x15e7):04x}',
            'pending_refill': word(0x15e0), 'last_supplied': word(0x15df),
            'file_buffer_object': file_object,
            'mailbox_first_words': {'0080': f'{word(0x80):04x}', '0090': f'{word(0x90):04x}'},
            'cursor': f'{cursor:04x}', 'limit': f'{limit:04x}', 'available_words': limit-cursor,
            'resident_selection': {'mode': word(0x3740), 'track': word(0x3765),
                                   'count': word(0x3768), 'continuation': word(0x3762)},
            'file_control': {'descriptor_bytes': (word(0x371a) << 16) | word(0x371b),
                             'counter': (word(0xa9f6) << 16) | word(0xa9f7),
                             'policy': word(0xaa1a), 'short_input': word(0xaa1c)},
            'bit_reader': {'remaining': word(0xa854),
                           'reservoir': f'{(word(0xa858)<<16)|word(0xa859):08x}',
                           'index': word(0xb331), 'base': f'{word(0xb332):04x}'},
            'byte_views': views,
            'scope': 'Captured contiguous pointer extent; both byte views reported without choosing '
                     'a fitted layout; ADTS signatures/lengths do not establish payload validity, '
                     'file provenance, retained operands, draining correctness or audio acceptance'}


def compare_projection_tail(text, data, result, reference):
    """Use captured processing ordinal and reservoir bits, never a fitted offset."""
    calls, blocks = rows(text, 'mu4_decoder_call'), rows(text, 'mu4_decoder_block')
    if not blocks or len(calls) != 2*len(blocks):
        raise ValueError('incomplete processing ordinal for tail comparison')
    for index, block in enumerate(blocks):
        entry, returned = calls[2*index:2*index+2]
        if (entry.get('phase') != 'entry' or returned.get('phase') != 'return'
                or any(int(row['call']) != index+1 for row in (entry, returned, block))
                or any(row.get('identity') != 'Execute_entry' for row in (entry, returned, block))
                or any(row.get('context_only') != '1' for row in (entry, returned))
                or block.get('retirement') != 'unclaimed' or int(block['words']) != 2048*(index+1)
                or int(returned['result'], 16) & 0xffff
                or int(block['frame']) != index or block.get('bad') != '0'):
            raise ValueError('failed or nonordinal native processing before tail')
    reader = result['bit_reader']
    remaining = reader['remaining']
    if not 0 <= remaining <= 32 or remaining % 8 or reader['base'] != result['cursor']:
        raise ValueError('tail comparison requires byte-aligned buffered reservoir at cursor')
    cursor, limit = int(result['cursor'], 16), int(result['limit'], 16)
    raw = data[2*(cursor-FIRST):2*(limit-FIRST)]
    high = bytearray(raw)
    high[::2], high[1::2] = raw[1::2], raw[::2]
    prefix = (int(reader['reservoir'], 16) & ((1<<remaining)-1)).to_bytes(remaining//8, 'big')
    buffered = prefix + bytes(high)
    inventory = adts_inventory(reference)
    chain = inventory['first_candidate_chain']
    if len(chain) <= len(blocks):
        raise ValueError('projection has no tail after captured processing ordinal')
    offset = chain[len(blocks)]
    expected = reference[offset:]
    if len(buffered) < len(expected) or buffered[:len(expected)] != expected:
        raise ValueError('buffer differs from projection at fixed processing ordinal')
    return {'processing_blocks': len(blocks), 'reference_sha256': hashlib.sha256(reference).hexdigest(),
            'reference_packet_ordinal_zero_based': len(blocks), 'reference_offset': offset,
            'reservoir_prefix_hex': prefix.hex(), 'equal_tail_bytes': len(expected),
            'uncompared_buffer_bytes': len(buffered)-len(expected),
            'scope': 'Byte equality to a host-side projection at captured ordinal; '
                     'not native tail decoding, validity of the final projected packet, '
                     'waveform conformance or drain correctness'}


def check(text, data, reference=None):
    scopes = rows(text, 'mu4_cleanup_buffer_scope')
    events = rows(text, 'mu4_cleanup_buffer')
    ends = rows(text, 'mu4_cleanup_buffer_terminal')
    stops = rows(text, 'mu4_cleanup_buffer_stop')
    parents = rows(text, 'mu4_playback_parent_terminal')
    if any(len(items) != 1 for items in (scopes, events, ends, stops, parents)):
        raise ValueError('incomplete or repeated cleanup buffer capture')
    if scopes[0] != {'first': '0080', 'words': '65408', 'encoding': 'le16',
            'identity': 'debugger_Execute_hook', 'operands': 'context_only',
            'observer_writes': '0', 'audio_acceptance': '0'}:
        raise ValueError('cleanup buffer scope changed')
    event, end, stop, parent = events[0], ends[0], stops[0], parents[0]
    if (event.get('events') != '1' or end.get('events') != '1' or event.get('address') != '02956f'
            or event.get('identity') != 'debugger_Execute_hook' or event.get('operands') != 'context_only'
            or end.get('audio_acceptance') != '0' or stop.get('audio_acceptance') != '0'
            or stop.get('illegal') != '0' or float(stop['t']) < 9.86
            or any(parent.get(key) != value for key, value in {'illegal': '0',
                'first_session_complete': '1', 'overlay_verified': '1', 'opcode_mismatch': '0'}.items())):
        raise ValueError('invalid or unfinished cleanup buffer session')
    overlay = 'mu4_native_playback_overlay: PASS marker=aa99 records=3127 words=49843 checked_ram_words=49839 excluded_register_words=4 before_execution=1'
    residents = [line for line in text.splitlines() if 'mu4_playback_execute_entry: address=026d62' in line]
    position = text.index('mu4_cleanup_buffer:')
    if (text.count(overlay) != 1 or text.index(overlay) > position or len(residents) < 2
            or not text.index(residents[0]) < position < text.index(residents[1])):
        raise ValueError('cleanup snapshot outside verified first AA99 session')
    result = inspect_ram(data, *(int(event[key], 16) for key in ('object', 'cursor', 'limit')))
    if result['lifecycle'] != event['lifecycle'] or result['file_buffer_ref'] != event['file_buffer']:
        raise ValueError('snapshot context binding mismatch')
    result['cycles'] = int(event['cycles'])
    if reference is not None:
        result['projection_tail'] = compare_projection_tail(text, data, result, reference)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('ram', type=Path)
    parser.add_argument('--reference-aac', type=Path,
                        help='Compare reservoir plus high-byte-first buffer with the projection at the captured processing ordinal')
    parser.add_argument('--completion-log', type=Path,
                        help='Also check a separate passive refill-completion capture')
    args = parser.parse_args()
    try:
        result = check(args.log.read_text(), args.ram.read_bytes(),
                       args.reference_aac.read_bytes() if args.reference_aac else None)
        if args.completion_log:
            result['separate_completion_capture'] = check_completions(args.completion_log.read_text())
        print(json.dumps(result, indent=2))
    except (ValueError, KeyError, OSError) as error:
        print(f'cleanup buffer check: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
