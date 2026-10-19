#!/usr/bin/env python3
"""Validate mathematical RF-port controls; never claims GSM acquisition."""
import argparse
from pathlib import Path
import re
import sys


SUMMARY = re.compile(r"rom4_rf_score_summary: mode=(zero|dc|tone) reads=(\d+) results=(\d+) nonzero=(\d+) maximum=([0-9a-f]{10}) second=(\d+) accepted=(\d+) acquisition_claim=0")
WINDOW = re.compile(r"rom4_rf_window_summary: writes=(\d+) transitions=(\d+) reads_on=(\d+) reads_off=(\d+) physical_gate_claim=0")
WINDOW_MODE = re.compile(r"rom4_rf_window_mode: mode=([0-9a-f]{4}) reads_on=(\d+) reads_off=(\d+)")
WINDOW_STATE = re.compile(r"rom4_rf_window_state: mode=([0-9a-f]{4}) control=([0-9a-f]{4}) reads=(\d+) aged=(\d+) minimum_us=(\d+) maximum_us=(\d+)")


def observe(text):
    """Decode a unique observation, without asserting that the score passed."""
    summaries = SUMMARY.findall(text)
    if len(summaries) != 1:
        raise ValueError("missing or duplicate score summary")
    mode, reads, results, nonzero, maximum, second, accepted = summaries[0]
    reads, results, nonzero, second, accepted = map(int, (reads, results, nonzero, second, accepted))
    maximum = int(maximum, 16)
    return dict(mode=mode, reads=reads, results=results, nonzero=nonzero,
                maximum=maximum, second=second, accepted=accepted)


def observe_execution(text):
    """Count execution entries; fetch-summary score values are not used."""
    marker = 'rom4_rf_execution_probe: points=0ad1,0add,0ae4 phase=instruction_entry'
    if text.count(marker) != 1:
        raise ValueError('missing or duplicate execution-owned RF probe')
    source = observe(text)
    sample = dict(mode=source['mode'], reads=source['reads'], results=0,
                  nonzero=0, maximum=0, second=0, accepted=0)
    phase = 0
    rows = [line.split('rom4_rf_execute: ', 1)[1] for line in text.splitlines()
            if 'rom4_rf_execute: ' in line]
    for row in rows:
        match = re.fullmatch(r'pc=(0ad1|0add|0ae4) a=([0-9a-f]{10})', row)
        if not match:
            raise ValueError('malformed execution-owned RF record')
        pc, value = match[1], int(match[2], 16)
        if pc == '0ad1':
            sample['results'] += 1
            sample['nonzero'] += bool(value)
            sample['maximum'] = max(sample['maximum'], value)
            phase = 1
        elif pc == '0add' and phase == 1:
            sample['second'] += 1
            phase = 2
        elif pc == '0ae4' and phase == 2:
            sample['accepted'] += 1
            phase = 3
        else:
            raise ValueError('unordered execution-owned RF continuation')
    if not rows:
        raise ValueError('missing execution-owned RF entries')
    return sample


def check_sample(sample):
    mode, reads, results, nonzero, maximum, second, accepted = (
        sample[key] for key in ('mode', 'reads', 'results', 'nonzero', 'maximum', 'second', 'accepted'))
    if reads < 1000 or results < 1000 or not 0 <= accepted <= second <= results or not 0 <= nonzero <= results:
        raise ValueError("insufficient or inconsistent score observations")
    if mode in ("zero", "dc") and (nonzero or maximum or second or accepted):
        raise ValueError("silent/DC control unexpectedly advances the score")
    if mode == "tone" and (nonzero != results or maximum <= 0x574 or second == 0 or accepted == 0):
        raise ValueError("rotating input did not reach native accepted-value store")
    return mode, results, accepted


def check(text):
    return check_sample(observe(text))


def check_execution(text):
    return check_sample(observe_execution(text))


def check_window(text):
    """Validate passive bit-0/read accounting, not the claimed physical gate."""
    summaries = WINDOW.findall(text)
    if len(summaries) != 1:
        raise ValueError('missing or duplicate RF window summary')
    writes, transitions, reads_on, reads_off = map(int, summaries[0])
    modes = {}
    for mode, on, off in WINDOW_MODE.findall(text):
        mode = int(mode, 16)
        if mode in modes:
            raise ValueError('duplicate RF window mode')
        modes[mode] = dict(on=int(on), off=int(off))
    reads = observe(text)['reads']
    if not writes or not reads or transitions > writes or reads_on + reads_off != reads:
        raise ValueError('inconsistent RF window totals')
    if sum(case['on'] for case in modes.values()) != reads_on or sum(case['off'] for case in modes.values()) != reads_off:
        raise ValueError('RF window mode accounting differs from totals')
    states, accounted = {}, {mode: dict(on=0, off=0) for mode in modes}
    for mode, control, count, aged, minimum, maximum in WINDOW_STATE.findall(text):
        mode, control = int(mode, 16), int(control, 16)
        count, aged, minimum, maximum = map(int, (count, aged, minimum, maximum))
        key = (mode, control)
        if key in states or mode not in modes or not count or aged > count or minimum > maximum:
            raise ValueError('invalid RF window control/age observation')
        if (control & 1 and aged) or (not aged and (minimum or maximum)):
            raise ValueError('RF window age requires a clear-bit read after an observed falling edge')
        states[key] = dict(reads=count, aged=aged, minimum_us=minimum, maximum_us=maximum)
        accounted[mode]['on' if control & 1 else 'off'] += count
    if accounted != modes:
        raise ValueError('RF window control accounting differs from mode totals')
    return dict(writes=writes, transitions=transitions, reads_on=reads_on,
                reads_off=reads_off, modes=modes, states={f'{mode:04x}:{control:04x}': value for (mode, control), value in states.items()}, physical_gate_claim=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument('--window-only', action='store_true', help='check passive window-bit accounting, not score acceptance')
    args = parser.parse_args()
    try:
        text = args.log.read_text()
        if args.window_only:
            window = check_window(text)
            print(f"ROM4 RF window observation: PASS on={window['reads_on']} off={window['reads_off']} modes={len(window['modes'])} physical_gate_claim=0")
            return 0
        mode, results, accepted = check(text)
    except (OSError, ValueError) as error:
        print(f"ROM4 RF score: FAIL: {error}", file=sys.stderr)
        return 1
    print(f"ROM4 RF score: PASS mode={mode} results={results} accepted={accepted} acquisition_claim=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
