#!/usr/bin/env python3
"""Validate mathematical RF-port controls; never claims GSM acquisition."""
import argparse
from pathlib import Path
import re
import sys


SUMMARY = re.compile(r"rom4_rf_score_summary: mode=(zero|dc|tone) reads=(\d+) results=(\d+) nonzero=(\d+) maximum=([0-9a-f]{10}) second=(\d+) accepted=(\d+) acquisition_claim=0")


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


def check(text):
    sample = observe(text)
    mode, reads, results, nonzero, maximum, second, accepted = (
        sample[key] for key in ('mode', 'reads', 'results', 'nonzero', 'maximum', 'second', 'accepted'))
    if reads < 1000 or results < 1000 or not 0 <= accepted <= second <= results or not 0 <= nonzero <= results:
        raise ValueError("insufficient or inconsistent score observations")
    if mode in ("zero", "dc") and (nonzero or maximum or second or accepted):
        raise ValueError("silent/DC control unexpectedly advances the score")
    if mode == "tone" and (nonzero != results or maximum <= 0x574 or second == 0 or accepted == 0):
        raise ValueError("rotating input did not reach native accepted-value store")
    return mode, results, accepted


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        mode, results, accepted = check(args.log.read_text())
    except (OSError, ValueError) as error:
        print(f"ROM4 RF score: FAIL: {error}", file=sys.stderr)
        return 1
    print(f"ROM4 RF score: PASS mode={mode} results={results} accepted={accepted} acquisition_claim=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
