"""Diagnose the observed NSE-2 reason-68 restart; this is not a stable-boot gate."""
import argparse
import json
from pathlib import Path
import re


def diagnose(text):
    reasons = re.findall(r'nse2_reset_reason: value=68 pc=0028db6e lr=00290361 t=([0-9.]+) observer_writes=0', text)
    resets = re.findall(r'nse2_reset_entry: reason=00000068 lr=0023616d stack=([^ ]+) t=([0-9.]+) observer_writes=0', text)
    identities = re.findall(r'nse2_native_identity: n=(\d+) data=0004 mask=ffff pc=0f0f t=([0-9.]+) observer_writes=0', text)
    if len(reasons) != 1 or len(resets) != 1 or len(identities) != 2:
        raise ValueError('NSE-2 own reason-68 reset and two native startups missing')
    reason_time, reset_time = float(reasons[0]), float(resets[0][1])
    if not 30.9 < reason_time < 31.1 or not reason_time < reset_time < reason_time + 1.1:
        raise ValueError('NSE-2 observed restart timing differs')
    if not float(identities[0][1]) < reason_time < reset_time < float(identities[1][1]):
        raise ValueError('NSE-2 restart did not separate the two native startups')
    if '000000d5' not in resets[0][0].split(','):
        raise ValueError('NSE-2 delayed reset event d5 missing from captured stack')
    return dict(stable_long_boot=False, reset_reason='68', reason_time=reason_time,
                reset_time=reset_time, policy_caller='290361', native_startups=2,
                scope='observed failure diagnostic; no successful long-run claim')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(diagnose(args.trace.read_text()), indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()
