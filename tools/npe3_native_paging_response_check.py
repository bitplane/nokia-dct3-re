"""Prove a native Paging Response from timed port bits,not MCU state forcing."""
from tools.npe3_rach_transmit_check import check as check_access,check_agch
from tools.npe3_addressed_page_check import REQUEST
from tools.rom6_adc_measure_check import check_rx_publications
from tools.radio_mobile_identity import paging_group


def check(text,writes):
    access=check_access(text,writes,require_windows=True,require_releases=True,allow_normal=True)
    agch=check_agch(text,allow_normal=True)
    if access['attempts']<2 or agch['responses']<2:
        raise ValueError('paging requires an access/assignment separate from Location Updating')
    requests=list(REQUEST.finditer(text))
    if len(requests)!=1:raise ValueError('missing unique external addressed page')
    request=requests[0]
    identity=request[2]
    phase,offset=paging_group(identity,combined=True)
    if (int(request[3]),int(request[4]))!=(phase,offset):
        raise ValueError('BTS page does not target the registered IMSI group')
    normal=access['normal_control_blocks']
    registrations=[b for b in normal if b['octets'].startswith('013f490508')]
    responses=[b for b in normal if b['octets'].startswith('013f410627')]
    if len(registrations)!=1 or not responses:
        raise ValueError('missing native LU or RR Paging Response SABM')
    if bytes.fromhex(registrations[0]['octets'])[13:21].hex()!=identity:
        raise ValueError('page identity differs from the native Location Updating request')
    native_pages=[]
    for packet in check_rx_publications(text)['packets']:
        body=bytes.fromhex(packet['payload'])
        if packet['type']==0x80 and len(body)==33 and body[:1]==b'\x60' and body[2]==0 and body[10:].hex()==request[6]:
            native_pages.append(float(packet['time']))
    if not native_pages:raise ValueError('no successful native addressed-page decode')
    for response in responses:
        data=bytes.fromhex(response['octets'])
        if (data[5]>7 or data[6]!=3 or data[10]!=8 or data[11:19].hex()!=identity
                or data[19:]!=bytes([0x2b])*4):
            raise ValueError('malformed native Paging Response identity/classmark envelope')
        frame=response['start_frame']
        preceding=[f for f in access['reported_frames'] if float(request[1])<f*60000/13000000<frame*60000/13000000]
        if not preceding or not any(float(request[1])<t<preceding[-1]*60000/13000000 for t in native_pages):
            raise ValueError('native response is not preceded by the addressed page and a new access')
        if not any(start<frame and start>preceding[-1] for start in agch['agch_start_frames']):
            raise ValueError('native response lacks its new raw channel assignment')
    return dict(native_paging_response_qualified=True,identity=identity,
                response_start_frames=[r['start_frame'] for r in responses],
                access_attempts=access['attempts'],native_agch_responses=agch['responses'],
                physical_rf_tuning_qualified=False,stable_camping_qualified=False,
                call_signalling_qualified=False)
