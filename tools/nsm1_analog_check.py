"""Own nominal/saturated analog contrast and graphical presentation, not UI use."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_analog_contract import inspect
from tools.nsm1_published_record_check import records
from tools.nsk1_phonebook_roundtrip import frame_check

FRAME='24cbab0ad5b13376772a44f0eec7835a7ea865795c9ee229c7631fa2e0ea1fb2'


def check_trace(trace,nominal):
    rows=re.findall(r'nsm1_analog_entry: pc=0028a892 index=(\d+) r0=([0-9a-f]+) first=([0-9a-f]+) second=([0-9a-f]+) remaining=([0-9a-f]+) gain=([0-9a-f]+) offset=([0-9a-f]+) t=([0-9.]+) observer_writes=0',trace)
    if len(rows)!=8:raise ValueError('eight bounded own analog returns required')
    for index,row in enumerate(rows,1):
        number,result,first,second,remaining,gain,offset,time=row
        if (int(number)!=index or int(result,16)!=int(nominal)
                or int(first,16)!=(0xdfd if nominal else 0x19dc) or first!=second
                or int(remaining,16)!=(11-index if nominal else 10)
                or gain!='3f8033c4' or int(offset,16)!=0):
            raise ValueError('analog conversion/countdown contrast differs')
    tail='nsm1_analog_entry: pc=00223a68 '
    if bool(trace.count(tail))!=nominal:raise ValueError('shared report tail observation differs')
    report='nsm1_scalar_receive: task=01 value=00000014 '
    if bool(trace.count(report))!=nominal:raise ValueError('organic report14 delivery differs')
    if nominal and 'nsm1_readiness_nibble: value=000f0000 mask=00ff0000 pc=0028120c task=01 ' not in trace:
        raise ValueError('complete startup report bitmap not observed')
    if nominal and 'SIM status ins=a4 sw=9f0f' not in trace:
        raise ValueError('no observed SIM SELECT after readiness')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('control',type=Path);parser.add_argument('nominal',type=Path)
    args=parser.parse_args()
    try:
        for run,nominal in ((args.control,False),(args.nominal,True)):
            trace=(run/'error.log').read_text();check_trace(trace,nominal);records(trace,True)
            inspect((run/'nvram/nsm1r4t/flash').read_bytes())
        frame_check(args.nominal/'snap/6150-native-compatibility.png',FRAME)
    except (ValueError,OSError) as error:
        print(f'NSM-1 analog qualification failed: {error}',file=sys.stderr);return 1
    print('NSM-1 own analog contrast and Security code presentation PASS; interactive input unaccepted')
    return 0


if __name__=='__main__':raise SystemExit(main())
