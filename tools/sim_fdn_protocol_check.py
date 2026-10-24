"""Shared laboratory FDN APDU contract; handset input/UI checks stay local."""
from tools.nsk3_phonebook_check import transmitted
from tools.sim_security_trace_check import statuses,padded_pin


def apdus(tx):
    offset=0;selected=None
    incoming={0xa4,0x20,0x24,0x26,0x28,0x2c,0xdc,0xd6,0x10,0x14,0xc2,0x88,0x32,0xa2}
    while offset<len(tx):
        if tx[offset]==0xff:
            if offset+3>len(tx):raise ValueError('truncated PPS')
            offset+=3;continue
        if offset+5>len(tx) or tx[offset]!=0xa0:raise ValueError('truncated/unknown APDU header')
        _,ins,p1,p2,length=tx[offset:offset+5];offset+=5
        count=length if ins in incoming else 0
        if offset+count>len(tx):raise ValueError('truncated command body')
        body=tx[offset:offset+count];offset+=count
        if ins==0xa4 and len(body)==2:selected=int.from_bytes(body,'big')
        yield ins,p1,p2,length,body,selected


def validity_targets(tx,instruction):
    targets=[]
    for ins,p1,p2,length,body,selected in apdus(tx):
        if ins==instruction:
            if p1 or p2 or length:raise ValueError('validity parameters differ')
            targets.append(selected)
    return targets


def check_protocol_phase(trace,phase,tx):
    if phase not in range(4):raise ValueError('unknown FDN phase')
    if statuses(trace,0x20)!=[(0x9000,3,10)]*(1 if phase%2 else 2):raise ValueError('VERIFY results differ')
    if any(statuses(trace,ins) for ins in (0x24,0x26,0x28,0x2c)):
        raise ValueError('unexpected credential mutation')
    expected=([0x6f7e,0x6f07],[0x6f7e,0x6f07],[0x6f7e,0x6f07,0x6f3a],[])[phase]
    if len(tx)>=4096:raise ValueError('SIM transmit observation cap reached')
    if validity_targets(tx,0x44)!=expected:raise ValueError('identity/ADN rehabilitation route differs')
    if validity_targets(tx,4)!=([0x6f3a] if phase==0 else []):raise ValueError('ADN invalidation route differs')
    if statuses(trace,0x44)!=[(0x9000,3,10)]*len(expected):raise ValueError('rehabilitation result differs')
    if statuses(trace,4)!=([(0x9000,3,10)] if phase==0 else []):raise ValueError('invalidation result differs')
    if tx.count(bytes.fromhex('a020000108')+padded_pin('1234'))!=1:raise ValueError('startup PIN APDU differs')
    pin2=bytes.fromhex('a020000208')+padded_pin('5678')
    if tx.count(pin2)!=(0 if phase%2 else 1):raise ValueError('PIN2 APDU differs')
    if not phase%2:tx.index(bytes((0xa0,4 if phase==0 else 0x44,0,0,0)),tx.index(pin2)+len(pin2))


def check_phase(trace,phase,tx_label):
    check_protocol_phase(trace,phase,transmitted(trace,4096,tx_label))
