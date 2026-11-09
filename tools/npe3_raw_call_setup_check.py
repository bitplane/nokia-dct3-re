"""Join an external CC SETUP to native decoding and timed mobile responses."""
import re
from tools.rom6_adc_measure_check import check_rx_publications

SETUP=re.compile(r'npe3_raw_call_setup: uplink_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) policy=plain_test_cell raw_adc_only=1 physical_claim=0')
ASSIGNMENT=re.compile(r'npe3_raw_assignment: confirmed_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) raw_adc_only=1 physical_claim=0')
CALL_RR=re.compile(r'npe3_raw_call_rr: uplink_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) raw_adc_only=1 physical_claim=0')
CC_RELEASE=re.compile(r'npe3_raw_cc_release: disconnect_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) raw_adc_only=1 physical_claim=0')
CALL_CHANNEL_RELEASE=re.compile(r'npe3_raw_call_channel_release: complete_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) raw_adc_only=1 physical_claim=0')


def check(text,blocks):
    rows=list(SETUP.finditer(text))
    if len(rows)!=1 or text.count('npe3_raw_call_setup:')!=1:
        raise ValueError('missing unique raw incoming SETUP')
    row=rows[0];source,start=int(row[1]),int(row[2])
    expected=bytes.fromhex('030045030504046002008134015c0581551532f4'+'2b'*3)
    if bytes.fromhex(row[3])!=expected:
        raise ValueError('SETUP differs from declared speech/caller5551234 policy')
    requests=[b for b in blocks if b['start_frame']==source and b['octets'].startswith('013f410627')]
    if len(requests)!=1 or start!=((source+3)//51+1)*51+22+51:
        raise ValueError('SETUP is not sequenced after the actual Paging Response UA')
    packets=[]
    for packet in check_rx_publications(text)['packets']:
        body=bytes.fromhex(packet['payload'])
        if (packet['type']==128 and len(body)==33 and body[:3]==b'\x80\x2a\0' and
                int.from_bytes(body[3:6],'big')==(start+3)%2715648 and
                body[6:8]==b'\0\x23' and body[10:]==expected and
                float(packet['time'])>(start+3)*60000/13000000):
            packets.append(packet)
    if len(packets)!=1:raise ValueError('missing unique successful native SETUP decode')
    confirmed=[];alerting=[];connect=[]
    for block in blocks:
        data=bytes.fromhex(block['octets']);length=data[2]>>2
        if block['start_frame']<=start+3 or data[0]!=1 or data[1]&1 or data[2]&3!=1 or not 2<=length<=20:
            continue
        l3=data[3:3+length]
        if l3[0]!=0x83:continue
        if l3[1]&0x3f==8:confirmed.append(block['start_frame'])
        if l3[1]&0x3f==1:alerting.append(block['start_frame'])
        if l3[1]&0x3f==7:connect.append(block['start_frame'])
    return dict(native_setup_decode_qualified=True,setup_start_frame=start,
                native_decode_time=packets[0]['time'],
                native_call_confirmed_frames=confirmed,native_alerting_frames=alerting,
                native_connect_frames=connect,native_connect_emission_qualified=bool(connect),
                native_call_confirmed_qualified=bool(confirmed),
                native_alerting_qualified=bool(alerting),connected_call_qualified=False,
                speech_transport_qualified=False,physical_rf_tuning_qualified=False)


def check_assignment(text,blocks):
    setup=check(text,blocks)
    rows=list(ASSIGNMENT.finditer(text))
    if len(rows)!=1 or text.count('npe3_raw_assignment:')!=1:
        raise ValueError('missing unique raw Assignment Command')
    row=rows[0];source,start=int(row[1]),int(row[2])
    expected=bytes.fromhex('032221062e094023006301'+'2b'*12)
    if (bytes.fromhex(row[3])!=expected or source not in setup['native_call_confirmed_frames'] or
            start!=((source+3)//51+1)*51+22):
        raise ValueError('assignment does not acknowledge actual Call Confirmed on the declared channel')
    matches=[]
    for packet in check_rx_publications(text)['packets']:
        body=bytes.fromhex(packet['payload'])
        if (packet['type']==128 and len(body)==33 and body[:3]==b'\x80\x2a\0' and
                int.from_bytes(body[3:6],'big')==(start+3)%2715648 and
                body[6:8]==b'\0\x23' and body[10:]==expected and
                float(packet['time'])>(start+3)*60000/13000000):
            matches.append(packet)
    if len(matches)!=1:raise ValueError('missing unique native Assignment Command decode')
    return dict(native_assignment_decode_qualified=True,start_frame=start,
                native_decode_time=matches[0]['time'],traffic_channel_qualified=False,
                assignment_complete_qualified=False,speech_transport_qualified=False)


def check_call_rr(text,blocks):
    setup=check(text,blocks)
    rows=[]
    for pattern,name in ((CALL_RR,'call_rr'),(CC_RELEASE,'cc_release'),(CALL_CHANNEL_RELEASE,'call_channel_release')):
        found=list(pattern.finditer(text))
        if len(found)!=text.count('npe3_raw_'+name+':'):
            raise ValueError('malformed raw CC acknowledgement')
        rows.extend((row,name) for row in found)
    rows.sort(key=lambda item:item[0].start())
    if not rows or int(rows[0][0][1]) not in setup['native_call_confirmed_frames']:
        raise ValueError('first RR does not acknowledge actual Call Confirmed')
    packets=check_rx_publications(text)['packets']
    acknowledgements=[];previous=-1
    for index,(row,kind) in enumerate(rows):
        source,start=int(row[1]),int(row[2])
        sources=[b for b in blocks if b['start_frame']==source]
        if len(sources)!=1:raise ValueError('RR lacks a unique timed uplink source')
        data=bytes.fromhex(sources[0]['octets']);length=data[2]>>2
        sequence=(data[1]>>1)&7
        expected=bytes([3,(((sequence+1)&7)<<5)|1,1])+b'\x2b'*20
        if kind=='cc_release':
            if data[4]&0x3f!=0x25:raise ValueError('CC RELEASE lacks a DISCONNECT source')
            expected=bytes([3,(((sequence+1)&7)<<5)|2,9,data[3]^0x80,0x2d])+b'\x2b'*18
        elif kind=='call_channel_release':
            if data[4]&0x3f!=0x2a:raise ValueError('CHANNEL RELEASE lacks a RELEASE COMPLETE source')
            expected=bytes([3,(((sequence+1)&7)<<5)|4,13,6,13,0])+b'\x2b'*17
        if (source<=previous or sequence!=index%8 or data[0]!=1 or data[1]&1 or
                data[2]&3!=1 or not 2<=length<=20 or data[3]!=0x83 or
                bytes.fromhex(row[3])!=expected or start!=((source+3)//51+1)*51+22):
            raise ValueError('RR does not acknowledge the next actual CC I-frame')
        matches=[]
        for packet in packets:
            body=bytes.fromhex(packet['payload'])
            if (packet['type']==128 and len(body)==33 and body[:3]==b'\x80\x2a\0' and
                    int.from_bytes(body[3:6],'big')==(start+3)%2715648 and
                    body[6:8]==b'\0\x23' and body[10:]==expected and
                    float(packet['time'])>(start+3)*60000/13000000):
                matches.append(packet)
        if len(matches)!=1:raise ValueError('missing unique native CC RR decode')
        acknowledgements.append(dict(source_frame=source,start_frame=start,ns=sequence,kind=kind,
            cc_message_type=data[4]&0x3f,native_decode_time=matches[0]['time']))
        previous=source
    disconnect=[]
    for block in blocks:
        data=bytes.fromhex(block['octets']);length=data[2]>>2
        if (block['start_frame']>acknowledgements[0]['start_frame']+3 and data[0]==1 and not data[1]&1 and
                data[2]&3==1 and 5<=length<=20 and data[3]==0x83 and data[4]&0x3f==0x25 and
                2<=data[5]<=length-3):
            disconnect.append(block['start_frame'])
    return dict(native_call_rr_decode_qualified=True,acknowledgements=acknowledgements,
                native_disconnect_frames=disconnect,
                native_disconnect_qualified=bool(disconnect),call_release_qualified=False,
                connected_call_qualified=False,speech_transport_qualified=False)


def check_call_clearing(text,blocks):
    from tools.npe3_raw_ua_check import DISC_UA
    from tools.npe3_dedicated_check import packets as mcu_packets
    rr=check_call_rr(text,blocks)
    releases=list(CC_RELEASE.finditer(text));channels=list(CALL_CHANNEL_RELEASE.finditer(text))
    if (len(releases)!=1 or len(channels)!=1 or text.count('npe3_raw_cc_release:')!=1 or
            text.count('npe3_raw_call_channel_release:')!=1):
        raise ValueError('missing unique native call clearing inputs')
    release,channel=releases[0],channels[0]
    source,start=int(release[1]),int(release[2])
    complete,channel_start=int(channel[1]),int(channel[2])
    by_frame={b['start_frame']:bytes.fromhex(b['octets']) for b in blocks}
    acknowledgements={a['source_frame']:a for a in rr['acknowledgements']}
    if (len(by_frame)!=len(blocks) or source not in rr['native_disconnect_frames'] or
            source not in acknowledgements or complete not in acknowledgements):
        raise ValueError('call clearing lacks actual acknowledged uplink sources')
    disconnect=by_frame[source];completion=by_frame[complete]
    expected_release=bytes([3,((((disconnect[1]>>1)+1)&7)<<5)|2,9,disconnect[3]^0x80,0x2d])+b'\x2b'*18
    expected_channel=bytes([3,((((completion[1]>>1)+1)&7)<<5)|4,13,6,13,0])+b'\x2b'*17
    if (bytes.fromhex(release[3])!=expected_release or
            start!=acknowledgements[source]['start_frame'] or
            not start+3<complete or completion[0]!=1 or completion[1]&1 or
            completion[2]&3!=1 or not 2<=completion[2]>>2<=20 or
            completion[3]!=disconnect[3] or completion[4]&0x3f!=0x2a or
            bytes.fromhex(channel[3])!=expected_channel or
            channel_start!=acknowledgements[complete]['start_frame']):
        raise ValueError('CC RELEASE/COMPLETE or channel-release sequence differs')
    closings=[row for row in DISC_UA.finditer(text) if int(row[1])>channel_start+3]
    if len(closings)!=1:raise ValueError('missing unique post-call closing UA')
    closing=closings[0];disc,ua=int(closing[1]),int(closing[2])
    expected_ua=b'\x01\x73\x01'+b'\x2b'*20
    if (by_frame.get(disc,b'')[:3]!=b'\x01\x53\x01' or
            ua!=(disc//51+1)*51+22 or bytes.fromhex(closing[3])!=expected_ua or
            not any(channel_start+3<f<disc and data[:3]==b'\x03\x61\x01'
                    for f,data in by_frame.items())):
        raise ValueError('native channel-release acknowledgement/DISC differs')
    native=check_rx_publications(text)['packets'];times=[]
    for frame,expected in ((start,expected_release),(channel_start,expected_channel),(ua,expected_ua)):
        matches=[]
        for packet in native:
            body=bytes.fromhex(packet['payload'])
            if (packet['type']==128 and len(body)==33 and body[:3]==b'\x80\x2a\0' and
                    int.from_bytes(body[3:6],'big')==(frame+3)%2715648 and
                    body[6:8]==b'\0\x23' and body[10:]==expected and
                    float(packet['time'])>(frame+3)*60000/13000000):
                matches.append(packet)
        if len(matches)!=1:raise ValueError('missing unique native call-clearing downlink decode')
        times.append(float(matches[0]['time']))
    common=[p for p in mcu_packets(text) if times[-1]<p['time']<times[-1]+0.1 and
            p['kind']==2 and len(p['body'])==24 and p['body'][8]==0x60]
    if len(common)!=1:raise ValueError('MCU did not return to common control after call clearing')
    return dict(native_call_clearing_qualified=True,disconnect_frame=source,
                release_frame=start,release_complete_frame=complete,channel_release_frame=channel_start,
                disc_frame=disc,closing_ua_frame=ua,native_decode_times=times,
                common_configuration_time=common[0]['time'],stable_camping_qualified=False,
                traffic_channel_qualified=False,connected_call_qualified=False,speech_transport_qualified=False)
