"""Bind laboratory UA scheduling to independently decoded timed uplink bits."""
import re
from tools.rom6_adc_measure_check import check_rx_publications

UPLINK=re.compile(r'npe3_bts_uplink: start_frame=(\d+) octets=([0-9a-f]{46}) exact_code=1 port_bits_only=1 physical_claim=0')
UA=re.compile(r'npe3_raw_ua: uplink_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) raw_adc_only=1 physical_claim=0')
LU=re.compile(r'npe3_raw_lu_accept: uplink_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) policy=plain_test_cell raw_adc_only=1 physical_claim=0')
RELEASE=re.compile(r'npe3_raw_release: ack_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) raw_adc_only=1 physical_claim=0')
DISC_UA=re.compile(r'npe3_raw_disc_ua: disc_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) raw_adc_only=1 physical_claim=0')


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


def check_release(text,blocks):
    """Check actual normal-release exchange and the MCU return to common RX."""
    from tools.npe3_dedicated_check import packets as mcu_packets
    releases=list(RELEASE.finditer(text));responses=list(DISC_UA.finditer(text))
    if (len(releases)!=1 or len(responses)!=1 or
            text.count('npe3_raw_release:')!=1 or text.count('npe3_raw_disc_ua:')!=1):
        raise ValueError('missing unique raw normal-release exchange')
    release,response=releases[0],responses[0]
    locations=list(LU.finditer(text))
    ack,start=int(release[1]),int(release[2])
    disc,ua_start=int(response[1]),int(response[2])
    by_frame={block['start_frame']:bytes.fromhex(block['octets']) for block in blocks}
    expected_release=b'\x03\x02\x0d\x06\x0d\0'+b'\x2b'*17
    expected_ua=b'\x01\x73\x01'+b'\x2b'*20
    if (len(locations)!=1 or ack<=int(locations[0][2])+3 or release.start()<locations[0].end() or
            len(by_frame)!=len(blocks) or by_frame.get(ack,b'')[:3]!=b'\x03\x21\x01' or
            start!=(ack//51+1)*51+22 or bytes.fromhex(release[3])!=expected_release or
            by_frame.get(disc,b'')[:3]!=b'\x01\x53\x01' or disc<=start+3 or
            ua_start!=(disc//51+1)*51+22 or bytes.fromhex(response[3])!=expected_ua or
            response.start()<release.end()):
        raise ValueError('normal release differs from actual acknowledgement/DISC or channel grammar')
    if not any(start+3<frame<disc and data[:3]==b'\x03\x41\x01' for frame,data in by_frame.items()):
        raise ValueError('no native RR acknowledges second network I-frame')
    native=check_rx_publications(text)['packets'];times=[]
    for frame,data in ((start,expected_release),(ua_start,expected_ua)):
        matches=[]
        for packet in native:
            body=bytes.fromhex(packet['payload'])
            if (packet['type']==128 and len(body)==33 and body[:3]==b'\x80\x2a\0' and
                    int.from_bytes(body[3:6],'big')==(frame+3)%2715648 and
                    body[6:8]==b'\0\x23' and body[10:]==data and
                    float(packet['time'])>(frame+3)*60000/13000000):
                matches.append(packet)
        if len(matches)!=1:raise ValueError('missing unique native release/closing-UA decode')
        times.append(float(matches[0]['time']))
    common=[packet for packet in mcu_packets(text) if times[-1]<packet['time']<times[-1]+0.1 and
            packet['kind']==2 and len(packet['body'])==24 and packet['body'][8]==0x60]
    if len(common)!=1:raise ValueError('MCU did not return to common control after closing UA')
    return dict(native_normal_release_qualified=True,channel_release_start=start,
                disconnect_frame=disc,closing_ua_start=ua_start,
                native_decode_times=times,common_configuration_time=common[0]['time'],
                stable_camping_qualified=False,physical_transmission_qualified=False)


def check_paging_fill(text,common_configuration_time,*,addressed_octets=None):
    """Qualify native no-identity PCH decoding without inferring camping."""
    from tools.gsm_xcch_signal import paging_fill_octets
    fill=paging_fill_octets()
    marker='npe3_bts_pch: starts=6,12,16 octets='+fill.hex()+' tsc=2 no_identity=1 raw_adc_only=1 physical_claim=0'
    if text.count(marker)!=1 or text.count('npe3_bts_pch:')!=1:
        raise ValueError('missing exact no-identity paging source')
    assignments={bytes.fromhex(row[1]) for row in re.finditer(r'npe3_raw_agch: [^\n]*octets=([0-9a-f]{46}) ',text)}
    hits=[];failures=0;early_reports=[]
    for packet in check_rx_publications(text)['packets']:
        body=bytes.fromhex(packet['payload'])
        if packet['type']!=128 or not body or body[0]!=0x60:continue
        if len(body)!=33 or body[1]!=42:raise ValueError('malformed native common-channel block')
        if body[2]:
            failures+=1;continue
        frame=int.from_bytes(body[3:6],'big')
        if frame>=2715648 or frame%51 not in (9,15,19):
            raise ValueError('successful common block differs from combined source schedule')
        if body[10:] in assignments:continue
        if addressed_octets is not None and body[10:]==addressed_octets:continue
        if body[10:]!=fill:raise ValueError('native common block differs from transmitted no-identity fill')
        # Repeated identical fills cannot uniquely bind an output to one air
        # block. Preserve reported-FN/time disagreements rather than fitting a
        # clock/tolerance or relabelling an early report as source-correlated.
        if float(packet['time'])<=frame*60000/13000000:
            early_reports.append(dict(reported_frame=frame,time=packet['time']))
        hits.append((float(packet['time']),int.from_bytes(body[6:8],'big')))
    if not hits or not any(stamp>common_configuration_time for stamp,_ in hits):
        raise ValueError('no native paging-fill decode after normal release')
    return dict(native_paging_fill_qualified=True,blocks=len(hits),
                first_time=hits[0][0],last_time=hits[-1][0],
                reported_receiver_arfcns=sorted({key for _,key in hits}),
                early_reported_frames=early_reports,source_frame_binding_qualified=False,
                failed_common_blocks=failures,stable_camping_qualified=False,
                physical_carrier_selection_qualified=False)
