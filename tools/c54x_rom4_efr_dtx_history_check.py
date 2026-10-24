"""Check original first-speech history transitions, not codec conformance."""
import argparse
from pathlib import Path
import re

ROW = re.compile(r'efr_dtx_history: pc=([0-9a-f]{4}) flags=([0-9a-f]{4}) history=([0-9a-f]{4}) state=([0-9a-f]{4}) st0=([0-9a-f]{4}) a=([0-9a-f]+)')
RECOVERY = re.compile(r'efr_dtx_recovery: pc=([0-9a-f]{4}) flags=([0-9a-f]{4}) history=([0-9a-f]{4}) bfi=([0-9a-f]{4}) sid=([0-9a-f]{4})')


def check_trace(text, frames=518, require_previous_muting=False):
    groups = []
    for line in text.splitlines():
        if not line.startswith('efr_dtx_history:'):
            continue
        match = ROW.fullmatch(line)
        if not match:
            raise ValueError('malformed DTX history observation')
        row = tuple(int(value, 16) for value in match.groups())
        if row[0] == 0xc78f:
            groups.append([])
        if not groups:
            raise ValueError('history observation precedes decision')
        groups[-1].append(row)
    if len(groups) != frames * 2:
        raise ValueError('missing original/replay history decisions')
    writes = 0
    observed_values = set()
    for index in range(0, len(groups), 2):
        group = groups[index]
        if group != groups[index + 1]:
            raise ValueError('history decision differs after restoration')
        entry, end = group[0], group[-1]
        selected = bool(entry[1] & 0x0400)
        expected = [0xc78f, 0xc793, 0xc79b, 0xc79d] if selected else [0xc78f, 0xc79d]
        if [row[0] for row in group] != expected:
            raise ValueError('first-speech branch differs')
        if any(row[4] & 0x1ff != 0x13 for row in group):
            raise ValueError('direct operand no longer addresses 09c0')
        if any(row[1] != entry[1] for row in group):
            raise ValueError('flags changed within history decision')
        if selected:
            value = int(bool(entry[1] & 0x0100))
            if group[2][5] != value or end[2] != value or end[3] != 5:
                raise ValueError('first-speech history/state publication differs')
            writes += 1
            observed_values.add(value)
        elif end[2:4] != entry[2:4]:
            raise ValueError('unselected history branch changed state')
    if require_previous_muting and observed_values != {0, 1}:
        raise ValueError('missing both previous-muting history alternatives')
    return writes


def check_lost_sid_failure(console, trace):
    """Characterize only the strict run's completed prefix, never promote it."""
    comparisons = re.findall(r'^ROM4 EFR lost SID decoder comparison: frame=(\d+) mismatches=(\d+)$', console, re.M)
    if [(int(frame), int(count)) for frame, count in comparisons] != [(i, 86 if i == 97 else 0) for i in range(98)]:
        raise ValueError('lost-SID strict failure prefix differs')
    failure = 'Fatal error: TMS320C54x core conformance: native EFR decoder matches independent ETSI PCM'
    if console.splitlines()[-1:] != [failure] or 'lost SID decoder: PASS' in console:
        raise ValueError('missing strict nonconformance termination')
    if check_trace(trace, frames=98) != 1:
        raise ValueError('unexpected first-speech prefix transition count')
    rows = []
    for line in trace.splitlines():
        if not line.startswith('efr_dtx_recovery:'):
            continue
        match = RECOVERY.fullmatch(line)
        if not match:
            raise ValueError('malformed recovery observation')
        rows.append(tuple(int(value, 16) for value in match.groups()))
    if len(rows) != 98 * 4:
        raise ValueError('incomplete recovery original/replay prefix')
    for frame in range(98):
        before, after, replay_before, replay_after = rows[frame * 4:frame * 4 + 4]
        if before != replay_before or after != replay_after or (before[0], after[0]) != (0x28b0, 0x28f7):
            raise ValueError('recovery path differs after restoration')
    if rows[-4:-2] != [(0x28b0, 0x2c0, 1, 0, 0), (0x28f7, 0x240, 0, 0, 0)]:
        raise ValueError('original patch did not clear prior DTX muting/history')
    decisions = [tuple(int(value, 16) for value in match.groups())
                 for match in ROW.finditer(trace) if int(match[1], 16) == 0xc78f]
    if any(row[1] != 0x401 for row in decisions[-2:]):
        raise ValueError('native first speech unexpectedly preserves previous muting')
    return dict(compared_frames=98, exact_prefix_frames=97, first_difference_frame=97,
                first_difference_samples=86, reference_first_speech_flags='0501',
                native_first_speech_flags='0401', conformance_claim=False,
                complete_fixture_claim=False, handset_audio_claim=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--require-previous-muting', action='store_true')
    parser.add_argument('--lost-sid-failure-console', type=Path, help='characterize the known strict failure prefix, not a pass')
    args = parser.parse_args()
    try:
        if args.lost_sid_failure_console:
            if args.require_previous_muting:
                parser.error('failure characterization cannot require the unexecuted alternative')
            report = check_lost_sid_failure(args.lost_sid_failure_console.read_text(), args.log.read_text())
            print(f'CHARACTERIZED: {report}')
            return
        writes = check_trace(args.log.read_text(), require_previous_muting=args.require_previous_muting)
    except ValueError as error:
        parser.exit(1, f'FAIL: {error}\n')
    print(f'PASS: 518 original/replay frames; first-speech writes={writes}; no codec conformance claim')


if __name__ == '__main__':
    main()
