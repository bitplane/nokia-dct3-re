"""Own NSM-3 configuration-class evidence, not HAGAR physical field decoding."""
import hashlib
import math
import re
from tools.rom6_radio_census import WRITE
from tools.rom6_tuning_contract import KEYS, fields, rx_candidates

PREFIX_OFFSET=0x116b08
PREFIX_BASE=0x3e7e
PREFIX_WORDS=147
SOURCE_SPANS={(0x22a5,0x22b4):'8b7eb37dd0773ea8de0ab868aa5efc353b51355d17ecf8483fff5894e5597afc',
              (0x3d79,0x3d92):'225b7ee9ca41f84b45ad334b7d62014ec8ce51056e058e33f23b5b17114bbaf7'}


def observed_configuration_class(word):
    """Only the two whole words qualified in this MCU/upload composition."""
    try:
        return {0x4a800402:'GSM900',0x4a800208:'DCS1800'}[word]
    except KeyError:
        raise ValueError('unqualified configuration word; no masked-bit fallback') from None


def candidates_for_configuration(pll, configuration):
    band=observed_configuration_class(configuration)
    return [candidate for candidate in rx_candidates(pll) if candidate['band']==band]


def check_prefix(prefix, mcu):
    if len(prefix)!=PREFIX_WORDS*2 or prefix!=mcu[PREFIX_OFFSET:PREFIX_OFFSET+len(prefix)]:
        raise ValueError('configuration prefix differs from own MCU upload')
    if mcu.count(prefix)!=1:
        raise ValueError('configuration-prefix MCU identity is not unique')
    def word(address):
        offset=(address-PREFIX_BASE)*2
        return int.from_bytes(prefix[offset:offset+2],'big')
    if [word(a) for a in (0x3efc,0x3efe,0x3f0f,0x3f10)]!=[0x1054,0x082a,0x0402,0x0208]:
        raise ValueError('own route/configuration literals differ')
    return dict(dsp_words='3e7e..3f10',mcu_offset='116b08',bytes=len(prefix),
                sha256=hashlib.sha256(prefix).hexdigest(),unique_mcu_match=True)


def check(text, stream, prefix, mcu, overlay, program):
    provenance=check_prefix(prefix,mcu)
    selector=overlay[0x1990*2:0x19ae*2]
    if (len(overlay)!=0x4000 or len(program)!=0x20000 or len(selector)!=60
            or selector!=mcu[0x1161b0:0x1161b0+60] or mcu.count(selector)!=1):
        raise ValueError('own uploaded configuration selector differs')
    for (start,end),digest in SOURCE_SPANS.items():
        if hashlib.sha256(program[start*2:end*2]).hexdigest()!=digest:
            raise ValueError('resident copy/classifier source differs')
    footer=re.findall(r'rom6_config_complete: producers=(\d+) producer_cap=128 selections=(\d+) selection_cap=4096 observer_writes=0',text)
    if len(footer)!=1 or footer[0][0]!='4' or not 548<=int(footer[0][1])<4096:
        raise ValueError('missing or capped configuration capture')
    producers=[fields(row) for row in re.findall(r'rom6_config_write: ([^\r\n]+)',text)]
    if any(not {'sequence','address','value','cpu_pc','mask','observer_writes'}<=row.keys() for row in producers):
        raise ValueError('configuration producer fields incomplete')
    expected=[('3f0f','0000','000f15'),('3f10','0000','000f15'),
              ('3f0f','0402','0022af'),('3f10','0208','0022af')]
    if (len(producers)!=4 or [int(p['sequence']) for p in producers]!=[1,2,3,4]
            or [(p['address'],p['value'],p['cpu_pc']) for p in producers]!=expected
            or any(p['mask']!='ffff' or p['observer_writes']!='0' for p in producers)):
        raise ValueError('clear/upload producers differ or a new writer appeared')
    if text.count('rom6_config_prefix: base=3e7e words=147 incoming_store=3f10 observer_writes=0')!=1:
        raise ValueError('missing transaction-prefix provenance')
    events=[]
    for match in re.finditer(r'rom6_config_(class_entry|class_result|selector|select): ([^\r\n]+)',text):
        events.append((match[1],fields(match[2])))
    required={
        'class_entry':{'sequence','key','observer_writes'},
        'class_result':{'sequence','class263b','observer_writes'},
        'selector':{'sequence','old_payload','match3efc','class263b','observer_writes'},
        'select':{'sequence','t','value','word3f0f','word3f10','class263b','cpu_pc','mask','observer_writes'}}
    if any(not required[kind]<=row.keys() for kind,row in events):
        raise ValueError('configuration chain fields incomplete')
    count=int(footer[0][1])
    if len(events)!=count*4:
        raise ValueError('configuration-class chain coverage differs')
    keys=[]
    selections=[]
    for index in range(count):
        group=events[index*4:index*4+4]
        if ([kind for kind,_ in group]!=['class_entry','class_result','selector','select']
                or any(int(row['sequence'])!=index+1 or row.get('observer_writes')!='0' for _,row in group)):
            raise ValueError('configuration classification/selection reordered or incomplete')
        entry,result,selector,selected=[row for _,row in group]
        key=int(entry['key'],16)
        if key not in KEYS: raise ValueError('unqualified configuration class input')
        gsm=key<=124 or key>=975
        cls=int(result['class263b'],16)
        if bool(cls&4)!=gsm or selector['class263b']!=result['class263b'] or selected['class263b']!=result['class263b']:
            raise ValueError('native class bit differs from own key classifier')
        if (selector['old_payload']!=('1054' if gsm else '082a') or selector['match3efc']!='1054'
                or selected['value']!=('0402' if gsm else '0208')
                or selected['word3f0f']!='0402' or selected['word3f10']!='0208'
                or selected['mask']!='ffff' or selected['cpu_pc']!='00199f'):
            raise ValueError('own payload-comparison/configuration selection differs')
        keys.append(key); selections.append(selected)
    if set(keys)!=KEYS:
        raise ValueError('configuration did not cover all own RX key classes')
    pairs=[]
    low=None
    last_time=-1.0
    for line in stream:
        match=WRITE.fullmatch(line)
        if not match: raise ValueError('malformed configuration port stream')
        time,pc,port,value,mask=match.groups()
        now=float(time)
        if not math.isfinite(now) or now<last_time: raise ValueError('unordered configuration port stream')
        last_time=now
        if mask!=b'ffff': raise ValueError('partial configuration port write')
        if port==b'0031': low=int(value,16)
        elif port==b'0032':
            if low is None: raise ValueError('configuration high word has no low word')
            pairs.append((float(time),pc.decode(),low,int(value,16)))
            low=None
    configurations=[row for row in pairs if row[3]==0x4a80]
    if len(configurations)!=count:
        raise ValueError('staged/issued configuration counts differ')
    for selected,(time,pc,low,tag) in zip(selections,configurations):
        staged=float(selected['t'])
        if not math.isfinite(staged) or not 0<=staged<time or low!=int(selected['value'],16):
            raise ValueError('issued configuration differs from earlier native staging')
    tuning_keys=[int(fields(row)['a'],16) for row in re.findall(r'rom6_tuning_entry: ([^\r\n]+)',text)]
    tune_positions=[i for i,row in enumerate(pairs) if row[1]=='0038c1']
    if set(tuning_keys)!=KEYS or len(tuning_keys)!=len(tune_positions):
        raise ValueError('missing tuning/configuration correlation')
    preceding=None; next_configs={}; following=None
    for i in range(len(pairs)-1,-1,-1):
        next_configs[i]=following
        if pairs[i][3]==0x4a80: following=pairs[i][2]
    transitions=0; tuning_index=0
    for i,(time,pc,low,tag) in enumerate(pairs):
        if pc=='0038c1':
            key=tuning_keys[tuning_index]; tuning_index+=1
            expected_config=0x0208 if 512<=key<=885 else 0x0402
            transitions+=preceding!=expected_config
            if next_configs[i]!=expected_config:
                raise ValueError('next issued configuration does not match requested class')
            candidates=candidates_for_configuration((tag<<16)|low,0x4a800000|expected_config)
            if len(candidates)!=1 or candidates[0]['key']!=key:
                raise ValueError('configuration-assisted numeric inverse differs from own key')
        if tag==0x4a80: preceding=low
    return dict(upload=provenance,classifications=count,staged_and_issued=count,
        distinct_keys=len(set(keys)),config_words=['4a800402','4a800208'],
        preceding_config_wrong_class_at_pll=transitions,
        next_config_matches_tuning=len(tuning_keys),physical_field_ownership_proved=False,
        delivered_band_selection_proved=False)
