"""Join an external CC SETUP to native decoding and timed mobile responses."""
import re
from tools.rom6_adc_measure_check import check_rx_publications

SETUP=re.compile(r'npe3_raw_call_setup: uplink_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) policy=plain_test_cell raw_adc_only=1 physical_claim=0')
ASSIGNMENT=re.compile(r'npe3_raw_assignment: confirmed_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) raw_adc_only=1 physical_claim=0')
CALL_RR=re.compile(r'npe3_raw_call_rr: uplink_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) raw_adc_only=1 physical_claim=0')


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
    confirmed=[];alerting=[]
    for block in blocks:
        data=bytes.fromhex(block['octets']);length=data[2]>>2
        if block['start_frame']<=start+3 or data[0]!=1 or data[1]&1 or data[2]&3!=1 or not 2<=length<=20:
            continue
        l3=data[3:3+length]
        if l3[0]!=0x83:continue
        if l3[1]&0x3f==8:confirmed.append(block['start_frame'])
        if l3[1]&0x3f==1:alerting.append(block['start_frame'])
    return dict(native_setup_decode_qualified=True,setup_start_frame=start,
                native_decode_time=packets[0]['time'],
                native_call_confirmed_frames=confirmed,native_alerting_frames=alerting,
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
    rows=list(CALL_RR.finditer(text))
    if not rows or len(rows)!=text.count('npe3_raw_call_rr:'):
        raise ValueError('missing or malformed raw CC RR')
    if int(rows[0][1]) not in setup['native_call_confirmed_frames']:
        raise ValueError('first RR does not acknowledge actual Call Confirmed')
    packets=check_rx_publications(text)['packets']
    acknowledgements=[];previous=-1
    for index,row in enumerate(rows):
        source,start=int(row[1]),int(row[2])
        sources=[b for b in blocks if b['start_frame']==source]
        if len(sources)!=1:raise ValueError('RR lacks a unique timed uplink source')
        data=bytes.fromhex(sources[0]['octets']);length=data[2]>>2
        sequence=(data[1]>>1)&7
        expected=bytes([3,(((sequence+1)&7)<<5)|1,1])+b'\x2b'*20
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
        acknowledgements.append(dict(source_frame=source,start_frame=start,ns=sequence,
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
