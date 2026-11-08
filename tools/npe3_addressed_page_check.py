"""Check external paging inputs separately from native wake/camping results."""
import re
from tools.rom6_tuning_contract import fields

from tools.npe3_raw_ua_check import UPLINK,DISC_UA
from tools.radio_mobile_identity import paging_group
from tools.rom6_adc_measure_check import check_rx_publications


REQUEST=re.compile(r'npe3_bts_page_request: t=([\d.]+) identity=([0-9a-f]{16}) phase=(\d+) offset=(\d+) multiframes=2 after_frame=(\d+) octets=([0-9a-f]{46}) raw_adc_only=1 port_bits_identity=1 physical_claim=0')
AIR=re.compile(r'npe3_bts_page_air: start_frame=(\d+) t=([\d.]+) raw_adc_only=1 physical_claim=0')


def check_controller_lifecycle(text,page,image):
    if (image[0x115262:0x115276].hex()!='2004468120019002a0b1f15bf98c980271a0e0e3'
            or image[0x1153c0:0x1153c2].hex()!='71a5'):
        raise ValueError('controller flag writer instructions changed')
    rows=[fields(row) for row in re.findall(r'npe3_controller_transition: ([^\r\n]+)',text)]
    count=len(rows)
    if (not 0<count<256 or [int(r['sequence']) for r in rows]!=list(range(1,count+1))
            or text.count(f'npe3_controller_transition_complete: changes={count} cap=256 end=24 observer_writes=0')!=1):
        raise ValueError('missing or capped controller transition coverage')
    last={}
    for row in rows:
        name=row['field'];old=int(row['old'],16);new=int(row['new'],16)
        if (name not in ('procedure','pending','selector','continuation') or old==new
                or row['observer_writes']!='0' or not 0<=float(row['t'])<=24
                or name in last and last[name]!=old):
            raise ValueError('discontinuous controller field history')
        last[name]=new
    procedure=[r for r in rows if r['field']=='procedure']
    expected=[('00000000','00000001','00315272'),('00000001','00000000','003153c0'),
              ('00000000','00000001','00315272')]
    if ([(r['old'],r['new'],r['pc']) for r in procedure]!=expected
            or any(r['r4']!='00169be8' for r in procedure) or procedure[1]['saved']!='000007ef'
            or not float(procedure[0]['t'])<float(procedure[1]['t'])<float(procedure[2]['t'])<min(page['native_decode_times'])):
        raise ValueError('controller did not clear and re-enter its page-deferring condition')
    pending=[r for r in rows if r['field']=='pending']
    if (len(pending)!=1 or pending[0]['saved']!='0000083e' or pending[0]['pc']!='0031547a'
            or not min(page['native_decode_times'])<float(pending[0]['t'])<min(page['native_decode_times'])+0.01):
        raise ValueError('first addressed page did not set the pending latch')
    return dict(flag_set_times=[float(procedure[i]['t']) for i in (0,2)],
                flag_clear_time=float(procedure[1]['t']),clear_input='07ef',
                missing_initial_clear_disproved=True,procedure_semantics_qualified=False,
                stable_camping_qualified=False)

# Own controller: store/receive,three paging selectors,latch/free/repeat;
# later pending-page drain and its two context-byte guards.
CONTROLLER_CODE={
    0x31543e:'a0d8f15bf8a24648603870a52000f0b7fc9e90022000468149e098024288d00949df98024288d0052021018099024281d000e4c8a02ff15bf888200172e06aa0f0adfaf3200062a020044681e7d8',
    0x315816:'7ae02801d000e542a055f15afeb272e59801742048579002a043f15afeaa6aa0f0adf917200062a0a041f15afea22001f78afadb2801d10778e02800d10479a02801d001f7fdfdad',
    0x3133b8:'a0def15df8e575e520807320',
    0x3157d8:'0000083e0000083f',
    0x315988:'0000083e',
}


def check(text,requested_time):
    sources=list(re.finditer(r'rf_adc_broadcast_si: index=2 arfcn=35 octets=([0-9a-f]{46}) physical_claim=0',text))
    if len(sources)!=1:raise ValueError('missing unique raw SI3 paging configuration')
    si3=bytes.fromhex(sources[0][1])
    if si3[:3]!=b'\x49\x06\x1b' or si3[10]&0x3f!=1 or si3[11]&7:
        raise ValueError('raw SI3 does not advertise combined CCCH,reserved0,multiframes2')
    requests=list(REQUEST.finditer(text))
    if len(requests)!=1 or text.count('npe3_bts_page_request:')!=1 or 'npe3_bts_page_ignored:' in text:
        raise ValueError('missing unique accepted external BTS page input')
    request=requests[0]
    stamp=float(request[1]);identity=request[2]
    phase,offset=paging_group(identity,combined=True)
    octets=bytes.fromhex(request[6])
    if (abs(stamp-requested_time)>0.02 or (int(request[3]),int(request[4]))!=(phase,offset) or
            int(request[5])!=int(stamp*13_000_000//60_000)+1 or
            octets!=b'\x31\x06\x21\x10\x08'+bytes.fromhex(identity)+b'\x2b'*10):
        raise ValueError('addressed page differs from valid IMSI/PCH group grammar')
    sources=[]
    for row in UPLINK.finditer(text):
        block=bytes.fromhex(row[2])
        if block[:5]==b'\x01\x3f\x49\x05\x08' and block[12]==8 and block[13:21].hex()==identity:
            sources.append(row)
    closes=[row for row in DISC_UA.finditer(text) if row.end()<request.start()]
    if not sources or sources[0].end()>request.start() or not closes:
        raise ValueError('page identity is not from a preceding released air Location Updating request')
    if stamp< (int(closes[-1][2])+4)*60_000/13_000_000:
        raise ValueError('BTS page precedes completion of its raw closing UA')
    air=list(AIR.finditer(text))
    if len(air)!=text.count('npe3_bts_page_air:'):
        raise ValueError('truncated paging air-source observations')
    previous=-1
    for row in air:
        frame=int(row[1]);time=float(row[2]);position=time*13_000_000/60_000
        if (frame<=previous or frame<int(request[5]) or frame%51!=offset or (frame//51)%2!=phase or
                not frame-1e-5<=position<frame+4 or row.start()<request.end()):
            raise ValueError('paging air samples differ from the declared DRX schedule')
        previous=frame
    publications=[]
    for packet in check_rx_publications(text)['packets']:
        body=bytes.fromhex(packet['payload'])
        if packet['type']==128 and len(body)==33 and body[:3]==b'\x60\x2a\0' and body[10:]==octets:
            if not air or float(packet['time'])<float(air[0][2]):
                raise ValueError('addressed page decoded before sampled external source')
            publications.append(float(packet['time']))
    return dict(identity=identity,phase=phase,offset=offset,multiframes=2,
                octets=octets.hex(),source_identity_qualified=True,
                observed_air_blocks=len(air),native_publications=len(publications),
                native_decode_times=publications,
                native_addressed_decode_qualified=bool(publications),
                native_paging_response_qualified=False,stable_camping_qualified=False,
                physical_carrier_selection_qualified=False)


def check_mcu_admission(text,page):
    """Join actual published bytes to the MCU's completed group decision."""
    prefix='npe3_paging_admission: '
    records=[]
    for match in re.finditer(re.escape(prefix)+r'([^\r\n]+)',text):
        pairs=[token.split('=',1) for token in match[1].split()]
        if any(len(pair)!=2 for pair in pairs):raise ValueError('malformed MCU paging admission')
        row=dict(pairs)
        required={'entry','hit','t','r0','r1','r2','r5','r6','r7','controller','control','group','object','observer_writes'}
        if len(row)!=len(pairs) or not required<=row.keys() or row['observer_writes']!='0':
            raise ValueError('incomplete/mutating MCU paging admission')
        records.append(row)
    if len(records)!=text.count(prefix):raise ValueError('truncated MCU paging admission')
    footers=list(re.finditer(r'npe3_paging_admission_complete: entry=([0-9a-f]{8}) hits=(\d+) cap=32 end=24 observer_writes=0',text))
    expected={0x457db8,0x457dd0,0x457e2a,0x457e6e,0x457e80,0x457ebe,0x457eec,0x457a32,0x327b5e}
    if (len(footers)!=len(expected) or len(footers)!=text.count('npe3_paging_admission_complete:') or
            {int(row[1],16) for row in footers}!=expected or
            any(int(row['entry'],16) not in expected or not 0<=float(row['t'])<=24 for row in records)):
        raise ValueError('missing MCU paging admission coverage footer')
    for footer in footers:
        rows=[row for row in records if int(row['entry'],16)==int(footer[1],16)]
        if len(rows)!=int(footer[2]) or len(rows)>32 or [int(row['hit']) for row in rows]!=list(range(1,len(rows)+1)):
            raise ValueError('truncated/duplicate MCU paging admission records')
    native=[packet for packet in check_rx_publications(text)['packets']
            if packet['type']==128 and len(bytes.fromhex(packet['payload']))==33 and
            bytes.fromhex(packet['payload'])[:3]==b'\x60\x2a\0' and
            bytes.fromhex(packet['payload'])[10:].hex()==page['octets']]
    admitted=0
    for packet in native:
        body=bytes.fromhex(packet['payload']);stamp=float(packet['time'])
        path=[]
        for entry in (0x457db8,0x457dd0,0x457e2a,0x457e80,0x457a32):
            hits=[row for row in records if int(row['entry'],16)==entry and
                  stamp<float(row['t'])<=stamp+0.005 and bytes.fromhex(row['object'])[4:]==body]
            if len(hits)!=1:raise ValueError('missing unique MCU admission for a native addressed page')
            path.append(hits[0])
        group,decision,forward=path[2:]
        if ([float(row['t']) for row in path]!=sorted(float(row['t']) for row in path) or
                any(int(row['controller'],16) in (0,3) for row in path) or
                group['group']!=f'0000{page["phase"]:02x}' or
                int(decision['r6'],16)!=1 or int(decision['r7'],16)!=int.from_bytes(body[3:6],'big') or
                int(forward['r1'],16)!=0 or int(forward['r2'],16)!=1):
            raise ValueError('MCU rejected or misclassified the addressed paging group')
        admitted+=1
    if not admitted:raise ValueError('no native addressed pages reached MCU admission')
    return dict(native_mcu_paging_admission_qualified=True,admitted=admitted,
                forwarder='457a32',target_task=15,message='0623',
                native_paging_response_qualified=False,stable_camping_qualified=False)


def check_rr_delivery(text,page):
    """Qualify delivery/parsing,not the controller's decision to answer."""
    posts=list(re.finditer(r'npe3_paging_post: sequence=(\d+) t=([\d.]+) target=(15|18) header=(0623|05eb) object=([0-9a-f]{8}) payload=([0-9a-f]{8}) observer_writes=0',text))
    parsed=list(re.finditer(r'npe3_paging_parser_return: sequence=(\d+) t=([\d.]+) event=([0-9a-f]{4}) message=([0-9a-f]{8}) context=([0-9a-f]{8}) observer_writes=0',text))
    footer=list(re.finditer(r'npe3_paging_delivery_complete: posts=(\d+) post_cap=8 parsed=(\d+) parse_cap=8 end=24 observer_writes=0',text))
    if (len(footer)!=1 or text.count('npe3_paging_delivery_complete:')!=1 or
            len(posts)!=text.count('npe3_paging_post:') or len(parsed)!=text.count('npe3_paging_parser_return:') or
            (len(posts),len(parsed))!=(int(footer[0][1]),int(footer[0][2])) or
            not 0<len(posts)<=8 or not 0<len(parsed)<=8 or
            [int(row[1]) for row in posts]!=list(range(1,len(posts)+1)) or
            [int(row[1]) for row in parsed]!=list(range(1,len(parsed)+1))):
        raise ValueError('incomplete RR paging delivery observations')
    stamps=page['native_decode_times']
    if len(posts)!=2*len(stamps) or len(parsed)!=len(stamps):
        raise ValueError('paging publications/posts/parser-return counts differ')
    for index,stamp in enumerate(stamps):
        first,second=posts[2*index:2*index+2];result=parsed[index]
        if ((first[3],first[4])!=('15','0623') or (second[3],second[4])!=('18','05eb') or
                not stamp<float(first[2])<float(second[2])<float(result[2])<=stamp+0.005 or
                result[3]!='083e' or result[4]!=second[5] or int(second[6],16)<0x100000):
            raise ValueError('native page did not reach the ordinary RR parser in order')
    return dict(native_rr_paging_parse_qualified=True,parsed=len(parsed),
                task15_message='0623',task18_message='05eb',parser_event='083e',
                native_paging_response_qualified=False,stable_camping_qualified=False)


def check_controller(text,page,image):
    """Pin the observed deferral,without inventing the current phase's meaning."""
    for address,encoded in CONTROLLER_CODE.items():
        expected=bytes.fromhex(encoded);offset=address-0x200000
        if image[offset:offset+len(expected)]!=expected:
            raise ValueError(f'own paging-controller instructions differ at{address:06x}')
    records=[]
    for match in re.finditer(r'npe3_paging_wait: ([^\r\n]+)',text):
        pairs=[token.split('=',1) for token in match[1].split()]
        if any(len(pair)!=2 for pair in pairs):raise ValueError('malformed paging continuation')
        row=dict(pairs)
        required={'entry','hit','t','r0','r4','r7','r9','saved','peer','context','observer_writes'}
        if len(row)!=len(pairs) or not required<=row.keys() or row['observer_writes']!='0':
            raise ValueError('incomplete/mutating paging continuation')
        records.append(row)
    if len(records)!=text.count('npe3_paging_wait:'):raise ValueError('truncated paging continuation')
    footers=list(re.finditer(r'npe3_paging_wait_complete: entry=([0-9a-f]{8}) hits=(\d+) cap=64 begin=12 end=24 observer_writes=0',text))
    entries={0x31543e,0x315450,0x315472,0x314e04,0x3135c2}
    if (len(footers)!=len(entries) or len(footers)!=text.count('npe3_paging_wait_complete:') or
            {int(row[1],16) for row in footers}!=entries or
            any(int(row['entry'],16) not in entries or not 12<=float(row['t'])<=24 for row in records)):
        raise ValueError('missing paging continuation coverage')
    for footer in footers:
        rows=[row for row in records if int(row['entry'],16)==int(footer[1],16)]
        if (len(rows)!=int(footer[2]) or len(rows)>64 or
                [int(row['hit']) for row in rows]!=list(range(1,len(rows)+1))):
            raise ValueError('capped/duplicate paging continuation')
    deferred=0
    for stamp in page['native_decode_times']:
        path=[]
        for entry in (0x315450,0x315472,0x31543e):
            hits=[row for row in records if int(row['entry'],16)==entry and stamp<float(row['t'])<=stamp+0.005]
            if len(hits)!=1:raise ValueError('missing unique page return/latch/receive sequence')
            row=hits[0];context=bytes.fromhex(row['context'])
            if (len(context)!=48 or row['r4']!='00169be8' or row['r7']!='00169c14' or
                    int(row['peer'],16)!=4 or context[6]!=1 or
                    entry!=0x315450 and int(row['saved'],16)!=0x83e):
                raise ValueError('page follows a different controller context')
            path.append((row,context))
        returned,selected,repeated=path
        if (not float(returned[0]['t'])<float(selected[0]['t'])<float(repeated[0]['t']) or
                int(returned[0]['r0'],16)!=0x83e or int(selected[0]['r0'],16)!=0x83e or
                not int.from_bytes(selected[1][40:44],'big') or
                repeated[1][11]!=1 or any(repeated[1][40:44]) or int(repeated[0]['r9'],16)!=4):
            raise ValueError('native page was not latched/freed before state4 receive')
        deferred+=1
    if not deferred:raise ValueError('no native paging-controller deferral observed')
    return dict(deferred_pages=deferred,continuation='31543e',selector='315472',
                pending_byte='169bf3',context_byte6=1,continuation_state=4,
                pending_drain='315816',phase_semantics_qualified=False,
                native_paging_response_qualified=False,stable_camping_qualified=False)
