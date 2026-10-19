"""Validate complete native transform calls without modifying emulation inputs."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sys

if __package__:
    from .mu4_context_contract import contract, reference_transform_block
    from .mu4_recorder_transform_check import word_list
    from .mu4_playback_frame_check import rows as records
else:
    from mu4_context_contract import contract, reference_transform_block
    from mu4_recorder_transform_check import word_list
    from mu4_playback_frame_check import rows as records


def verify(text):
    overlay = ('mu4_native_recorder_overlay: PASS marker=aa88 records=4622 words=73766 '
               'checked_ram_words=73762 excluded_register_words=4 before_execution=1')
    if text.count(overlay) != 1 or text.find('mu4_record_full_block:') < text.find(overlay):
        raise ValueError('missing preceding verified original upload')
    terminals = records(text, 'mu4_record_full_terminal')
    if len(terminals) != 1:
        raise ValueError('expected one complete terminal')
    terminal = terminals[0]
    if records(text, 'mu4_record_full_unadmitted'):
        raise ValueError('unadmitted calls in capture')
    for key in ('unadmitted', 'bad', 'dropped', 'active', 'pending_write'):
        if int(terminal[key]):
            raise ValueError('incomplete capture: ' + key)
    rows = records(text, 'mu4_record_full_block')
    count = int(terminal['entries'])
    if not count or int(terminal['returns']) != count or len(rows) != count:
        raise ValueError('entry/return coverage mismatch')
    callers, domains, states = Counter(), Counter(), set()
    caller_states = {}
    previous = 0.0
    for ordinal, row in enumerate(rows, 1):
        if int(row['ordinal']) != ordinal:
            raise ValueError('reordered or missing call')
        if row['identity'] != 'Execute_entry_and_bus' or row['reads'] != '3' or row['writes'] != '3':
            raise ValueError('missing actual operand/store identity')
        if int(row['state_other_writes']) or int(row['source_writes']):
            raise ValueError('input state changed during call')
        start, end = float(row['start']), float(row['end'])
        if not previous <= start < end <= float(terminal['stop']):
            raise ValueError('invalid call chronology')
        previous = end
        state = word_list(row['state'], 82)
        inputs = word_list(row['input'], 2)
        outputs = word_list(row['output'], 2)
        if tuple(outputs) != reference_transform_block(*inputs, state, reverse=False):
            raise ValueError('native transform differs from independent model')
        if row['domain'] == 'aligned32':
            plain = bytes.fromhex(row['plain'])
            if len(plain) != 32 or inputs != [int.from_bytes(plain[i:i+2], 'little') for i in (0, 2)]:
                raise ValueError('aligned source does not match actual inputs')
            args = word_list(row['arguments'], 6)
            outer = (int(row['sp'], 16) + 2) & 0xffff
            if (row['caller'] != '03ee95' or args[:2] != [0, 32] or args[3:] != [0, 28, 4]
                    or word_list(row['pointers'], 2) != [(outer+16)&0xffff, (outer+17)&0xffff]
                    or int(row['position'], 16)):
                raise ValueError('aligned caller ABI mismatch')
        elif row['domain'] != 'other':
            raise ValueError('unknown transform domain')
        callers[row['caller']] += 1
        domains[row['domain']] += 1
        states.add(tuple(state))
        caller_states.setdefault(row['caller'], set()).add(tuple(state))
    reported = records(text, 'mu4_record_full_caller')
    if len(reported) != len(callers) or {r['caller']: int(r['calls']) for r in reported} != dict(callers):
        raise ValueError('caller census mismatch')
    return dict(calls=count, callers=dict(callers), domains=dict(domains),
                state_variants=len(states), native_transform_validated=True,
                caller_state_variants={caller: len(values) for caller, values in caller_states.items()},
                stored_file_correspondence_validated=False, codec_conformance_validated=False,
                audio_acceptance=False)


def verify_file(text, saved, *, inventory_divergence=False):
    """Compare actual call outputs at one fixed, uniquely anchored file origin."""
    report = verify(text)
    windows = [row for row in records(text, 'mu4_record_full_block') if row['domain'] == 'aligned32']
    if not windows:
        raise ValueError('no aligned recording windows')
    origin = None
    plaintext = bytearray()
    encoded = bytearray()
    for index, row in enumerate(windows):
        if int(row['cursor'], 16) != index*32:
            raise ValueError('recording cursor is not one contiguous stream')
        plain = bytes.fromhex(row['plain'])
        native = b''.join(word.to_bytes(2, 'little') for word in word_list(row['output'], 2))+plain[4:]
        if origin is None:
            origin = saved.find(native)
            if origin < 0 or saved.find(native, origin+1) >= 0:
                raise ValueError('first native window has no unique file anchor')
        plaintext.extend(plain)
        encoded.extend(native)
    stored = saved[origin:]
    common = min(len(stored), len(encoded))
    mismatch = next((index for index in range(common) if stored[index] != encoded[index]), None)
    if not inventory_divergence and (mismatch is not None or len(stored) < len(encoded)):
        raise ValueError('actual native output differs from or exceeds stored file')
    prefix = common if mismatch is None else mismatch
    report.update(stored_file_correspondence_validated=mismatch is None and len(stored) >= len(encoded), file_origin=origin,
                  native_windows=len(windows), native_window_bytes=len(plaintext),
                  persisted_native_prefix_bytes=prefix, first_stored_mismatch=mismatch,
                  stored_bytes_after_origin=len(stored), native_bytes_beyond_stored_extent=max(0,len(encoded)-len(stored)),
                  bytes_after_windows=max(0,len(stored)-len(encoded)),
                  plaintext_sha256=hashlib.sha256(plaintext).hexdigest(),
                  stored_file_sha256=hashlib.sha256(saved).hexdigest(),
                  whole_file_validated=False, tail_completion_validated=False)
    return report, bytes(plaintext)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', type=Path)
    parser.add_argument('--source-a00', required=True, type=Path)
    parser.add_argument('--stored-file', type=Path)
    parser.add_argument('--output-native-plain', type=Path)
    parser.add_argument('--inventory-divergence', action='store_true',
                        help='report, rather than accept, a stored-tail mismatch or shorter file')
    args = parser.parse_args()
    try:
        source = args.source_a00.read_bytes()
        contract(source)
        text = args.trace.read_text()
        if (args.output_native_plain or args.inventory_divergence) and not args.stored_file:
            raise ValueError('plaintext export requires independent stored-file comparison')
        if args.stored_file:
            report, plain = verify_file(text, args.stored_file.read_bytes(), inventory_divergence=args.inventory_divergence)
            if args.output_native_plain:
                with args.output_native_plain.open('xb') as output:
                    output.write(plain)
        else:
            report = verify(text)
        report['source_a00_sha256'] = hashlib.sha256(source).hexdigest()
        print(json.dumps(report, indent=2))
    except (ValueError, KeyError, OSError) as error:
        print(f'Full recorder transform: FAIL: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
