"""Own-ROM CCCH formatting/filtering and host scalar contract; observation only."""
import hashlib
import re
from tools.rom6_adc_measure_check import check_rx_publications
from tools.npe3_dedicated_check import packets as mcu_packets
from tools.gsm_xcch_signal import paging_fill_octets

SIGNATURES={
    0x2ecd:'f000b0414a1188117710000110818a11f4e2',
    0x35c5:'71da0578f0742e2ff0732e64',
    0x3d4e:'44f8057a40f80578',
    0x3d63:'30f8057844f8057af462f788',
    0x71d8:'f074059861f83eb50010fc2044f8057840f8058531f83eb8f77801838183fc00',
    0x79ef:'61f83eb50001f82079fc11f805786ff805790e20f84579fc6d90',
    0x7aea:'10f8057880f80579',
    0x4ee5:'72132f1e77122f5b6c834ef3771309127714250af2734ef911f8250e771324567714261611f826181a8480926ff82f200c48',
    0x4f1d:'61f83eb50200f8204f2c61f805860001f8204f2ce5106b920002f0734f2de5188192',
    0x5127:'10f8051df030004061840003e901f845515ef830515e6ff82f610c58f0100021',
    0x515a:'6d946c8d5147491361f8264e0030f820516768f8264effcfe901fe0073132f0a',
}
SCALAR_REFERENCES=(0x35c6,0x3d51,0x3d64,0x71df,0x79f5,0x7aeb)


def static_contract(program,drom):
    if len(program)!=131072 or len(drom)!=131072:raise ValueError('expected full native program/data snapshots')
    for address,encoded in SIGNATURES.items():
        expected=bytes.fromhex(encoded)
        if program[2*address:2*address+len(expected)]!=expected:
            raise ValueError(f'native idle contract code differs at{address:04x}')
    if int.from_bytes(drom[2*0xb08b:2*0xb08c],'big')!=0x35c5:
        raise ValueError('host4a dispatch descriptor differs')
    references=tuple(at//2 for at in range(0x4000,len(program),2) if program[at:at+2]==b'\x05\x78')
    if references!=SCALAR_REFERENCES:raise ValueError('scalar literal-reference census differs')
    return dict(host_scalar_handler='35c5',literal_reference_sites=len(references),
                scalar_read_sites=5,scalar_write_sites=1,indirect_writer_closure=False,
                program_sha256=hashlib.sha256(program).hexdigest())


def rows(text,name):
    prefix='npe3_idle_'+name+': '
    required={
        'format':{'sequence','t','owner','low','mask','flags','selector','primary','secondary','mode','status','receiver'},
        'scalar':{'sequence','t','read','value','mask'},
        'publish_gate':{'sequence','t','read','result','mask','mode','controls','payload'},
        'observer_complete':{'formats','format_cap','scalars','scalar_cap','gates','gate_cap','window_end'},
    }[name]
    result=[]
    for match in re.finditer(re.escape(prefix)+r'([^\r\n]+)',text):
        pairs=[token.split('=',1) for token in match[1].split()]
        if any(len(pair)!=2 for pair in pairs):raise ValueError('malformed idle observation')
        fields=dict(pairs)
        if len(fields)!=len(pairs) or fields.get('observer_writes')!='0' or not required<=fields.keys():
            raise ValueError('incomplete/duplicate/mutating idle observation')
        result.append(fields)
    if len(result)!=text.count(prefix):raise ValueError('incomplete idle observations')
    for index,row in enumerate(result,1):
        if 'sequence' in row and int(row['sequence'])!=index:raise ValueError('idle sequence differs')
    return result


def check(text,program,drom):
    report=static_contract(program,drom)
    formats=rows(text,'format');scalars=rows(text,'scalar');gates=rows(text,'publish_gate')
    totals=rows(text,'observer_complete')
    if len(totals)!=1:raise ValueError('missing idle observation footer')
    total=totals[0]
    for key,observed,cap in (('formats',formats,128),('scalars',scalars,64),('gates',gates,256)):
        cap_name={'formats':'format_cap','scalars':'scalar_cap','gates':'gate_cap'}[key]
        if int(total[key])!=len(observed) or int(total[cap_name])!=cap or not 0<len(observed)<=cap:
            raise ValueError('truncated idle observation')
    if total['window_end']!='24':raise ValueError('idle observation horizon differs')
    final=[];adjustments=0
    for index,row in enumerate(formats):
        owner=int(row['owner'],16);low=int(row['low'],16)
        flags=[int(value,16) for value in row['flags'].split(',')]
        if len(flags)!=2:raise ValueError('missing formatter flag pair')
        selected=row['secondary'] if int(row['selector'],16) else row['primary']
        high,source_low=(int(value,16) for value in selected.split(','))
        adjusted=bool(flags[0]&0x200 and flags[1]&1)
        if (int(row['mask'],16)!=65535 or float(row['t'])>24 or
                owner not in ((0x4f27,0x4f28) if adjusted else (0x4f2c,)) or
                low!=((source_low+(2 if owner==0x4f28 else 0))&65535)):
            raise ValueError('native formatter differs from selected frame/conditional adjustment')
        if owner==0x4f27:
            if index+1>=len(formats):raise ValueError('missing final frame adjustment')
            following=formats[index+1]
            if following['owner']!='004f28' or any(row[key]!=following[key] for key in
                    ('selector','primary','secondary','flags','mode','status','receiver')):
                raise ValueError('frame adjustment context changes')
            continue
        adjustments+=int(adjusted)
        final.append((row,((high&255)<<16)|low))
    if len(final)!=len(gates):raise ValueError('formatter/paging gate counts differ')
    accepted=[];filtered=0
    for (formatted,frame),gate in zip(final,gates):
        body=bytes.fromhex(gate['payload']);value=int(gate['result'],16)
        if (int(gate['mask'],16)!=65535 or int(gate['read'],16)!=0x5168 or
                len(body)!=33 or body[0]!=0x60 or int.from_bytes(body[3:6],'big')!=frame or
                body[2]!=int(formatted['status'],16) or
                not float(formatted['t'])<=float(gate['t'])<=24 or not 0<=value<=65535):
            raise ValueError('native paging gate differs from formatter/return stage')
        if value:accepted.append((body,float(gate['t'])))
        elif body[2]==0 and body[10:]==paging_fill_octets():
            controls=[int(value,16) for value in gate['controls'].split(',')]
            if len(controls)!=2 or not controls[1]&0x40 or int(gate['mode'],16)!=0x10:
                raise ValueError('no-identity suppression lacks active native filter')
            filtered+=1
    native=[packet for packet in check_rx_publications(text)['packets'] if
            packet['type']==128 and packet['payload'].startswith('60') and float(packet['time'])<=24]
    if len(native)!=len(accepted):raise ValueError('native publication does not match gate decisions')
    for packet,(body,stamp) in zip(native,accepted):
        if bytes.fromhex(packet['payload'])!=body or float(packet['time'])<stamp:
            raise ValueError('gate/publication content or order differs')
    writes=[row for row in scalars if int(row['read'],16)==0x35c5]
    commands=[packet for packet in mcu_packets(text) if packet['kind']==0x4a and packet['time']<=24]
    if len(writes)!=len(commands) or not writes or not filtered or not adjustments:
        raise ValueError('missing scalar command/filtered fill/adjusted-frame witness')
    for command,write in zip(commands,writes):
        if (len(command['body'])!=2 or int(write['mask'],16)!=65535 or
                int(write['value'],16)!=int.from_bytes(command['body'],'big') or
                not command['time']<=float(write['t'])<=24):
            raise ValueError('host scalar write differs from actual command')
    report.update(frame_adjustments=adjustments,formatted_blocks=len(final),
                  native_publications=len(accepted),filtered_valid_fill=filtered,
                  scalar_commands=len(commands),observation_end=24,
                  source_frame_binding_qualified=False,stable_camping_qualified=False)
    return report
