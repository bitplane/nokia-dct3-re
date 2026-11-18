"""Join an external CC SETUP to native decoding and timed mobile responses."""
import re
from tools.rom6_adc_measure_check import check_rx_publications

SETUP=re.compile(r'npe3_raw_call_setup: uplink_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) policy=plain_test_cell raw_adc_only=1 physical_claim=0')
ASSIGNMENT=re.compile(r'npe3_raw_assignment: confirmed_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) raw_adc_only=1 physical_claim=0')
LATE_ASSIGNMENT=re.compile(r'npe3_raw_late_assignment: connect_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) raw_adc_only=1 physical_claim=0')
CALL_RR=re.compile(r'npe3_raw_call_rr: uplink_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) raw_adc_only=1 physical_claim=0')
CC_RELEASE=re.compile(r'npe3_raw_cc_release: disconnect_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) raw_adc_only=1 physical_claim=0')
CALL_CHANNEL_RELEASE=re.compile(r'npe3_raw_call_channel_release: complete_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) raw_adc_only=1 physical_claim=0')
CM_ACCEPT=re.compile(r'npe3_raw_cm_service_accept: uplink_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) policy=plain_test_cell raw_adc_only=1 physical_claim=0')
PROCEEDING=re.compile(r'npe3_raw_call_proceeding: setup_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) raw_adc_only=1 physical_claim=0')
MO_ASSIGNMENT=re.compile(r'npe3_raw_mo_assignment: ack_start_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) raw_adc_only=1 physical_claim=0')


def check_outgoing_progress(text,blocks,*,assignment=False):
    outgoing=check_outgoing(text,blocks,admitted=True)
    rows=list(PROCEEDING.finditer(text))
    if len(rows)!=1 or text.count('npe3_raw_call_proceeding:')!=1:
        raise ValueError('missing unique raw CALL PROCEEDING')
    row=rows[0];source,start=int(row[1]),int(row[2])
    proceeding=bytes.fromhex('0322098302'+'2b'*18)
    if source not in outgoing['setup_frames'] or start!=((source+3)//51+1)*51+22 or bytes.fromhex(row[3])!=proceeding:
        raise ValueError('CALL PROCEEDING does not acknowledge actual SETUP')
    acknowledgements=[b['start_frame'] for b in blocks if b['start_frame']>start+3 and
                      bytes.fromhex(b['octets'])[:3]==b'\x03\x41\x01']
    if not acknowledgements:raise ValueError('no native RR acknowledges CALL PROCEEDING')
    expected=[(start,proceeding)]
    report=dict(native_call_proceeding_decode_qualified=True,native_lapdm_ack_qualified=True,proceeding_start_frame=start,
                acknowledgement_frames=acknowledgements,native_assignment_decode_qualified=False,
                traffic_channel_qualified=False,connected_call_qualified=False,speech_transport_qualified=False)
    if assignment:
        rows=list(MO_ASSIGNMENT.finditer(text))
        if len(rows)!=1 or text.count('npe3_raw_mo_assignment:')!=1:
            raise ValueError('missing unique raw outgoing Assignment Command')
        row=rows[0];ack,assigned=int(row[1]),int(row[2])
        command=bytes.fromhex('032421062e094023006301'+'2b'*12)
        if ack not in acknowledgements or assigned!=(ack//51+1)*51+22 or bytes.fromhex(row[3])!=command:
            raise ValueError('outgoing assignment does not follow real CALL PROCEEDING acknowledgement')
        expected.append((assigned,command))
        report.update(native_assignment_decode_qualified=True,assignment_start_frame=assigned)
    native=check_rx_publications(text)['packets'];times=[]
    for frame,octets in expected:
        matches=[]
        for packet in native:
            body=bytes.fromhex(packet['payload'])
            if (packet['type']==128 and len(body)==33 and body[:3]==b'\x80\x2a\0' and
                    int.from_bytes(body[3:6],'big')==(frame+3)%2715648 and body[6:8]==b'\0\x23' and
                    body[10:]==octets and float(packet['time'])>(frame+3)*60000/13000000):
                matches.append(packet)
        if len(matches)!=1:raise ValueError('missing unique native outgoing progress decode')
        times.append(matches[0]['time'])
    report['native_decode_times']=times
    return report


def check_outgoing(text,blocks,*,admitted=False):
    from tools.npe3_raw_ua_check import check as check_ua,UA
    from tools.radio_outgoing_call_trace_check import decode_called_digits
    # A later physical End clears the connected call; it is checked separately.
    keys=[key for key in re.finditer(r'npe3_native_physical: key=(\w+) t=([\d.]+) observer_writes=0',text) if key[1]!='End']
    names=[f'Digit{i}' for i in range(1,8)]+['Send']
    if len(keys)!=8 or [row[1] for row in keys]!=names:
        raise ValueError('outgoing physical dial/Send sequence incomplete')
    for index,row in enumerate(keys):
        end=keys[index+1].start() if index<7 else (text.find('npe3_native_physical: key=End',row.end())%(len(text)+1))
        codes=re.findall(r'npe3_native_key: value=([0-9a-f]{2}) observer_writes=0',text[row.end():end])
        if abs(float(row[2])-(20+index))>0.02 or not codes or set(codes)!={f'{index+1 if index<7 else 14:02x}'}:
            raise ValueError('outgoing physical key timing/decode differs')
    check_ua(text,blocks)
    lu=[bytes.fromhex(b['octets']) for b in blocks if b['octets'].startswith('013f490508')]
    requests=[b for b in blocks if b['octets'].startswith('013f410524')]
    if len(lu)!=1 or len(requests)!=1:raise ValueError('outgoing request lacks unique LU/CM identity sources')
    source=requests[0]['start_frame'];request=bytes.fromhex(requests[0]['octets'])[3:19]
    if (source*60000/13000000<=27 or request[2]&15!=1 or request[3]!=3 or
            request[7]!=8 or request[8:]!=lu[0][13:21]):
        raise ValueError('CM call service request differs from established air identity')
    report=dict(native_cm_service_request_qualified=True,request_start_frame=source,
                identity=request[8:].hex(),native_cm_service_accept_qualified=False,
                native_outgoing_setup_qualified=False,connected_call_qualified=False,
                speech_transport_qualified=False,physical_rf_tuning_qualified=False)
    rows=list(CM_ACCEPT.finditer(text))
    if not admitted:
        if rows or 'npe3_raw_cm_service_accept:' in text:
            raise ValueError('CM no-reply control unexpectedly admitted service')
        return report
    if len(rows)!=1 or text.count('npe3_raw_cm_service_accept:')!=1:
        raise ValueError('missing unique raw CM SERVICE ACCEPT')
    row=rows[0];start=int(row[2]);expected=b'\x03\0\x09\x05\x21'+b'\x2b'*18
    ua=[match for match in UA.finditer(text) if int(match[1])==source]
    if len(ua)!=1 or int(row[1])!=source or start!=int(ua[0][2])+51 or bytes.fromhex(row[3])!=expected:
        raise ValueError('CM admission does not follow actual request/contention UA')
    native=[]
    for packet in check_rx_publications(text)['packets']:
        body=bytes.fromhex(packet['payload'])
        if (packet['type']==128 and len(body)==33 and body[:3]==b'\x80\x2a\0' and
                int.from_bytes(body[3:6],'big')==(start+3)%2715648 and body[6:8]==b'\0\x23' and
                body[10:]==expected and float(packet['time'])>(start+3)*60000/13000000):
            native.append(packet)
    if len(native)!=1:raise ValueError('missing unique native CM SERVICE ACCEPT decode')
    setups=[]
    for block in blocks:
        data=bytes.fromhex(block['octets']);length=data[2]>>2
        if (block['start_frame']>start+3 and data[0]==1 and not data[1]&1 and
                data[2]&3==1 and 2<=length<=20 and data[3]&15==3 and data[4]&63==5):
            setups.append((block['start_frame'],data[3:3+length]))
    if not setups or len({pdu for _,pdu in setups})!=1 or decode_called_digits(setups[0][1])!='1234567':
        raise ValueError('native outgoing SETUP omits the physically dialed number')
    report.update(native_cm_service_accept_qualified=True,accept_start_frame=start,
                  native_accept_time=native[0]['time'],native_outgoing_setup_qualified=True,
                  setup_frames=[frame for frame,_ in setups],called_number='1234567')
    return report


def incoming_setup(caller):
    """The lab's SETUP: speech bearer, signal IE and an international calling number."""
    digits=[int(d) for d in caller]+([15] if len(caller)%2 else [])
    number=bytes([0x81])+bytes(digits[i]|digits[i+1]<<4 for i in range(0,len(digits),2))
    l3=bytes.fromhex('03050404600200813401')+bytes([0x5c,len(number)])+number
    return bytes([3,0,len(l3)<<2|1])+l3+b'\x2b'*(20-len(l3))


def check(text,blocks,caller='5551234'):
    rows=list(SETUP.finditer(text))
    if len(rows)!=1 or text.count('npe3_raw_call_setup:')!=1:
        raise ValueError('missing unique raw incoming SETUP')
    row=rows[0];source,start=int(row[1]),int(row[2])
    expected=incoming_setup(caller)
    if bytes.fromhex(row[3])!=expected:
        raise ValueError(f'SETUP differs from declared speech/caller{caller} policy')
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


def check_assignment(text,blocks,*,after_connect=False,caller='5551234'):
    setup=check(text,blocks,caller)
    pattern=LATE_ASSIGNMENT if after_connect else ASSIGNMENT
    marker='npe3_raw_late_assignment:' if after_connect else 'npe3_raw_assignment:'
    rows=list(pattern.finditer(text))
    if len(rows)!=1 or text.count(marker)!=1:
        raise ValueError('missing unique raw Assignment Command')
    row=rows[0];source,start=int(row[1]),int(row[2])
    expected=bytes.fromhex(('0362' if after_connect else '0322')+'21062e094023006301'+'2b'*12)
    sources=setup['native_connect_frames'] if after_connect else setup['native_call_confirmed_frames']
    if (bytes.fromhex(row[3])!=expected or source not in sources or
            start!=((source+3)//51+1)*51+22):
        raise ValueError('assignment does not acknowledge its declared actual CC source')
    matches=[]
    for packet in check_rx_publications(text)['packets']:
        body=bytes.fromhex(packet['payload'])
        if (packet['type']==128 and len(body)==33 and body[:3]==b'\x80\x2a\0' and
                int.from_bytes(body[3:6],'big')==(start+3)%2715648 and
                body[6:8]==b'\0\x23' and body[10:]==expected and
                float(packet['time'])>(start+3)*60000/13000000):
            matches.append(packet)
    if len(matches)!=1:raise ValueError('missing unique native Assignment Command decode')
    return dict(native_assignment_decode_qualified=True,start_frame=start,after_connect=after_connect,
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


TCH_START=re.compile(r'npe3_tch_start: first_frame=(\d+) timeslot=(\d+) tsc=(\d+) ')


def check_traffic_channel(text,assignment_start):
    """The native TCH/F after an Assignment Command, from the fixture's exact decodes.

    Establishment only: FACCH SABM/UA, RR Assignment Complete and its
    acknowledgement, and SACCH Measurement Reports. Speech is not qualified."""
    from tools.rom4_lab_registration_check import FAULT,TCH_UPLINK,TCH_REPLY
    starts=TCH_START.findall(text)
    if len(starts)!=1 or text.count('npe3_tch_start:')!=1:
        raise ValueError('missing unique traffic channel start')
    first,timeslot,tsc=(int(value) for value in starts[0])
    if timeslot!=1 or tsc!=2 or first<assignment_start+4:
        raise ValueError('traffic channel does not follow the assigned TCH/F')
    after=text[text.find('npe3_tch_start:'):]
    if 'native_dsp_reset_line:' in after or FAULT.search(after):
        raise ValueError('native DSP reset or faulted on the traffic channel')
    uplinks=[(kind,int(frame),bytes.fromhex(octets),code=='1') for kind,frame,octets,code in TCH_UPLINK.findall(text)]
    if len(uplinks)!=text.count('npe3_tch_uplink:') or not all(exact for *_,exact in uplinks):
        raise ValueError('traffic-channel uplink block without an exact convolutional decode')
    facch=[(frame,block) for kind,frame,block,_ in uplinks if kind=='facch']
    sabm=[frame for frame,block in facch if block[:3]==b'\x01\x3f\x01']
    complete=[frame for frame,block in facch if block[3:6]==b'\x06\x29\x00']
    if not sabm or not complete or complete[0]<=sabm[0] or facch[0][0]!=sabm[0]:
        raise ValueError('FACCH lacks SABM followed by RR Assignment Complete')
    replies={}
    for kind,source in TCH_REPLY.findall(text):
        replies.setdefault(int(source),kind)
    # Assignment Complete is acknowledged by RR, or on a host-answered dialled
    # call by the ALERTING I-frame's N(R).
    if replies.get(sabm[0])!='ua' or replies.get(complete[0]) not in ('rr','cc_alerting'):
        raise ValueError('network FACCH replies are not bound to SABM and Assignment Complete')
    reports=[frame for kind,frame,block,_ in uplinks if kind=='sacch' and block[5:7]==b'\x06\x15']
    if len(reports)<4 or reports[0]<first:
        raise ValueError('too few SACCH Measurement Reports on the traffic channel')
    return dict(traffic_channel_qualified=True,tch_first_frame=first,sabm_frame=sabm[0],
                assignment_complete_frame=complete[0],measurement_reports=len(reports),
                assignment_complete_qualified=True,connected_call_qualified=False,
                speech_transport_qualified=False)


# Handset clearing on FACCH after physical End (GSM 04.08 5.4.3): DISCONNECT,
# the lab's RELEASE, RELEASE COMPLETE, Channel Release, then DISC/UA.
CLEARING=(('disconnect',lambda block:block[3]&15==3 and block[4]&0x3f==0x25),
          ('release_complete',lambda block:block[3]&15==3 and block[4]&0x3f==0x2a),
          ('disc',lambda block:block[:3]==b'\x01\x53\x01'))
CLEARING_REPLIES=(('cc_release','disconnect'),('channel_release','release_complete'),('disc_ua','disc'))
END_KEY=re.compile(r'npe3_native_physical: key=End t=([0-9.]+) ')


def check_traffic_clearing(text,end_at):
    """Physical End clears the call on FACCH, and TS1 carried the lab's tones both ways."""
    from tools.rom4_lab_registration_check import TCH_UPLINK,TCH_REPLY,TCH_SPEECH,check_traffic_audio
    pressed=END_KEY.findall(text)
    if len(pressed)!=1 or abs(float(pressed[0])-end_at)>0.05:
        raise ValueError('missing unique timed physical End')
    end_frame=float(pressed[0])*13_000_000/60_000
    facch=[(int(frame),bytes.fromhex(octets)) for kind,frame,octets,_ in TCH_UPLINK.findall(text) if kind=='facch']
    frames={}
    for frame,block in facch:
        for name,match in CLEARING:
            if match(block):frames.setdefault(name,frame)
    if list(frames)!=[name for name,_ in CLEARING] or not end_frame<frames['disconnect']<frames['release_complete']<frames['disc']:
        raise ValueError(f'traffic-channel clearing missing or out of order: {list(frames)}')
    if frames['disconnect']>end_frame+217:
        raise ValueError('DISCONNECT did not follow physical End within one second')
    replies={}
    for kind,source in TCH_REPLY.findall(text):
        replies.setdefault(kind,int(source))
    for kind,message in CLEARING_REPLIES:
        if replies.get(kind)!=frames[message]:
            raise ValueError(f'{kind} is not bound to the first {message}')
    result={f'tch_{name}_frame':frame for name,frame in frames.items()}
    result['end_frame']=int(end_frame)
    result['traffic_clearing_qualified']=True
    if TCH_SPEECH.search(text):
        result.update(check_traffic_audio(text))
        result['speech_transport_qualified']=True
    elif 'npe3_tch_audio:' not in text:
        raise ValueError('traffic channel did not stop after DISC')
    return result
