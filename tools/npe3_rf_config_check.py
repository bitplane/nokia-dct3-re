"""Own 6210 upload -> configuration staging -> port-word provenance."""
import re
import hashlib
from tools.rom6_tuning_contract import FORMATTER_SHA256,KEYS,fields,pack_scalar

SOURCE=re.compile(r'npe3_rf_config_source: sequence=(\d+) t=([\d.]+) address=([\da-f]{4}) value=([\da-f]{4}) mask=ffff owner=([\da-f]{6,8}) observer_writes=0')
STAGE=re.compile(r'npe3_rf_config_stage: sequence=(\d+) t=([\d.]+) value=([\da-f]{4}) source0=0502 source1=0308 route=1054 mask=ffff owner=([\da-f]{6,8}) observer_writes=0')


def check_formatter(text,program,prefix,stream):
    if (len(program)!=0x20000 or hashlib.sha256(program[0x388c*2:0x3938*2]).hexdigest()!=FORMATTER_SHA256
            or len(prefix)!=294):
        raise ValueError('6210 formatter identity is not established')
    constants=[int.from_bytes(prefix[(a-0x3e7e)*2:(a-0x3e7e)*2+2],'big')
               for a in (0x3e7f,0x3e80,0x3ed4,0x3ed5)]
    if constants!=[4675,4450,8514,8039]:raise ValueError('6210 formatter addends changed')
    entries=[fields(row) for row in re.findall(r'npe3_rf_formatter: ([^\r\n]+)',text)]
    count=len(entries)
    if (not 0<count<8192 or text.count(f'npe3_rf_formatter_complete: entries={count} cap=8192 observer_writes=0')!=1
            or [int(row['sequence']) for row in entries]!=list(range(1,count+1))):
        raise ValueError('missing or capped 6210 formatter input coverage')
    stores={}
    for row in re.findall(r'npe3_rf_formatter_store: ([^\r\n]+)',text):
        item=fields(row);stores.setdefault(int(item['sequence']),[]).append(item)
    direct=[]
    for line in stream:
        stamp,pc,port,value,mask=line.split()
        if (pc,port) in (('0038bc','0031'),('0038c1','0032')):
            if mask!='ffff':raise ValueError('partial direct formatter output')
            direct.append((port,int(value,16)))
    expected_direct=[]
    keys=set();rx=tx=0
    for n,row in enumerate(entries,1):
        key=int(row['a'],16);pointer=int(row['ar2'],16);receive=row['entry']=='388e'
        if (row['entry'] not in ('388e','389e') or key not in KEYS or row['observer_writes']!='0'
                or int(row['flags'],16)&6!=4 or int(row['class'],16)&0xa2!=0xa0
                or not int(row['mode'],16)&0x80 or row['addend0']!='1243' or row['addend1']!='1162'):
            raise ValueError('6210 formatter left the evidenced numeric branch')
        gsm=not 512<=key<=885
        normalized=key-1024 if key>=975 else key
        addend=constants[(0 if receive else 1)+(0 if gsm else 2)]
        scalar=(normalized+addend)*(2 if gsm else 1)
        packed=pack_scalar(scalar)
        group=stores.pop(n,[])
        if any(float(r['t'])<float(row['t']) or (n<count and float(r['t'])>=float(entries[n]['t'])) for r in group):
            raise ValueError('buffered output is outside its formatter invocation')
        if pointer==0x31:
            if group:raise ValueError('direct formatter unexpectedly stored a buffer')
            expected_direct.extend((('0031',packed&65535),('0032',packed>>16)))
        elif ([(r['address'],r['value'],r['mask'],r['owner'],r['observer_writes']) for r in group]
              !=[(f'{pointer:04x}',f'{packed&65535:04x}','ffff','0038c7','0'),
                 (f'{pointer+1:04x}',f'{packed>>16:04x}','ffff','0038c7','0')]):
            raise ValueError('6210 buffered formatter output differs from independent arithmetic')
        if receive:rx+=1;keys.add(key)
        else:tx+=1
    if stores or direct!=expected_direct:raise ValueError('unowned or missing formatter output')
    return dict(rx_entries=rx,tx_entries=tx,observed_rx_keys=sorted(keys),
                full_key_domain_claim=False,physical_configuration_bits_proved=False,rf_readiness=False)


def check(text,prefix,mcu,stream):
    if len(prefix)!=294 or prefix!=mcu[0x2083c:0x20962] or mcu.count(prefix)!=1:
        raise ValueError('6210 RF prefix is not its unique own MCU upload')
    def word(address):
        at=2*(address-0x3e7e)
        return int.from_bytes(prefix[at:at+2],'big')
    if [word(a) for a in (0x3efc,0x3efe,0x3f0f,0x3f10)]!=[0x1054,0x082a,0x0502,0x0308]:
        raise ValueError('6210 route/configuration literals changed')
    sources=list(SOURCE.finditer(text))
    expected=[('3f0f','0000','000f14'),('3f10','0000','000f14'),
              ('3f0f','0502','0022ad'),('3f10','0308','0022ad')]
    if (len(sources)!=4 or text.count('npe3_rf_config_source:')!=4
            or [(r[3],r[4],r[5]) for r in sources]!=expected
            or [int(r[1]) for r in sources]!=list(range(1,5))):
        raise ValueError('6210 source clear/upload producers changed')
    stages=list(STAGE.finditer(text))
    count=len(stages)
    if (not 0<count<8192 or count!=text.count('npe3_rf_config_stage:')
            or [int(r[1]) for r in stages]!=list(range(1,count+1))
            or text.count(f'npe3_rf_config_complete: writes=4 write_cap=128 stages={count} stage_cap=8192 observer_writes=0')!=1):
        raise ValueError('missing, malformed or capped 6210 staging evidence')
    issued=[]
    low=None
    for line in stream:
        stamp,pc,port,value,mask=line.split()
        if port=='0031':low=(stamp,value)
        elif port=='0032' and value=='4a80':
            if low is None or mask!='ffff' or pc!='001a89':
                raise ValueError('6210 configuration port source changed')
            issued.append(low)
    if len(issued)!=count:
        raise ValueError('6210 staged/issued configuration count differs')
    for stage,(stamp,value) in zip(stages,issued):
        if stage[3] not in ('0502','0308') or stage[3]!=value or not 0<=float(stamp)-float(stage[2])<0.00001:
            raise ValueError('6210 staged configuration did not reach its port word')
    return dict(source_words=['0502','0308'],staged_and_issued=count,
                physical_bit_meaning_proved=False,rf_readiness=False)
