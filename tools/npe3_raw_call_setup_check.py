"""Join an external CC SETUP to native decoding and timed mobile responses."""
import re
from tools.rom6_adc_measure_check import check_rx_publications

SETUP=re.compile(r'npe3_raw_call_setup: uplink_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) policy=plain_test_cell raw_adc_only=1 physical_claim=0')


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
