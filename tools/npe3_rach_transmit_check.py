"""Join native RF port transactions to independently checked access bits."""
import csv
import io
import math
from tools.gsm_rach_signal import decode_p27_access
from tools.rom6_adc_measure_check import check_rx_publications


def transactions(writes):
    result=[]
    active=None
    previous=-1
    for row in csv.reader(io.StringIO(writes),delimiter='\t'):
        if len(row)!=5: raise ValueError('malformed DSP I/O journal')
        stamp=float(row[0])
        if not math.isfinite(stamp) or stamp<previous:
            raise ValueError('unordered DSP I/O journal')
        previous=stamp
        port,value,mask=(int(field,16) for field in row[2:])
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
            result.append((active[0],stamp,active[1]))
            active=None
    if active is not None: raise ValueError('incomplete RF serializer transaction')
    if not result: raise ValueError('no complete RF serializer transaction')
    return result


def check(text,writes,bsic=42):
    bursts=transactions(writes)
    reports=[packet for packet in check_rx_publications(text)['packets'] if packet['type']==0x84]
    if len(reports)!=len(bursts): raise ValueError('native RA_INFO/serializer count differs')
    decoded=[]
    frames=[]
    for (start,end,words),report in zip(bursts,reports):
        body=bytes.fromhex(report['payload'])
        result=decode_p27_access(words,bsic)
        if (len(body)!=4 or body[0]!=result['ra'] or
                not float(report['time'])<=start<end or
                int.from_bytes(body[1:],'big')>=2715648):
            raise ValueError('native RA_INFO differs from independently decoded port data')
        decoded.append(result)
        frames.append(int.from_bytes(body[1:],'big'))
    return dict(native_access_serialization_qualified=True,attempts=len(bursts),
                random_access_octets=[row['ra'] for row in decoded],reported_frames=frames,
                framing_offsets=[row['framing_offset'] for row in decoded],
                physical_release_qualified=False,network_decode_qualified=False,
                camping_qualified=False,registration_qualified=False)
