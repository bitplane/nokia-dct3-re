"""Qualify native NSK-3 queue activity without claiming physical RF semantics."""
import argparse
from pathlib import Path
import re
import sys


def check(trace):
    pattern=(r'nsk3_rf_queue_sample: t=([0-9.]+) producer_writes=(\d+) '
             r'consumer_writes=(\d+) producer_changes=(\d+) consumer_changes=(\d+) '
             r'producer=([0-9a-f]{4}) consumer=([0-9a-f]{4}) observer_writes=0')
    matches=re.findall(pattern,trace)
    if len(matches)!=trace.count('nsk3_rf_queue_sample: ') or len(matches)!=6:
        raise ValueError('missing or malformed queue activity census')
    rows=[(float(row[0]),*[int(value) for value in row[1:5]],*[int(value,16) for value in row[5:]]) for row in matches]
    for target,row in zip((5,20,40,45,65,90),rows):
        if not target<=row[0]<target+0.1:raise ValueError('queue sample horizon differs')
        if any(not 0<=row[i+2]<=row[i] for i in (1,2)):
            raise ValueError('queue change count exceeds writes')
    for previous,current in zip(rows,rows[1:]):
        if any(current[i]<previous[i] for i in range(1,5)):
            raise ValueError('queue activity counter regressed')
    for previous,current in zip(rows[2:],rows[3:]):
        if any(current[i]-previous[i]<100 for i in range(1,5)):
            raise ValueError('late queue activity stopped')
    if any(row[5]!=row[6] for row in rows[2:]):
        raise ValueError('late sampled queue no longer drained')
    return {'late_producer_changes':rows[-1][3]-rows[2][3],
            'late_consumer_changes':rows[-1][4]-rows[2][4],
            'physical_rf_semantics_proved':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace',type=Path)
    args=parser.parse_args()
    try:print(check(args.trace.read_text()))
    except (ValueError,OSError) as error:
        print(f'NSK-3 RF queue qualification failed: {error}',file=sys.stderr)
        return 1
    return 0


if __name__=='__main__':raise SystemExit(main())
