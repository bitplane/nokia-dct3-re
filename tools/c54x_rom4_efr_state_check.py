"""Compare passive native/reference EFR boundaries, without a conformance claim."""
import argparse
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.c54x_rom4_efr_loss_check import lost


WIDTHS = dict(R2=10, LSF=10, RAW=160, LPC=44)
STATE = re.compile(r'EFRSTATE frame=(\d+) kind=(R2|LSF|RAW|LPC) values=([0-9a-f]+)')
GAIN = re.compile(r'EFRGAIN frame=(\d+) kind=(pitch|code) subframe=(\d+) value=([0-9a-f]{4})')
POLICY = re.compile(r'EFR(?:POLICY|CANDIDATE) frame=(\d+) subframe=(\d+) prev_bf=([0-9a-f]{4}) previous_gain=([0-9a-f]{4}) value=([0-9a-f]{4})')
FLAG = re.compile(r'EFRFLAG frame=(\d+) stage=(before|after) value=([0-9a-f]{4})')


def parse_flags(text):
    rows = []
    for line in text.splitlines():
        if line.startswith('EFRFLAG '):
            match = FLAG.fullmatch(line)
            if not match:
                raise ValueError('malformed EFR recovery-flag observation')
            rows.append((int(match[1]), match[2], int(match[3], 16)))
    expected = [(frame, stage, value) for frame in (9, 35, 88, 136)
                for stage, value in (('before', 1), ('after', 0))]
    if rows != expected:
        raise ValueError('incomplete or changed original EFR recovery-flag clear')
    return rows


def parse_policy(text, candidate=False):
    records = {}
    prefix = 'EFRCANDIDATE ' if candidate else 'EFRPOLICY '
    for line in text.splitlines():
        if not line.startswith(prefix):
            continue
        match = POLICY.fullmatch(line)
        if not match:
            raise ValueError('malformed EFR code-gain policy observation')
        key = (int(match[1]), int(match[2]))
        if key in records:
            raise ValueError('duplicate EFR code-gain policy observation')
        records[key] = tuple(int(match[index], 16) for index in (3, 4, 5))
    expected = {(frame, subframe) for frame in range(163) if not lost(frame) for subframe in range(4)}
    if records.keys() != expected:
        raise ValueError('incomplete or out-of-domain EFR code-gain policy observations')
    return records


def parse(text, frames=163):
    states, gains, events = {}, {}, []
    for line in text.splitlines():
        if line.startswith('EFRSTATE '):
            match = STATE.fullmatch(line)
            if not match or len(match[3]) != 4 * WIDTHS[match[2]]:
                raise ValueError('malformed EFR state observation')
            key = (int(match[1]), match[2])
            if key in states:
                raise ValueError('duplicate EFR state observation')
            states[key] = tuple(int(match[3][i:i + 4], 16) for i in range(0, len(match[3]), 4))
            events.append(key)
        if line.startswith('EFRGAIN '):
            match = GAIN.fullmatch(line)
            if not match:
                raise ValueError('malformed EFR gain observation')
            key = (int(match[1]), match[2], int(match[3]))
            if key in gains:
                raise ValueError('duplicate EFR gain observation')
            gains[key] = int(match[4], 16)
            events.append(key)
    expected_states = {(frame, kind) for frame in range(frames) for kind in WIDTHS}
    expected_gains = {(frame, kind, subframe) for frame in range(frames)
                      for kind in ('pitch', 'code') for subframe in range(4)}
    if states.keys() != expected_states or gains.keys() != expected_gains:
        raise ValueError('incomplete or out-of-domain EFR state/gain coverage')
    expected_events = []
    for frame in range(frames):
        expected_events.extend([(frame, 'R2'), (frame, 'LSF')])
        expected_events.extend((frame, kind, subframe) for subframe in range(4) for kind in ('pitch', 'code'))
        expected_events.extend([(frame, 'RAW'), (frame, 'LPC')])
    if events != expected_events:
        raise ValueError('unordered EFR state/gain observation boundaries')
    return states, gains


def compare(native, reference):
    flags = parse_flags(native)
    actual, actual_gains = parse(native)
    expected, expected_gains = parse(reference)
    frames = []
    for frame in range(163):
        fields = {kind: sum(a != b for a, b in zip(actual[frame, kind], expected[frame, kind]))
                  for kind in WIDTHS}
        fields.update({kind: sum(actual_gains[frame, kind, sub] != expected_gains[frame, kind, sub]
                                for sub in range(4)) for kind in ('pitch', 'code')})
        frames.append(dict(frame=frame, differences=fields))
    first = {kind: next((row['frame'] for row in frames if row['differences'][kind]), None)
             for kind in (*WIDTHS, 'pitch', 'code')}
    candidates, policy = parse_policy(native, True), parse_policy(reference)
    recovery = []
    signed = lambda word: word if word < 32768 else word - 65536
    for key, (native_prev, native_previous_gain, candidate) in candidates.items():
        reference_prev, reference_previous_gain, result = policy[key]
        frame, subframe = key
        native_result = actual_gains[frame, 'code', subframe]
        if result != expected_gains[frame, 'code', subframe]:
            raise ValueError('EFR reference policy disagrees with the published code gain')
        native_expected = min(signed(candidate), signed(native_previous_gain)) if native_prev else signed(candidate)
        if signed(native_result) != native_expected:
            raise ValueError(f'native EFR clamp candidate does not explain its published gain at {key}: '
                             f'candidate={signed(candidate)} previous={signed(native_previous_gain)} '
                             f'flag={native_prev} result={signed(native_result)}')
        if reference_prev:
            recovery.append(dict(frame=frame, subframe=subframe, native_previous_bad=native_prev,
                                 reference_previous_bad=reference_prev, native_gain=signed(native_result),
                                 reference_gain=signed(result), reference_previous_gain=signed(reference_previous_gain),
                                 reference_clamp_matches=min(signed(candidate), signed(reference_previous_gain)) == signed(result)))
    return dict(schema=1, status='compared', frames=163, first_differing_frame=first,
                conformance_claim=False, handset_audio_claim=False, records=frames,
                recovery_code_gain=recovery, original_flag_clears=len(flags) // 2)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('native', type=Path)
    parser.add_argument('reference', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        result = compare(args.native.read_text(), args.reference.read_text())
    except (OSError, ValueError) as error:
        print(f'EFR state comparison rejected: {error}', file=sys.stderr)
        return 1
    text = json.dumps(result, indent=2) + '\n'
    if args.output:
        args.output.write_text(text)
    else:
        print(text, end='')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
