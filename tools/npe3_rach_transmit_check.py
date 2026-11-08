"""Join native RF port transactions to independently checked access bits."""
import csv
import io
import math
import re
from tools.gsm_rach_signal import decode_p27_access
from tools.gsm_normal_signal import decode_p27_normal,decode_control
from tools.rom6_adc_measure_check import check_rx_publications

WINDOW=re.compile(r'npe3_tx_window: sequence=(\d+) t=([\d.]+) words=(\d+) counter=([0-9a-f]{4}) controls=([0-9a-f,]+) deadlines=([\d.,-]+) observer_writes=0 physical_claim=0')
RELEASE=re.compile(r'npe3_tx_release: sequence=(\d+) t=([\d.]+) target=([0-9a-f]{4}) counter=([0-9a-f]{4}) words=([0-9a-f,]+) network_reply=0 physical_claim=0')
AGCH=re.compile(r'npe3_raw_agch: ra=([0-9a-f]{2}) access_frame=(\d+) start_frame=(\d+) octets=([0-9a-f]{46}) raw_adc_only=1 tuned_air_source=0 physical_claim=0')


def check_agch(text, *, require_native=True,allow_normal=False):
    """Check external response content independently of the C++ decoder/BTS."""
    releases=[]
    normal=0
    for release in RELEASE.finditer(text):
        words=[int(word,16) for word in release[5].split(',')]
        try:decode_p27_access(words,42)
        except ValueError:
            if not allow_normal:raise
            decode_p27_normal(words)
            normal+=1
        else:releases.append(release)
    replies=list(AGCH.finditer(text))
    if not replies or len(replies)!=len(releases) or len(replies)!=text.count('npe3_raw_agch:'):
        raise ValueError('raw AGCH lacks one valid response per released access burst')
    if allow_normal and text.count('npe3_raw_agch_skip: not_access=1 response=0')!=normal:
        raise ValueError('normal transmit buffers unexpectedly generated an access response')
    starts=[]
    expected_native=[]
    for release,reply in zip(releases,replies):
        words=[int(word,16) for word in release.group(5).split(',')]
        ra=decode_p27_access(words,42)['ra']
        frame=int(float(release.group(2))*13000000)//60000
        got_ra,got_frame,start,payload=reply.groups()
        start=int(start)
        expected_end=frame+4
        while expected_end%51 not in (9,15,19,25,29,35,39,45,49):expected_end+=1
        t1=(frame//1326)&31;t2=frame%26;t3=frame%51
        expected=bytes([0x2d,6,0x3f,0,0x20,0,35,ra,(t1<<3)|(t3>>3),
                        ((t3<<5)|t2)&255,0,0]+[0x2b]*11)
        if (reply.start()<release.end() or int(got_ra,16)!=ra or int(got_frame)!=frame or
                start!=expected_end-3 or bytes.fromhex(payload)!=expected):
            raise ValueError('raw AGCH differs from independent access bits/time or CCCH grammar')
        starts.append(start)
        expected_native.append((start+3,expected,float(release.group(2))))
    native=[]
    if require_native:
        packets=check_rx_publications(text)['packets']
        for frame,payload,release_time in expected_native:
            matches=[]
            for packet in packets:
                body=bytes.fromhex(packet['payload'])
                if (packet['type']==0x80 and len(body)==33 and body[0]==0x60 and
                        body[2]==0 and int.from_bytes(body[3:6],'big')==frame%2715648 and
                        body[10:]==payload and float(packet['time'])>release_time):
                    matches.append(packet)
            if len(matches)!=1:
                raise ValueError('missing unique successful native raw-AGCH publication')
            native.append(matches[0]['time'])
    return dict(external_access_decode_qualified=True,responses=len(replies),
                agch_start_frames=starts,native_agch_decode_qualified=require_native,
                native_publication_times=native,
                tuned_air_source=False,physical_release_qualified=False)


def transactions(writes):
    result=[]
    active=None
    previous=-1
    compares={port:0 for port in range(5,9)}
    for row in csv.reader(io.StringIO(writes),delimiter='\t'):
        if len(row)!=5: raise ValueError('malformed DSP I/O journal')
        stamp=float(row[0])
        if not math.isfinite(stamp) or stamp<previous:
            raise ValueError('unordered DSP I/O journal')
        previous=stamp
        port,value,mask=(int(field,16) for field in row[2:])
        if port in compares:
            if mask!=0xffff: raise ValueError('partial projected compare write')
            compares[port]=value
            continue
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
            result.append((active[0],stamp,active[1],tuple(compares.values())))
            active=None
    if active is not None: raise ValueError('incomplete RF serializer transaction')
    if not result: raise ValueError('no complete RF serializer transaction')
    return result


def check_windows(text,bursts,frames):
    rows=list(WINDOW.finditer(text))
    if len(rows)!=len(bursts) or len(rows)!=text.count('npe3_tx_window:'):
        raise ValueError('incomplete projected transmit windows')
    source_frames=[]
    p05_deadlines=[]
    for index,(row,burst) in enumerate(zip(rows,bursts),1):
        number,stamp,count,counter,controls,deadlines=row.groups()
        stamp=float(stamp);counter=int(counter,16)
        controls=tuple(int(word,16) for word in controls.split(','))
        due=tuple(float(word) for word in deadlines.split(','))
        if (int(number)!=index or int(count)!=15 or stamp!=burst[1] or controls!=burst[3] or
                len(controls)!=4 or len(due)!=4 or not 0<=counter<5000 or
                any(not 0<=target<5000 for target in controls) or
                any(not math.isfinite(at) or at<=stamp for at in due)):
            raise ValueError('transmit window differs from actual buffer/control publication')
        distances=[(target-counter)%5000 or 5000 for target in controls]
        phases=[distance*12/13000000-(at-stamp) for distance,at in zip(distances,due)]
        if (min(phases)<-2e-9 or max(phases)>12/13000000+2e-9 or
                max(phases)-min(phases)>2e-9 or not due[2]<due[0]<due[1]<due[3]):
            raise ValueError('projected transmit controls do not share the declared counter phase')
        source_frames.append(int(due[0]*13000000)//60000%2715648)
        p05_deadlines.append(due[0])
    if source_frames!=frames:
        raise ValueError('P05 source-frame projection differs from native RA_INFO')
    return dict(windows=len(rows),p05_source_frames=source_frames,p05_deadlines=p05_deadlines,
                declared_counter_hz='13000000/12',physical_release_qualified=False)


def check_releases(text,bursts,windows):
    identity='npe3_tx_candidate: p05_start=1 port_buffer_only=1 network_reply=0 physical_claim=0'
    rows=list(RELEASE.finditer(text))
    deadlines=windows.get('p05_deadlines',[])
    if (text.count(identity)!=1 or text.count('npe3_tx_candidate:')!=1 or
            len(rows)!=len(bursts) or len(deadlines)!=len(bursts) or len(rows)!=text.count('npe3_tx_release:')):
        raise ValueError('missing explicit complete P05 laboratory releases')
    for index,(row,burst,due) in enumerate(zip(rows,bursts,deadlines),1):
        sequence,stamp,target,counter,words=row.groups()
        if (int(sequence)!=index or target!=counter or int(target,16)!=burst[3][0] or
                tuple(int(word,16) for word in words.split(','))!=tuple(burst[2]) or
                not burst[1]<float(stamp) or abs(float(stamp)-due)>1e-9):
            raise ValueError('timed candidate release differs from native buffer or counter deadline')
    return dict(timed_candidate_release_qualified=True,releases=len(rows),
                network_reply=False,physical_release_qualified=False)


def check(text,writes,bsic=42,*,require_windows=False,require_releases=False,allow_normal=False):
    all_bursts=transactions(writes)
    access_indices=[]
    normal=[]
    for index,burst in enumerate(all_bursts):
        try:decode_p27_access(burst[2],bsic)
        except ValueError:
            if not allow_normal:raise
            normal.append((index,decode_p27_normal(burst[2])))
        else:access_indices.append(index)
    bursts=[all_bursts[index] for index in access_indices]
    if not bursts:raise ValueError('no independently decoded native access burst')
    if allow_normal and not require_releases:
        raise ValueError('mixed transmit qualification requires timed releases')
    reports=[packet for packet in check_rx_publications(text)['packets'] if packet['type']==0x84]
    if len(reports)!=len(bursts): raise ValueError('native RA_INFO/serializer count differs')
    decoded=[]
    frames=[]
    for (start,end,words,_),report in zip(bursts,reports):
        body=bytes.fromhex(report['payload'])
        result=decode_p27_access(words,bsic)
        if (len(body)!=4 or body[0]!=result['ra'] or
                not float(report['time'])<=start<end or
                int.from_bytes(body[1:],'big')>=2715648):
            raise ValueError('native RA_INFO differs from independently decoded port data')
        decoded.append(result)
        frames.append(int.from_bytes(body[1:],'big'))
    result=dict(native_access_serialization_qualified=True,attempts=len(bursts),
                random_access_octets=[row['ra'] for row in decoded],reported_frames=frames,
                framing_offsets=[row['framing_offset'] for row in decoded],
                physical_release_qualified=False,network_decode_qualified=False,
                camping_qualified=False,registration_qualified=False)
    if require_windows or require_releases:
        window_frames=frames
        if allow_normal:
            releases=list(RELEASE.finditer(text))
            if len(releases)!=len(all_bursts):raise ValueError('mixed transmit release count differs')
            window_frames=[int(float(row[2])*13000000)//60000%2715648 for row in releases]
            if [window_frames[index] for index in access_indices]!=frames:
                raise ValueError('timed native access frames differ from RA_INFO')
        result['candidate_windows']=check_windows(text,all_bursts,window_frames)
    if require_releases:
        result['candidate_releases']=check_releases(text,all_bursts,result['candidate_windows'])
    elif 'npe3_tx_release:' in text or 'npe3_tx_candidate:' in text:
        raise ValueError('no-release control unexpectedly enabled the transmit candidate')
    if allow_normal:
        if not normal or len(normal)%4:
            raise ValueError('native normal transmit stream lacks complete four-burst blocks')
        blocks=[]
        for at in range(0,len(normal),4):
            group=normal[at:at+4]
            indices=[item[0] for item in group]
            source_frames=[window_frames[index] for index in indices]
            if (indices!=list(range(indices[0],indices[0]+4)) or
                    source_frames!=[(source_frames[0]+offset)%2715648 for offset in range(4)]):
                raise ValueError('normal block buffers are not consecutive timed frames')
            payload=decode_control([item[1] for item in group])
            blocks.append(dict(start_frame=source_frames[0],octets=payload.hex()))
        result['normal_control_blocks']=blocks
        result['native_port_xcch_qualified']=True
    return result
