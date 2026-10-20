"""Physical Send/End qualification, not native call or registration acceptance."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk3_static_check import inspect
from tools.nsk3_packet_check import check_cache, check_mode_chain, check_completion_wait


def contacts(trace,enabled):
    rows=re.findall(r'nsk3_radio_contact: key=([0-9a-f]+) down=([01]) t=([0-9.]+) firmware_writes=0',trace)
    if len(rows)!=trace.count('nsk3_radio_contact:'):raise ValueError('malformed physical contact')
    expected=[('0e','1',22),('0e','0',22.2),('0f','1',30),('0f','0',30.2)] if enabled else []
    if len(rows)!=len(expected):raise ValueError('physical Call/End contact count differs')
    for row,want in zip(rows,expected):
        if row[:2]!=want[:2] or abs(float(row[2])-want[2])>0.05:
            raise ValueError('physical Call/End ordering/timing differs')
    for key in ('0e','0f'):
        decoded=bool(re.search(r'nsk3_logical_key: site=002ac2fc key='+key+r' cycles=[0-9a-f]+ observer_writes=0',trace))
        if decoded!=enabled:raise ValueError('own logical Send/End decoding differs')
    if not re.search(r'nsk3_security_accepted: result=00000001 ',trace):
        raise ValueError('physical phone code was not accepted')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path)
    parser.add_argument('call',type=Path)
    parser.add_argument('control',type=Path)
    args=parser.parse_args()
    try:
        _,image=inspect(args.source,'boot4')
        for run,enabled in ((args.call,True),(args.control,False)):
            trace=(run/'error.log').read_text()
            contacts(trace,enabled)
            check_cache(trace)
            check_mode_chain(trace)
            check_completion_wait(trace)
            if 'nsk3_queue_construct:' in trace or 'nsk3_server_dynamic_construct:' in trace:
                raise ValueError('previously dormant producer now runs; reclassify the boundary')
            if (run/'nvram/nsk3r4t/flash').read_bytes()!=image:
                raise ValueError('own executable flash changed')
        if (args.call/'nvram/nsk3r4t/sim_card').read_bytes()!=(args.control/'nvram/nsk3r4t/sim_card').read_bytes():
            raise ValueError('matched-pair SIM storage differs')
        call_nv=(args.call/'nvram/nsk3r4t/eeprom').read_bytes()
        control_nv=(args.control/'nvram/nsk3r4t/eeprom').read_bytes()
        if len(call_nv)!=8192 or len(control_nv)!=8192:raise ValueError('own EEPROM extent differs')
        differences=[hex(i) for i,(a,b) in enumerate(zip(call_nv,control_nv)) if a!=b]
    except (ValueError,OSError) as error:
        print(f'NSK-3 call-input check failed: {error}',file=sys.stderr)
        return 1
    print('Physical Send/End decode qualified; matched server remains empty; calls/network unaccepted')
    print({'eeprom_changed_offsets':differences,'eeprom_change_semantics_qualified':False})
    return 0


if __name__=='__main__':raise SystemExit(main())
