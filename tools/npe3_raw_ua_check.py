"""Bind laboratory UA scheduling to independently decoded timed uplink bits."""
import re
from tools.rom6_adc_measure_check import check_rx_publications

UPLINK=re.compile(r'npe3_bts_uplink: start_frame=(\d+) octets=([0-9a-f]{46}) exact_code=1 port_bits_only=1 physical_claim=0')
UA=re.compile(r'npe3_raw_ua: uplink_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) raw_adc_only=1 physical_claim=0')
LU=re.compile(r'npe3_raw_lu_accept: uplink_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) policy=plain_test_cell raw_adc_only=1 physical_claim=0')


def check(text,blocks,*,require_native=True):
    uplinks=list(UPLINK.finditer(text))
    replies=list(UA.finditer(text))
    expected=[block for block in blocks if block['start_frame']%51==37]
    if (len(uplinks)!=text.count('npe3_bts_uplink:') or
            len(replies)!=text.count('npe3_raw_ua:') or not replies or
            [(int(row[1]),row[2]) for row in uplinks]!=
            [(block['start_frame'],block['octets']) for block in expected]):
        raise ValueError('external BTS uplinks differ from independent timed port decoding')
    candidates=[]
    identity=None
    for row in uplinks:
        data=bytes.fromhex(row[2]);length=data[2]>>2
        if data[0:2]!=b'\x01\x3f' or data[2]&3!=1 or not 1<=length<=20:continue
        request=data[2:3+length]
        if identity is not None and request!=identity:continue
        identity=request
        reply=b'\x01\x73'+request+b'\x2b'*(20-length)
        frame=int(row[1])
        candidates.append((frame,((frame+3)//51+1)*51+22,reply.hex()))
    if [(int(row[1]),int(row[2]),row[3]) for row in replies]!=candidates:
        raise ValueError('UA schedule or contention echo differs from transmitted SABM')
    for reply in replies:
        source=next(row for row in uplinks if row[1]==reply[1])
        if reply.start()<source.end():raise ValueError('UA precedes its complete uplink block')
    publications=[]
    if require_native:
        packets=check_rx_publications(text)['packets']
        for reply in replies:
            end_frame=int(reply[2])+3
            matches=[]
            for packet in packets:
                body=bytes.fromhex(packet['payload'])
                if (packet['type']==0x80 and len(body)==33 and body[:3]==b'\x80\x2a\0' and
                        int.from_bytes(body[3:6],'big')==end_frame%2715648 and
                        body[6:8]==b'\0\x23' and body[10:].hex()==reply[3] and
                        float(packet['time'])>end_frame*60000/13000000):
                    matches.append(packet)
            if len(matches)!=1:raise ValueError('missing unique successful native UA publication')
            publications.append(matches[0]['time'])
    return dict(external_sabm_decode_qualified=True,raw_ua_schedules=len(replies),
                raw_ua_start_frames=[int(row[2]) for row in replies],
                native_ua_decode_qualified=require_native,native_ua_times=publications,registration_qualified=False,
                physical_transmission_qualified=False)


def check_location_update(text,blocks):
    """Qualify native L2 delivery/acknowledgement,not MM registration."""
    rows=list(LU.finditer(text))
    if len(rows)!=1 or text.count('npe3_raw_lu_accept:')!=1:
        raise ValueError('missing unique plain-cell raw Location Updating Accept')
    row=rows[0];source=int(row[1]);start=int(row[2])
    requests=[block for block in blocks if block['start_frame']==source]
    ua=[match for match in UA.finditer(text) if int(match[1])==source]
    if len(requests)!=1 or len(ua)!=1 or start!=int(ua[0][2])+51 or row.start()<ua[0].end():
        raise ValueError('raw LU Accept is not ordered after its transmitted request and UA')
    sabm=bytes.fromhex(requests[0]['octets']);l3=sabm[3:21]
    if (sabm[:3]!=b'\x01\x3f\x49' or l3[:2]!=b'\x05\x08' or
            l3[9]!=8 or l3[10]&7!=1):
        raise ValueError('raw LU Accept lacks a complete transmitted IMSI request')
    # Explicit laboratory PLMN001-01,LAC1;echo the transmitted IMSI,not NV.
    expected=b'\x03\0\x45\x05\x02\0\xf1\x10\0\x01\x17\x08'+l3[10:18]+b'\x2b'*3
    if bytes.fromhex(row[3])!=expected:raise ValueError('raw LU Accept content differs from cell/request')
    matches=[]
    for packet in check_rx_publications(text)['packets']:
        body=bytes.fromhex(packet['payload'])
        if (packet['type']==128 and len(body)==33 and body[:3]==b'\x80\x2a\0' and
                int.from_bytes(body[3:6],'big')==(start+3)%2715648 and
                body[6:8]==b'\0\x23' and body[10:]==expected and
                float(packet['time'])>(start+3)*60000/13000000):
            matches.append(packet)
    if len(matches)!=1:raise ValueError('missing unique native raw LU Accept decode')
    acknowledgements=[block for block in blocks if block['start_frame']>start+3 and
                      bytes.fromhex(block['octets'])[:3]==b'\x03\x21\x01']
    if not acknowledgements:raise ValueError('no independently decoded native RR acknowledges LU Accept')
    return dict(native_lu_accept_decode_qualified=True,native_lapdm_ack_qualified=True,
                downlink_start_frame=start,native_decode_time=matches[0]['time'],
                acknowledgement_frame=acknowledgements[0]['start_frame'],
                mm_registration_qualified=False,physical_transmission_qualified=False)
