"""Join native RF port transactions to independently checked access bits."""
import csv
import io
import math
import re
from tools.gsm_rach_signal import decode_p27_access
from tools.rom6_adc_measure_check import check_rx_publications

WINDOW=re.compile(r'npe3_tx_window: sequence=(\d+) t=([\d.]+) words=(\d+) counter=([0-9a-f]{4}) controls=([0-9a-f,]+) deadlines=([\d.,-]+) observer_writes=0 physical_claim=0')


def transactions(writes):
    result=[]
    active=None
    previous=-1
    compares={port:0 for port in range(5,9)}
    for row in csv.reader(io.StringIO(writes),delimiter='\t'):
        if len(row)!=5: raise ValueError('malformed DSP I/O journal')
        stamp=float(row[0])
        if not math.isfinite(stamp) or stamp<previous:
            raise ValueError('unordered DSP I/O journal')
        previous=stamp
        port,value,mask=(int(field,16) for field in row[2:])
        if port in compares:
            if mask!=0xffff: raise ValueError('partial projected compare write')
            compares[port]=value
            continue
        if port not in (0x21,0x27): continue
        if mask!=0xffff: raise ValueError('partial RF serializer port write')
        if port==0x27:
            if active is None: raise ValueError('unframed P27 transmit data')
            active[1].append(value)
        elif value&0xe00==0xa00:
            if active is not None: raise ValueError('RF serializer restarted before commit')
            active=[stamp,[]]
        elif active is not None:
            if value&0xe00!=0x600: raise ValueError('RF serializer aborted before commit')
            result.append((active[0],stamp,active[1],tuple(compares.values())))
            active=None
    if active is not None: raise ValueError('incomplete RF serializer transaction')
    if not result: raise ValueError('no complete RF serializer transaction')
    return result


def check_windows(text,bursts,frames):
    rows=list(WINDOW.finditer(text))
    if len(rows)!=len(bursts) or len(rows)!=text.count('npe3_tx_window:'):
        raise ValueError('incomplete projected transmit windows')
    source_frames=[]
    for index,(row,burst) in enumerate(zip(rows,bursts),1):
        number,stamp,count,counter,controls,deadlines=row.groups()
        stamp=float(stamp);counter=int(counter,16)
        controls=tuple(int(word,16) for word in controls.split(','))
        due=tuple(float(word) for word in deadlines.split(','))
        if (int(number)!=index or int(count)!=15 or stamp!=burst[1] or controls!=burst[3] or
                len(controls)!=4 or len(due)!=4 or not 0<=counter<5000 or
                any(not 0<=target<5000 for target in controls) or
                any(not math.isfinite(at) or at<=stamp for at in due)):
            raise ValueError('transmit window differs from actual buffer/control publication')
        distances=[(target-counter)%5000 or 5000 for target in controls]
        phases=[distance*12/13000000-(at-stamp) for distance,at in zip(distances,due)]
        if (min(phases)<-2e-9 or max(phases)>12/13000000+2e-9 or
                max(phases)-min(phases)>2e-9 or not due[2]<due[0]<due[1]<due[3]):
            raise ValueError('projected transmit controls do not share the declared counter phase')
        source_frames.append(int(due[0]*13000000)//60000%2715648)
    if source_frames!=frames:
        raise ValueError('P05 source-frame projection differs from native RA_INFO')
    return dict(windows=len(rows),p05_source_frames=source_frames,
                declared_counter_hz='13000000/12',physical_release_qualified=False)


def check(text,writes,bsic=42,*,require_windows=False):
    bursts=transactions(writes)
    reports=[packet for packet in check_rx_publications(text)['packets'] if packet['type']==0x84]
    if len(reports)!=len(bursts): raise ValueError('native RA_INFO/serializer count differs')
    decoded=[]
    frames=[]
    for (start,end,words,_),report in zip(bursts,reports):
        body=bytes.fromhex(report['payload'])
        result=decode_p27_access(words,bsic)
        if (len(body)!=4 or body[0]!=result['ra'] or
                not float(report['time'])<=start<end or
                int.from_bytes(body[1:],'big')>=2715648):
            raise ValueError('native RA_INFO differs from independently decoded port data')
        decoded.append(result)
        frames.append(int.from_bytes(body[1:],'big'))
    result=dict(native_access_serialization_qualified=True,attempts=len(bursts),
                random_access_octets=[row['ra'] for row in decoded],reported_frames=frames,
                framing_offsets=[row['framing_offset'] for row in decoded],
                physical_release_qualified=False,network_decode_qualified=False,
                camping_qualified=False,registration_qualified=False)
    if require_windows:result['candidate_windows']=check_windows(text,bursts,frames)
    return result
