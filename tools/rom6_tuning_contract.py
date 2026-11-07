"""Own NSM-3 RX word arithmetic, not a validated RF-wire decoder."""
import hashlib
import re
from tools.rom6_radio_census import WRITE

KEYS=set(range(125))|set(range(512,886))|set(range(975,1024))
SELECTED={1,124,512,838,885,975,1023}
FORMATTER_SHA256='37dc843e29d036630964d06e8d310074afd2734095f1036d9dc39d2e9243e22b'


def pack_scalar(value):
    if not 38<=value<=32805:
        raise ValueError('scalar outside the bounded positive formatter domain')
    quotient,remainder=divmod(value-38,64)
    return (0x4400<<16)+((quotient<<7)|remainder)+38


def unpack_scalar(word):
    if not 0<=word<=0xffffffff or word>>16!=0x4400:
        raise ValueError('unsupported word format, not the observed 4400 branch')
    raw=(word&0xffff)-38
    if raw<0 or raw&0x40:
        raise ValueError('noncanonical divider packing')
    scalar=(raw>>7)*64+(raw&63)+38
    if pack_scalar(scalar)!=word:
        raise ValueError('divider packing does not round-trip')
    return scalar


def rx_scalar(key):
    if key not in KEYS:
        raise ValueError('outside the observed RX scan key domain')
    if 512<=key<=885:
        return key+8514
    return 2*((key-1024 if key>=975 else key)+4675)


def rx_candidates(word):
    """Return every matching numeric RX interpretation, never a fallback."""
    scalar=unpack_scalar(word)
    result=[]
    if not scalar%2:
        normalized=scalar//2-4675
        if 0<=normalized<=124 or -49<=normalized<0:
            result.append(dict(band='GSM900',key=normalized%1024,rf_hz=scalar*100000))
    key=scalar-8514
    if 512<=key<=885:
        result.append(dict(band='DCS1800',key=key,rf_hz=scalar*200000))
    return result


def fields(row):
    pairs=re.findall(r'(\w+)=([^\s]+)',row)
    result=dict(pairs)
    if len(result)!=len(pairs): raise ValueError('duplicate tuning fields')
    return result


def check(text, stream, program):
    if len(program)!=0x20000 or hashlib.sha256(program[0x388c*2:0x3938*2]).hexdigest()!=FORMATTER_SHA256:
        raise ValueError('not the pinned resident formatter')
    footer=re.findall(r'rom6_tuning_complete: entries=(\d+) cap=65536 observer_writes=0',text)
    if len(footer)!=1 or not 548<=int(footer[0])<65536:
        raise ValueError('tuning observation absent or capped')
    entries=[fields(row) for row in re.findall(r'rom6_tuning_entry: ([^\r\n]+)',text)]
    entry_fields={'sequence','address','a','ar2','flags3e7e','flags3ec8','flags051f','addend3e7f','addend3e80','observer_writes'}
    if any(set(entry)!=entry_fields for entry in entries):
        raise ValueError('incomplete tuning entry fields')
    if len(entries)!=int(footer[0]) or [int(e['sequence']) for e in entries]!=list(range(1,len(entries)+1)):
        raise ValueError('tuning entries missing, duplicated or unordered')
    stages={}
    for row in re.findall(r'rom6_tuning_stage: ([^\r\n]+)',text):
        item=fields(row)
        if set(item)!={'sequence','address','a','b','ar2','ar3','field60','st0','st1','observer_writes'}:
            raise ValueError('incomplete tuning stage fields')
        stages.setdefault(int(item['sequence']),[]).append(item)
    if set(stages)-set(range(1,len(entries)+1)):
        raise ValueError('tuning stages have no owning entry')
    emitted=[]
    for line in stream:
        match=WRITE.fullmatch(line)
        if not match: raise ValueError('malformed native port stream')
        time,pc,port,value,mask=match.groups()
        if (pc,port) in ((b'0038bc',b'0031'),(b'0038c1',b'0032')):
            if mask!=b'ffff': raise ValueError('partial native formatter write')
            emitted.append((int(port,16),int(value,16)))
    if len(emitted)!=2*len(entries):
        raise ValueError('native formatter entry/port-pair coverage differs')
    observed=set()
    detailed=0
    for index,entry in enumerate(entries):
        sequence=index+1
        key=int(entry['a'],16)
        if (entry.get('observer_writes')!='0' or entry['address']!='388c' or entry['ar2']!='0031'
                or int(entry['flags3e7e'],16)&6!=4 or int(entry['flags3ec8'],16)&0xa2!=0xa0
                or not int(entry['flags051f'],16)&0x80
                or entry['addend3e7f']!='1243' or entry['addend3e80']!='1162'):
            raise ValueError('entry left the qualified RX formatter branch')
        scalar=rx_scalar(key); packed=pack_scalar(scalar); observed.add(key)
        if emitted[index*2:index*2+2]!=[(0x31,packed&0xffff),(0x32,packed>>16)]:
            raise ValueError('own native port pair differs from RX arithmetic')
        selected=sequence<=4 or key in SELECTED
        group=stages.get(sequence,[])
        if not selected:
            if group: raise ValueError('unexpected unselected detailed trace')
            continue
        detailed+=1
        if [s['address'] for s in group]!=['38aa','38c9','38de','38f4','38c8']:
            raise ValueError('selected formatter stages missing or reordered')
        gsm=not 512<=key<=885
        normalized=key-1024 if key>=975 else key
        addend=4675 if gsm else 8514
        quotient,remainder=divmod(scalar-38,64)
        expected=[(normalized&0xffffffffff,addend,normalized&0xffff),
                  (normalized+addend,addend,normalized&0xffff),
                  (scalar,int(gsm),normalized&0xffff),
                  (remainder,packed,quotient),(packed,packed,quotient)]
        for stage,(a,b,scratch) in zip(group,expected):
            if (stage.get('observer_writes')!='0' or int(stage['a'],16)!=a
                    or int(stage['b'],16)!=b or int(stage['field60'],16)!=scratch
                    or int(stage['ar3'],16)!=int(gsm)
                    or int(stage['ar2'],16)!=(0x33 if stage['address']=='38c8' else 0x31)):
                raise ValueError('native formatter stages differ from independent arithmetic')
    if observed!=KEYS:
        raise ValueError('incomplete native RX key coverage')
    return dict(entries=len(entries),distinct_keys=len(observed),actual_port_pairs=len(entries),
        detailed_paths=detailed,format='4400',wire_validated=False,
        physical_band_selection_validated=False,rf_delivery_validated=False,
        collision_48c8=rx_candidates(0x440048c8),
        unmatched_9044=rx_candidates(0x44009044))
