"""Own 6210 upload -> configuration staging -> port-word provenance."""
import re

SOURCE=re.compile(r'npe3_rf_config_source: sequence=(\d+) t=([\d.]+) address=([\da-f]{4}) value=([\da-f]{4}) mask=ffff owner=([\da-f]{6,8}) observer_writes=0')
STAGE=re.compile(r'npe3_rf_config_stage: sequence=(\d+) t=([\d.]+) value=([\da-f]{4}) source0=0502 source1=0308 route=1054 mask=ffff owner=([\da-f]{6,8}) observer_writes=0')


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
