"""Qualify observed storage-only DSP controls, without inventing events."""
import re
import struct


def inspect_peripheral_consumers(program):
    fragments={
        0x71ee:'f27474fa771a000a',
        0x8ec5:'f07423f6f0749162f0200100',
        0x8ed8:'74f8000800358813f1200200',
        0xb792:'74f80008003576f806b20000',
        0xb81f:'4d82f49575f800080034f07335f7',
        0x917b:'10f806c6f000b3d1881174f8000800361181f5e2',
    }
    for address,encoded in fragments.items():
        expected=bytes.fromhex(encoded)
        if program[2*address:2*address+len(expected)]!=expected:
            raise ValueError(f'peripheral consumer code differs at {address:04x}')
    return dict(rejected_raw_port_read=dict(candidate='71ef',reason='74fa is CALLD destination at 71ee; 000a belongs to delay-slot STM'),
                request0100=dict(default_handler='8ec5',callee='9162',state_table='b3d1 indexed by 06c6',status_port='36'),
                request0200=dict(handlers=['8ed3','b787'],input_port='35'),
                alternate_output=dict(site='b821',port='34'),
                sampling_control_link_proved=False,physical_peripheral_identity_proved=False)


def inspect_word_port_activity(data,text):
    if len(data)!=0x20000 or struct.unpack_from('>6H',data,0xb3d1*2)!=(0x9185,0x9199,0x91b0,0x91be,0x91fb,0x9200):
        raise ValueError('word-transmit state table differs')
    writes=re.findall(r'nse2_word_port_write: port=(34|35|36) value=([0-9a-f]{4}) mask=ffff pc=([0-9a-f]{4}) n=(\d+) t=([0-9.]+) observer_writes=0',text)
    if [(p,v,pc,int(n)) for p,v,pc,n,_ in writes]!=[('36','0080','8d98',1),('36','0080','90bc',2),('34','1eff','920d',1)]:
        raise ValueError('word-port initialization/transmit sequence differs')
    summaries=re.findall(r'nse2_word_port_summary: port=(34|35|36) reads=(\d+) writes=(\d+) last=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0',text)
    if [(p,int(r),int(w),v) for p,r,w,v,_ in summaries]!=[('34',0,1,'1eff'),('35',0,0,'0000'),('36',4,2,'0080')]:
        raise ValueError('complete word-port activity summary differs')
    if not all(29.9<=float(t)<=30.1 for *_,t in summaries):
        raise ValueError('word-port observation does not cover thirty seconds')
    initial=re.findall(r'nse2_word_tx_state: state=([0-9a-f]{4}) flags=([0-9a-f]{4}) pending=([0-9a-f]{4}) pointer=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0',text)
    final=re.findall(r'nse2_word_terminal: state=([0-9a-f]{4}) flags=([0-9a-f]{4}) pending=([0-9a-f]{4}) pointer=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0',text)
    if len(initial)!=1 or initial[0][:4]!=('0001','0001','0009','0701') or len(final)!=1 or final[0][:4]!=('0002','0001','0009','0701'):
        raise ValueError('word transmitter pending-state boundary differs')
    if not 0.3<float(initial[0][-1])<0.4 or not 29.9<float(final[0][-1])<30.1:
        raise ValueError('word transmitter pending-state timing differs')
    return dict(first_word='1eff',state_after_transmit=2,pending=9,continuation='91b0',
                completion_route='request0100 via 8ec5/9162',rx_words=0,
                transmit_completion_modelled=False,physical_baud_proved=False,
                receive_supervision_fix_proved=False)


def inspect_word_control_contract(program):
    fragments={
        0x8da5:'76820700f49575820001ec0ef4957482000269820700f49575820002',
        0x90ad:'698204006882fcfff49575820002',
        0x8ec5:'f07423f6f0749162f0200100f49575f800080001',
        0x90e9:'10f806c5fe447712006074f800080036f1301000fc4d',
        0x9129:'748200366882ffe16982801cf49575820036',
        0x913c:'771200607482003661824000f820913e68827ffff49575820036',
    }
    for address,encoded in fragments.items():
        expected=bytes.fromhex(encoded)
        if program[address*2:address*2+len(expected)]!=expected:
            raise ValueError(f'word-port control contract differs at {address:04x}')
    return dict(initial_request_mask='0700',active_transmit_mask_clear='0300',
                active_transmit_mask_set='0400',tx_request='0100',
                acknowledgement_after_transmitter_call=True,completion_status_test='1000',
                reset_config_clear='001e',reset_config_set='801c',reset_wait_mask='4000',reset_release_clear='8000',
                baud_and_framing_proved=False,physical_ready_latency_proved=False)


def inspect_word_configuration_contract(program):
    """Pin decoded configuration/ISR instructions, not physical clock semantics."""
    fragments={
        0x9232:'f07423e877120060771306f3771406f4',
        0x9295:'74820036118261840300f330fe01ff30f3400180f030007ef2a08082f49575820036',
        0x92a6:'1084f030079e8083f0749009',
        0x34da:'74f8000b0036f3302000f84d34f1',
        0x34e8:'75f8000b0034',
        0x34f4:'75f800080034',
        0xffd0:'f2733416',
        0x3bb1:'81f806f410d2f130ff00f77863f8000b015bf77c81f806f6',
        0x3bc8:'80f806f5f07439004a12f07492328a12f0733924',
    }
    for address,encoded in fragments.items():
        expected=bytes.fromhex(encoded)
        if program[address*2:address*2+len(expected)]!=expected:
            raise ValueError(f'word-port configuration contract differs at {address:04x}')
    return dict(configuration_helper='9232',pending_flags='06f4',active_flags='06f3',
                control_preserve_mask='fe01',state_bits_to_control='007e',
                conditional_control_bits='0180',committed_state_mask='079e',
                interrupt_vector='ffd0',interrupt_entry='3416',
                interrupt_output_status_mask='2000',output_port='34',
                host_pending_store='3bb1',host_timing_stores=['3bbb','3bc8'],
                baud_and_framing_proved=False,completion_source_proved=False)


def inspect_word_pending_frames(data):
    """Qualify queued FBUS-shaped bytes, not electrical identity or delivery."""
    if len(data)!=0x20000:
        raise ValueError('word queue requires full DSP data image')
    pointers=struct.unpack_from('>2H',data,0x06c1*2)
    if pointers!=(0x070b,0x0701):
        raise ValueError('word queue producer/consumer pointers differ')
    queued=data[0x700*2:0x70b*2]
    frames=[]
    offset=0
    while offset<len(queued):
        if len(queued)-offset<8 or queued[offset]!=0x1e:
            raise ValueError('word queue frame header differs')
        length=int.from_bytes(queued[offset+4:offset+6],'big')
        extent=6+length+(length&1)+2
        frame=queued[offset:offset+extent]
        if len(frame)!=extent:
            raise ValueError('word queue frame is truncated')
        if length&1 and frame[6+length]!=0:
            raise ValueError('word queue padding differs')
        checks=[0,0]
        for index,value in enumerate(frame):
            checks[index&1]^=value
        if checks!=[0,0]:
            raise ValueError('word queue alternating XOR checksums differ')
        frames.append(dict(destination=f'{frame[1]:02x}',source=f'{frame[2]:02x}',
                           message_type=f'{frame[3]:02x}',payload=frame[6:6+length].hex(),
                           frame=frame.hex()))
        offset+=extent
    if [frame['frame'] for frame in frames]!=['1eff00d000030101e000ff2d','1e1400f4000103001de1']:
        raise ValueError('word queue native frame content differs')
    return dict(frames=frames,alternating_xor_valid=True,wire_delivery_proved=False,
                physical_baud_proved=False,receive_supervision_link_proved=False)


def inspect_word_service_observation(text):
    """Qualify the bounded nine-second fetch/store correlation, not silicon timing."""
    fetches=re.findall(r'nse2_word_service_fetch_summary: address=([0-9a-f]{4}) reads=(\d+) exact=(\d+) plusone=(\d+) other=(\d+) observer_writes=0',text)
    expected=[('3924','46','0','46','0'),('3f4e','1247','0','1247','0'),
              ('8dd0','1247','0','1247','0'),('9232','0','0','0','0')]
    if fetches!=expected:
        raise ValueError('word service fetch/PC observation differs')
    increments=re.findall(r'nse2_word_service_increment_summary: address=([0-9a-f]{4}) increments=(\d+) min=([0-9.]+) max=([0-9.]+) observer_writes=0',text)
    if [(a,n) for a,n,_,_ in increments]!=[('06b3','1247'),('06c4','1247')]:
        raise ValueError('word service increment coverage differs')
    for _,_,minimum,maximum in increments:
        if not (0.00006<float(minimum)<0.00007 and 0.169<float(maximum)<0.170):
            raise ValueError('word service variable cadence differs')
    return dict(entry_fetches=1247,counter_increments=1247,observed_fetch_pc_delta=1,
                fixed_counter_period_proved=False,physical_completion_proved=False)


def inspect_word_host_command(program,data):
    """Resolve the host command using data-space, never program-space tables."""
    if len(data)!=0x20000:
        raise ValueError('word host command requires full DSP data space')
    fragments={
        0x398d:'f000b00f4a1188117710000110818a11f4e2',
        0x3924:'f07423d461f806e10004f820394ff0748dd0',
        0x3f4e:'f07423d4f0748dd077100001e7368814fc45',
        0x8e4b:'12f806f5f150fffffa4d8e690af806b3',
        0x8e6e:'6bf806c400016bf806b30001',
        0x9185:'13f806c40bf806f6f84b9191',
    }
    for address,encoded in fragments.items():
        expected=bytes.fromhex(encoded)
        if program[address*2:address*2+len(expected)]!=expected:
            raise ValueError(f'word host command code differs at {address:04x}')
    entries=struct.unpack_from('>82H',data,0xb00f*2)
    if [index for index,target in enumerate(entries) if target==0x3b92]!=[0x4c]:
        raise ValueError('word configuration command table differs')
    return dict(command='4c',handler='3b92',table='data:b00f',
                duration_comparison='06f5 against 06b3; ffff bypasses',
                start_comparison='06c4 against 06f6',
                counter_increment_site='8e6e',physical_counter_period_proved=False)


def inspect_peripheral_routes(program):
    if len(program)!=0x20000:
        raise ValueError('peripheral routes require full live page-zero program')
    words=struct.unpack('>65536H',program)
    routes=[(address,words[address+1],words[address+3])
            for address in range(0x35c0,0x35f7)
            if words[address]==0xf130 and words[address+2]==0xf84c]
    expected=[(0x35c0,0x1000,0x369a),(0x35c4,0x1c,0x362f),
              (0x35cd,0x100,0x8ec5),(0x35d1,0x200,0x8ed3),
              (0x35d7,0x100,0xb7dc),(0x35db,0x200,0xb787),
              (0x35df,0x400,0x8e89),(0x35e3,0x80,0x371d),
              (0x35e7,2,0x3621),(0x35eb,1,0x362d),
              (0x35ef,0x20,0x3764),(0x35f3,0x40,0x3764)]
    if routes!=expected:
        raise ValueError('peripheral request dispatcher route census differs')
    if words[0x35c8:0x35cd]!=(0x61f8,0x06f3,0x0400,0xf830,0x35d7):
        raise ValueError('alternate peripheral handler selector differs')
    return dict(routes=[dict(test=f'{a:04x}',mask=f'{m:04x}',target=f'{t:04x}') for a,m,t in routes],
                alternate_selector='06f3 mask 0400',sampling_source_assignment_proved=False)


def inspect_sampling_controls(program,text):
    fragments={
        0x447e:'491109f82193f84a448501f800eef49575f8000b000a',
        0x448b:'f1203a98f49575f8000b000b',
        0x4494:'75f800110009',
    }
    for address,encoded in fragments.items():
        expected=bytes.fromhex(encoded)
        if program[2*address:2*address+len(expected)]!=expected:
            raise ValueError(f'sampling control code differs at {address:04x}')
    rows=re.findall(r'nse2_sampling_control: port=(09|0a|0b) value=([0-9a-f]{4}) mask=ffff n=(\d+) pc=([0-9a-f]{4}) mode=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0',text)
    summaries=re.findall(r'nse2_sampling_summary: port=(09|0a|0b) writes=(\d+) last=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0',text)
    if len(summaries)!=3 or {p for p,_,_,_ in summaries}!={'09','0a','0b'}:
        raise ValueError('sampling summaries missing or duplicated')
    counts={}
    for port,count,last,time in summaries:
        count=int(count)
        observed=[row for row in rows if row[0]==port]
        if count<12 or [int(row[2]) for row in observed]!=list(range(1,13)) or not 29.9<=float(time)<=30.1:
            raise ValueError('sampling control prefix/count coverage differs')
        expected={'09':'0332','0a':'028f','0b':'3a98'}[port]
        if last!=expected:
            raise ValueError('terminal sampling control differs')
        counts[port]=count
    if counts['0a']!=counts['0b'] or counts['0a']!=2*counts['09']-3:
        raise ValueError('sampling control activation/deactivation accounting differs')
    scan=[row for row in rows if row[4]=='000c']
    if [(p,v,pc) for p,v,_,pc,_,_ in scan[:3]]!=[('0a','028f','4489'),('0b','3a98','4491'),('09','0332','4497')]:
        raise ValueError('first scan sampling-control sequence differs')
    return dict(sampling_control_writes=counts,scan_controls={'09':'0332','0a':'028f','0b':'3a98'},
                sampling_events_implemented=False,physical_semantics_proved=False)
