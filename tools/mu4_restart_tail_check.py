"""Bind finalized cleanup RAM to the native resident restart selection."""
import argparse
from decimal import Decimal
import hashlib
import json
from pathlib import Path
import sys

if __package__:
    from .mu4_cleanup_buffer_check import check as check_buffer
    from .mu4_context_contract import decode_resident_cleanup_restart, resident_cleanup_restart
    from .mu4_clock_contract import SOURCE_SHA256
    from .mu4_playback_frame_check import rows
    from .mu4_serial_epoch_check import codec_control_history
    from .noki5510_a00_inventory import uploaded_word_image
else:
    from mu4_cleanup_buffer_check import check as check_buffer
    from mu4_context_contract import decode_resident_cleanup_restart, resident_cleanup_restart
    from mu4_clock_contract import SOURCE_SHA256
    from mu4_playback_frame_check import rows
    from mu4_serial_epoch_check import codec_control_history
    from noki5510_a00_inventory import uploaded_word_image


def check_restart(text, snapshot):
    scopes, events, ends = (rows(text, marker) for marker in
                           ('mu4_restart_scope', 'mu4_restart_context', 'mu4_restart_terminal'))
    if (len(scopes) != 1 or scopes[0] != {'identity': 'debugger_Execute_hook',
            'operands': 'context_only', 'observer_writes': '0', 'audio_acceptance': '0'}
            or len(ends) != 1 or not events
            or len(events) != int(ends[0]['events']) or len(events) >= int(ends[0]['cap'])
            or ends[0].get('audio_acceptance') != '0'):
        raise ValueError('missing, capped or changed restart capture')
    selection = snapshot['resident_selection']
    model = resident_cleanup_restart(selection['continuation'], selection['mode'],
                                     selection['track'], selection['count'])
    if snapshot['lifecycle'] != '0004':
        raise ValueError('restart snapshot not taken in playback lifecycle')
    increment = (selection['track'] + 1) & 0xffff
    advancing = model['index_advanced']
    wrap = advancing and model['track'] != increment
    addresses = (['03a98f', '03a9a2'] + (['03a9a5'] if wrap else []) + ['03a9af']
                 if advancing else ['03a98f', '03a9aa', '03a9af'])
    if [row['address'] for row in events] != addresses:
        raise ValueError('restart successors disagree with source model')
    previous = snapshot['cycles']
    for ordinal, event in enumerate(events, 1):
        cycles = int(event['cycles'])
        if (int(event['ordinal']) != ordinal or cycles <= previous
                or event.get('identity') != 'debugger_Execute_hook'
                or event.get('operands') != 'context_only'):
            raise ValueError('restart context identity or chronology changed')
        previous = cycles
        expected_track = (selection['track'] if ordinal == 1 or not advancing else
                          model['track'] if event['address'] == '03a9af' else increment)
        expected = {'mode': selection['mode'], 'count': selection['count'],
                    'track': expected_track, 'lifecycle': 5,
                    'continuation': model['continuation']}
        if any(int(event[key], 16) != value for key, value in expected.items()):
            raise ValueError('restart state differs from captured input/source model')
        if event['address'] == '03a9a2' and int(event['ar2'], 16) != (increment-selection['count']) & 0xffff:
            raise ValueError('restart wrap predicate context differs')
    return {'selection_at_cleanup': selection, 'restart_contexts': len(events),
            'source_model': model, 'wrap_observed': wrap,
            'scope': 'Cleanup snapshot and ordered Execute-entry contexts match resident model; '
                     'not retained instruction operands, intended repeat settings, tail validity or audio acceptance'}


def check_eof_mute(text, snapshot):
    control = snapshot['file_control']
    if (not 0 < control['descriptor_bytes'] <= 0xffffffff
            or control['descriptor_bytes'] % 2
            or control['counter'] != control['descriptor_bytes']
            or control['policy'] != 1 or control['short_input'] != 1
            or not snapshot['pending_refill']):
        raise ValueError('cleanup snapshot differs from default short-input path')
    writes = [row for row in rows(text, 'mu4_playback_lifecycle_write')
              if row['address'] == '373e']
    cleanup = [row for row in writes if row['value'] == '0005']
    if len(cleanup) != 1:
        raise ValueError('missing unique cleanup lifecycle')
    def nanoseconds(row):
        value = Decimal(row['t'])
        if not value.is_finite() or value < 0:
            raise ValueError('invalid lifecycle time')
        return value * 1_000_000_000
    cleanup_ns = nanoseconds(cleanup[0])
    restart = [row for row in writes if row['value'] == '0004'
               and nanoseconds(row) > cleanup_ns]
    if len(restart) != 1:
        raise ValueError('missing unique restarted playback lifecycle')
    restart_ns = nanoseconds(restart[0])
    history = codec_control_history(text)
    def muted(registers):
        return bool(registers[5] & 8) and all((registers[i] & 0x7f) < 0x30 for i in (2, 3))
    def enabled(registers):
        return (bool(registers[9] & 1) and not registers[6] & 0x98
                and bool(registers[4] & 0x10) and not registers[5] & 8
                and all((registers[i] & 0x7f) >= 0x30 for i in (2, 3)))
    before = [ns for ns, registers in history if ns < cleanup_ns and enabled(registers)]
    silence = [ns for ns, registers in history
               if cleanup_ns < ns < restart_ns and muted(registers)]
    after = [ns for ns, registers in history if ns > restart_ns and enabled(registers)]
    if not before or not silence or not after:
        raise ValueError('missing enabled/muted/re-enabled codec control sequence')
    return {'file_control': control, 'pending_refill_words': snapshot['pending_refill'],
            'cleanup_ns': str(cleanup_ns), 'mute_observed_ns': silence[0],
            'restart_ns': str(restart_ns), 'reenabled_ns': after[0],
            'scope': 'Original software short-input state and reconstructed mute controls; '
                     'not analog silence, intended sample count or correct end-of-track policy'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--source-a00', type=Path, required=True)
    parser.add_argument('--reference-aac', type=Path, required=True)
    args = parser.parse_args()
    try:
        source = args.source_a00.read_bytes()
        if hashlib.sha256(source).hexdigest() != SOURCE_SHA256:
            raise ValueError('expected original R060 source')
        contract = decode_resident_cleanup_restart(uploaded_word_image(
            bytes.fromhex(source.decode('ascii')), 'aa22'))
        text = (args.directory/'error.log').read_text()
        snapshot = check_buffer(text, (args.directory/'mu4_cleanup_input.le16').read_bytes(),
                                args.reference_aac.read_bytes())
        print(json.dumps({'restart': check_restart(text, snapshot),
                          'eof_mute': check_eof_mute(text, snapshot),
                          'cleanup_buffer': snapshot, 'source_contract': contract}, indent=2))
    except (ValueError, KeyError, OSError) as error:
        print(f'restart/tail check: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
