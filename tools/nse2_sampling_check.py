"""Qualify observed storage-only DSP controls, without inventing events."""
import re


def inspect_sampling_controls(program,text):
    fragments={
        0x447e:'491109f82193f84a448501f800eef49575f8000b000a',
        0x448b:'f1203a98f49575f8000b000b',
        0x4494:'75f800110009',
    }
    for address,encoded in fragments.items():
        expected=bytes.fromhex(encoded)
        if program[2*address:2*address+len(expected)]!=expected:
            raise ValueError(f'sampling control code differs at {address:04x}')
    rows=re.findall(r'nse2_sampling_control: port=(09|0a|0b) value=([0-9a-f]{4}) mask=ffff n=(\d+) pc=([0-9a-f]{4}) mode=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0',text)
    summaries=re.findall(r'nse2_sampling_summary: port=(09|0a|0b) writes=(\d+) last=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0',text)
    if len(summaries)!=3 or {p for p,_,_,_ in summaries}!={'09','0a','0b'}:
        raise ValueError('sampling summaries missing or duplicated')
    counts={}
    for port,count,last,time in summaries:
        count=int(count)
        observed=[row for row in rows if row[0]==port]
        if count<12 or [int(row[2]) for row in observed]!=list(range(1,13)) or not 29.9<=float(time)<=30.1:
            raise ValueError('sampling control prefix/count coverage differs')
        expected={'09':'0332','0a':'028f','0b':'3a98'}[port]
        if last!=expected:
            raise ValueError('terminal sampling control differs')
        counts[port]=count
    if counts['0a']!=counts['0b'] or counts['0a']!=2*counts['09']-3:
        raise ValueError('sampling control activation/deactivation accounting differs')
    scan=[row for row in rows if row[4]=='000c']
    if [(p,v,pc) for p,v,_,pc,_,_ in scan[:3]]!=[('0a','028f','4489'),('0b','3a98','4491'),('09','0332','4497')]:
        raise ValueError('first scan sampling-control sequence differs')
    return dict(sampling_control_writes=counts,scan_controls={'09':'0332','0a':'028f','0b':'3a98'},
                sampling_events_implemented=False,physical_semantics_proved=False)
