"""Qualify observed storage-only DSP controls, without inventing events."""
import re
import struct


def inspect_peripheral_consumers(program):
    fragments={
        0x71ee:'f27474fa771a000a',
        0x8ec5:'f07423f6f0749162f0200100',
        0x8ed8:'74f8000800358813f1200200',
        0xb792:'74f80008003576f806b20000',
        0xb81f:'4d82f49575f800080034f07335f7',
        0x917b:'10f806c6f000b3d1881174f8000800361181f5e2',
    }
    for address,encoded in fragments.items():
        expected=bytes.fromhex(encoded)
        if program[2*address:2*address+len(expected)]!=expected:
            raise ValueError(f'peripheral consumer code differs at {address:04x}')
    return dict(rejected_raw_port_read=dict(candidate='71ef',reason='74fa is CALLD destination at 71ee; 000a belongs to delay-slot STM'),
                request0100=dict(default_handler='8ec5',callee='9162',state_table='b3d1 indexed by 06c6',status_port='36'),
                request0200=dict(handlers=['8ed3','b787'],input_port='35'),
                alternate_output=dict(site='b821',port='34'),
                sampling_control_link_proved=False,physical_peripheral_identity_proved=False)


def inspect_peripheral_routes(program):
    if len(program)!=0x20000:
        raise ValueError('peripheral routes require full live page-zero program')
    words=struct.unpack('>65536H',program)
    routes=[(address,words[address+1],words[address+3])
            for address in range(0x35c0,0x35f7)
            if words[address]==0xf130 and words[address+2]==0xf84c]
    expected=[(0x35c0,0x1000,0x369a),(0x35c4,0x1c,0x362f),
              (0x35cd,0x100,0x8ec5),(0x35d1,0x200,0x8ed3),
              (0x35d7,0x100,0xb7dc),(0x35db,0x200,0xb787),
              (0x35df,0x400,0x8e89),(0x35e3,0x80,0x371d),
              (0x35e7,2,0x3621),(0x35eb,1,0x362d),
              (0x35ef,0x20,0x3764),(0x35f3,0x40,0x3764)]
    if routes!=expected:
        raise ValueError('peripheral request dispatcher route census differs')
    if words[0x35c8:0x35cd]!=(0x61f8,0x06f3,0x0400,0xf830,0x35d7):
        raise ValueError('alternate peripheral handler selector differs')
    return dict(routes=[dict(test=f'{a:04x}',mask=f'{m:04x}',target=f'{t:04x}') for a,m,t in routes],
                alternate_selector='06f3 mask 0400',sampling_source_assignment_proved=False)


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
