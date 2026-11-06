"""Correlate a complete I/O stream with the bounded native compare prefix.

This checks the current comparator/firmware contract, not silicon latch timing.
"""
from collections import Counter
import re
from tools.rom6_radio_census import WRITE


EDGE=re.compile(r'rom4_transfer_timer: expiry=(\d+) target=([0-9a-f]{4}) counter=([0-9a-f]{4}) delay=(\d+) t=(\d+\.\d{9})')


def nanoseconds(value):
    seconds,fraction=value.split('.')
    return int(seconds)*1_000_000_000+int(fraction)


def check(text, stream):
    edges=list(EDGE.finditer(text))
    totals=re.findall(r'\btransfer_expiries=(\d+)\b',text)
    if len(totals)!=1 or len(edges)!=min(int(totals[0]),256) or len(edges)<2:
        raise ValueError('missing or truncated compare prefix accounting')
    if len(edges)!=text.count('rom4_transfer_timer:'):
        raise ValueError('malformed compare prefix record')
    events=[]
    for ordinal,edge in enumerate(edges,1):
        sequence,target,counter,delay,time=edge.groups()
        if int(sequence)!=ordinal or target!=counter or not 0<int(delay)<=65536:
            raise ValueError('unordered or mismatched compare event')
        events.append((nanoseconds(time),'edge',int(target,16),ordinal))
    last=-1
    for line in stream:
        match=WRITE.fullmatch(line)
        if not match:
            raise ValueError('malformed timed-output source write')
        time,pc,port,value,mask=(value.decode() for value in match.groups())
        stamp=nanoseconds(time)
        if stamp<last or mask!='ffff':
            raise ValueError('unordered or partial timed-output source write')
        last=stamp
        if int(port,16) in (1,2,4,0x0d,0x0e,0x30,0x31,0x32):
            events.append((stamp,'write',int(port,16),int(value,16)))
    events.sort(key=lambda event:event[0])
    payload={0x30:0,0x31:0,0x32:0}
    target=None
    period=5000
    armed_payload=None
    armed_at=None
    owners=Counter()
    stable=0
    matched=[]
    interval_ack=False
    previous_edge=None
    acknowledgement_intervals=Counter()
    replacements=0
    writes_since_edge=0
    for stamp,kind,address,value in events:
        if kind=='write':
            if address in payload:
                payload[address]=value
            elif address==4:
                if writes_since_edge: replacements+=1
                writes_since_edge+=1
                target=value; armed_payload=payload.copy(); armed_at=stamp
            elif address==0x0e:
                period=value+1 if value else 5000
            elif address in (1,2) and value&0x1000 and previous_edge is not None:
                interval_ack=True
        else:
            if previous_edge is not None:
                acknowledgement_intervals['with_ack' if interval_ack else 'without_ack']+=1
            if address!=target or not 0<=address<period or armed_at is None or stamp<=armed_at:
                raise ValueError('compare did not use the latest in-range target')
            stable+=int(payload==armed_payload)
            owners[f'{payload[0x30]:04x}']+=1
            matched.append(dict(sequence=value,target=f'{address:04x}',
                words=[f'{payload[port]:04x}' for port in (0x30,0x31,0x32)]))
            previous_edge=stamp; interval_ack=False; writes_since_edge=0
            if value==len(edges): break # Never infer events beyond the logged cap.
    if len(matched)!=len(edges):
        raise ValueError('incomplete compare correlation')
    return dict(captured_edges=len(edges),total_model_expiries=int(totals[0]),
        payload_equal_at_arm_and_edge=stable,control_at_edge=dict(owners),
        reprograms_before_observed_edge=replacements,
        consecutive_edge_intervals=dict(acknowledgement_intervals),
        physical_latch_timing_proved=False,hardware_fifo_proved=False)
