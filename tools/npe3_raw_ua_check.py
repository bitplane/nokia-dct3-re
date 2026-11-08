"""Bind laboratory UA scheduling to independently decoded timed uplink bits."""
import re
from tools.rom6_adc_measure_check import check_rx_publications

UPLINK=re.compile(r'npe3_bts_uplink: start_frame=(\d+) octets=([0-9a-f]{46}) exact_code=1 port_bits_only=1 physical_claim=0')
UA=re.compile(r'npe3_raw_ua: uplink_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) raw_adc_only=1 physical_claim=0')


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
