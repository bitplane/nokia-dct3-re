"""Bounded post-record NSM-1 startup observation, not hardware-absence proof."""
import argparse
from pathlib import Path
import re
import sys


def check(trace):
    rows=re.findall(r'nsm1_startup_sample: t=([0-9.]+) entries=([^ ]+) reports=([0-9a-f]+) charger_state=([0-9a-f]+) notification=([0-9a-f]+) sim_ready=([0-9a-f]+) selector=([0-9a-f]+) observer_writes=0',trace)
    if len(rows)!=6 or len(rows)!=trace.count('nsm1_startup_sample: '):
        raise ValueError('incomplete startup observation horizon')
    expected='280ac0:0,280b0c:0,2813a0:0,2bf2e6:0,2239f2:0,2bc4d4:0,2af080:2'
    for time,row in zip(range(10,61,10),rows):
        if not time<=float(row[0])<time+0.1 or row[1:]!=(expected,'0e','00','01','00','ff'):
            raise ValueError('post-record startup lifecycle changed')
    return {'observed_seconds':60,'report14_observed':False,'sim_initializations':2,
            'all_possible_producers_closed':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('trace',type=Path)
    args=parser.parse_args()
    try:print(check(args.trace.read_text()))
    except (ValueError,OSError) as error:
        print(f'NSM-1 startup observation failed: {error}',file=sys.stderr);return 1
    return 0


if __name__=='__main__':raise SystemExit(main())
