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


def check_metric(trace,program):
    # Pin live instructions before assigning resident-map meanings to observations.
    fragments={0x4701:'f0742424',0x47e6:'7712125477131255308a218a20823083',
               0x330a:'74d60027',0x0ae6:'6ff8219b0d20'}
    if len(program)!=131072:raise ValueError('live DSP program extent differs')
    for address,raw in fragments.items():
        expected=bytes.fromhex(raw)
        if program[address*2:address*2+len(expected)]!=expected:
            raise ValueError(f'live measurement instruction differs at {address:04x}')
    rows=re.findall(r'nsk3_scan_metric: index=(\d+) pc=([0-9a-f]+) a=([0-9a-f]+) threshold=([0-9a-f]+) producer=([0-9a-f]+) consumer=([0-9a-f]+) t=([0-9.]+) observer_writes=0',trace)
    if len(rows)!=8 or len(rows)!=trace.count('nsk3_scan_metric: '):
        raise ValueError('measurement return census missing or malformed')
    for index,row in enumerate(rows,1):
        n,pc,a,threshold,producer,consumer,time=row
        if int(n)!=index or int(pc,16)!=0x0ae7 or int(a,16)!=0 or int(threshold,16)!=0x574:
            raise ValueError('measurement return or qualification threshold differs')
        if producer!=consumer or not 39<float(time)<40:
            raise ValueError('measurement queue or observation window differs')
    return {'measurement_returns':8,'accumulator':0,'threshold':0x574,
            'physical_rf_semantics_proved':False}


def check_source(trace):
    rows=re.findall(r'nsk3_scan_rf_input: index=(\d+) value=([0-9a-f]{4}) pc=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0',trace)
    if len(rows)!=8 or len(rows)!=trace.count('nsk3_scan_rf_input: '):
        raise ValueError('RF source observation missing or malformed')
    previous=0
    for index,(number,value,pc,time) in enumerate(rows):
        timestamp=float(time)
        if int(number)!=index+1 or int(value,16)!=0 or int(pc,16)!=(0x330c,0x3312,0x3318,0x331e)[index%4]:
            raise ValueError('RF source value or consumer differs')
        if not 39<timestamp<40 or timestamp<=previous:raise ValueError('RF source ordering differs')
        previous=timestamp
    queue=re.findall(r'nsk3_scan_rf_queue: start=1a00 words=72 data=([0-9a-f]+) observer_writes=0',trace)
    if len(queue)!=1 or trace.count('nsk3_scan_rf_queue: ')!=1 or queue[0]!='0000'*72:
        raise ValueError('observed queue is not exactly72 zero words')
    return {'source_words':8,'observed_queue_words':72,'all_zero':True,
            'physical_rf_semantics_proved':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace',type=Path)
    parser.add_argument('--metric-program',type=Path)
    parser.add_argument('--source-input',action='store_true')
    args=parser.parse_args()
    try:
        trace=args.trace.read_text()
        print(check_metric(trace,args.metric_program.read_bytes()) if args.metric_program else check(trace))
        if args.source_input:print(check_source(trace))
    except (ValueError,OSError) as error:
        print(f'NSK-3 RF queue qualification failed: {error}',file=sys.stderr)
        return 1
    return 0


if __name__=='__main__':raise SystemExit(main())
