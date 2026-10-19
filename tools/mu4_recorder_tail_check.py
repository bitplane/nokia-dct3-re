"""Compare stop-time pending RAM with native transform and stored-file evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

if __package__:
    from .mu4_clock_contract import SOURCE_SHA256
    from .noki5510_a00_inventory import uploaded_word_image
    from .mu4_recorder_full_transform_check import records, verify_file
    from .mu4_recorder_transform_check import word_list
    from .mu4_recorder_media_inventory import adts_header
else:
    from mu4_clock_contract import SOURCE_SHA256
    from noki5510_a00_inventory import uploaded_word_image
    from mu4_recorder_full_transform_check import records, verify_file
    from mu4_recorder_transform_check import word_list
    from mu4_recorder_media_inventory import adts_header


SOURCE_RANGES = (
    (0x389cf, 0x38a30, '3cf7e0102f4ce1590ea2b66f9d5eb3321d81d81970553c9e10a47594590f16f8'),
    (0x3e30c, 0x3e332, 'af117d8d539ea50ab898ee26d1369bab98f37202bdf336820c16edbc2e6a051f'),
    (0x3e76f, 0x3e7f9, '8809c82c5cebea077eadee73f9df6155d6801800b91e6cf2f5a0aea088e13b22'),
    (0x3e918, 0x3e92b, '8ae5379cfb94db36a94dfde1bd0cadd1895f06613b2c9433b53dd360919e4657'),
    (0x38ff7, 0x39001, '8d084b9df84f2471410aa22c0d58bada6c0657f11bd31105fc2f09fbbff1ca32'),
    (0x567e, 0x569b, 'b55c341e1939fe35c5825f1ac5b34183566a6521f51d5b59e567d1724beb8df9'),
    (0x2dcee, 0x2dcf9, '5d766a4446216c903def4aee270de3b48523ea57f942e939468d7583c43776ab'),
    (0x70d6, 0x70e1, '9815f8f2aa7eb3f69bdd3287160dda255a3e0cb1764ed3e0f4679bf7b702f02b'),
)


def adapter_methods(words):
    """Selected original table, not a recovered universal interface ABI."""
    # Slot zero is not classified as a callable entry.
    slots = {slot: (words[0x567e+2*slot]<<16)|words[0x567f+2*slot]
             for slot in range(1,15)}
    methods = {slot: target for slot,target in slots.items() if target}
    if methods != {6: 0x3e6f5, 14: 0x3e76f}:
        raise ValueError('original adapter method inventory changed')
    return dict(table='567e', classified_slots=list(range(1,15)),
                nonzero_methods={f'{slot:02x}': f'{target:06x}' for slot,target in methods.items()},
                stop_slot='0c', stop_target='000000',
                scope='Original slots 1..14 only; slot zero and dynamic table replacements not closed')


def source_contract(source):
    if hashlib.sha256(source).hexdigest() != SOURCE_SHA256:
        raise ValueError('expected original R060 InitData source')
    words = uploaded_word_image(bytes.fromhex(source.decode('ascii')), 'aa88')
    for start, end, expected in SOURCE_RANGES:
        payload = b''.join(words[address].to_bytes(2, 'big') for address in range(start, end+1))
        if hashlib.sha256(payload).hexdigest() != expected:
            raise ValueError('recorder tail source changed')
    return dict(pinned_words=sum(end-start+1 for start,end,_ in SOURCE_RANGES),
                source_sha256=SOURCE_SHA256,
                adapter=adapter_methods(words),
                carry_literal_sites=[f'{address:06x}' for address,value in sorted(words.items()) if value == 0xfd70],
                carry_census_scope='Literal-word references only, not an exhaustive indirect-writer proof',
                scope='Buffer compaction/reset, selected stop tail, dispatch, adapter, forwarder and initial configuration; no flush guarantee')


def pending_tail(text, encoded, plain, persisted):
    requests = records(text, 'mu4_recorder_tail_request')
    terminals = records(text, 'mu4_recorder_tail_terminal')
    if len(requests) != 1 or len(terminals) != 1:
        raise ValueError('expected one stop-time capture and terminal')
    row, terminal = requests[0], terminals[0]
    if row['ordinal'] != '1' or terminal['captures'] != '1' or terminal['capped'] != '0' or terminal['illegal'] != '0':
        raise ValueError('incomplete or invalid tail observation')
    ed, ee, held = (int(row[key], 16) for key in ('ed', 'ee', 'held'))
    if not 0 <= ee <= ed <= 760 or not 0 < held < 16:
        raise ValueError('tail counts outside selected adapter contract')
    if row['finish'] != '0000,0000' or row['process'] != '0003,e76f':
        raise ValueError('selected runtime method table changed')
    buffer = b''.join(word.to_bytes(2, 'big') for word in word_list(row['buffer'], ed))
    carry = b''.join(word.to_bytes(2, 'big') for word in word_list(row['carry'], held))
    if len(encoded) != len(plain) or not 0 < persisted < len(encoded) or buffer != encoded[persisted:]:
        raise ValueError('stop-time buffer does not equal the native unpersisted suffix')
    if ee != ed or terminal['ed'] != '0000' or terminal['ee'] != '0000':
        raise ValueError('selected pending-buffer reset not observed')
    if not 1 < float(row['t']) < float(terminal['t']):
        raise ValueError('invalid stop-time capture chronology')
    captured = plain+carry
    at, packets = 0, 0
    while at < len(captured):
        header = adts_header(captured, at)
        if not header or not header['frame_within_file']:
            raise ValueError('captured carry does not complete the packet chain')
        at += header['frame_bytes']
        packets += 1
    return dict(pending_transformed_bytes=len(buffer), pending_untransformed_bytes=len(carry),
                stop_buffer_matches_native_suffix=True, captured_packets=packets,
                captured_bytes=len(captured), captured_sha256=hashlib.sha256(captured).hexdigest(),
                stored_tail_complete=False, native_flush_validated=False,
                scope='Host-only concatenation of observed native plaintext and pending RAM; not committed media'), captured


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', type=Path)
    parser.add_argument('stored_file', type=Path)
    parser.add_argument('--source-a00', required=True, type=Path)
    parser.add_argument('--output-captured-aac', type=Path)
    args = parser.parse_args()
    try:
        source = source_contract(args.source_a00.read_bytes())
        text = args.trace.read_text()
        transform, plain = verify_file(text, args.stored_file.read_bytes(), inventory_divergence=True)
        windows = [row for row in records(text, 'mu4_record_full_block') if row['domain'] == 'aligned32']
        encoded = b''.join(b''.join(word.to_bytes(2, 'little') for word in word_list(row['output'], 2))+
                           bytes.fromhex(row['plain'])[4:] for row in windows)
        tail, captured = pending_tail(text, encoded, plain, transform['persisted_native_prefix_bytes'])
        if args.output_captured_aac:
            with args.output_captured_aac.open('xb') as output:
                output.write(captured)
        print(json.dumps(dict(source=source, transform=transform, tail=tail), indent=2))
    except (OSError, ValueError, KeyError) as error:
        print(f'Recorder tail: FAIL: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
