"""Check the captured ROM6 counter retry timing, not the fitted handset clock."""
import argparse
import json
from pathlib import Path
import re

PATTERN = re.compile(
    r'rom6_reset_policy_counter: sample=(\d+) t=([\d.]+) '
    r'value=([0-9a-f]{4}) pc=([0-9a-f]{6}) observer_writes=0')


def check(text, *, reference=False):
    rows = [(int(n), float(t), int(v, 16), int(pc, 16))
            for n, t, v, pc in PATTERN.findall(text)]
    if [row[0] for row in rows] != list(range(1, 65)):
        raise ValueError('requires exactly 64 ordered counter samples')
    clock = 45_500_000 if reference else 13_000_000
    clocks = re.findall(r'native_dsp_terminal_timing: released=[01] clock=(\d+)', text)
    if clocks != [str(clock)]:
        raise ValueError('terminal configured clock does not match the diagnostic')
    retry_pairs = equal_pairs = 0
    for previous, current in zip(rows, rows[1:]):
        _, t0, value0, pc0 = previous
        _, t1, value1, pc1 = current
        if t1 <= t0 or pc0 not in (0x3eea, 0x3ef0) or pc1 not in (0x3eea, 0x3ef0):
            raise ValueError('counter sample time or consumer changed')
        if pc0 == pc1 == 0x3ef0:
            retry_pairs += 1
            if abs((t1 - t0) * clock - 14) > 0.001:
                raise ValueError('retry interval is not 14 configured DSP clocks')
        equal_pairs += value0 == value1
    if reference:
        if not equal_pairs or not retry_pairs:
            raise ValueError('reference lacks stable reads or an observed retry')
    elif (retry_pairs != 63 or equal_pairs
          or any(not 1 <= (b[2] - a[2]) % 5000 <= 2 for a, b in zip(rows, rows[1:]))):
        raise ValueError('13 MHz capture does not show the continuous unequal retry')
    return {'samples': len(rows), 'configured_dsp_hz': clock,
            'assumed_counter_reference_hz': 13_000_000, 'assumed_counter_divider': 12,
            'retry_pairs': retry_pairs, 'equal_adjacent_values': equal_pairs,
            'conditional_retry_minimum_hz_exclusive': 14 * 13_000_000 / 12,
            'fitted_clock_claim': False, 'whole_phone_acceptance': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', type=Path)
    parser.add_argument('--reference', action='store_true')
    args = parser.parse_args()
    try:
        result = check(args.trace.read_text(), reference=args.reference)
    except (OSError, ValueError) as error:
        parser.exit(1, f'ROM6 counter diagnostic failed: {error}\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
