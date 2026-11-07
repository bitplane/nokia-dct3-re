"""Check partial command-state receipts against independent native bus evidence."""
from collections import Counter
import re

from tools.rom6_config_contract import candidates_for_configuration
from tools.rom6_timed_output_check import OUTPUT

RECEIPT=re.compile(r'hagar_command: sequence=(\d+) control=([0-9a-f]{4}) word=([0-9a-f]{8}) seen=(\d+) route=([0-9a-f]{4}) pll=([0-9a-f]{4}) configuration=([0-9a-f]{4}) candidate=([01]) key=(\d+) frequency=(\d+) rf_ready=0 t=(\d+\.\d{9})')


def check(text):
    rows=list(RECEIPT.finditer(text))
    edges=list(OUTPUT.finditer(text))
    if len(rows)!=text.count('hagar_command:') or len(rows)!=len(edges) or not rows:
        raise ValueError('missing, malformed or capped HAGAR command receipts')
    route=pll=configuration=seen=0
    keys=set()
    states=Counter()
    for ordinal,(row,edge) in enumerate(zip(rows,edges),1):
        sequence,control,word,actual_seen,actual_route,actual_pll,actual_configuration,candidate,key,frequency,time=row.groups()
        source_sequence,source_control,low,high,target,counter,pending,source_time=edge.groups()
        raw=int(word,16)
        if (int(sequence)!=ordinal or sequence!=source_sequence or control!=source_control
                or time!=source_time or raw!=(int(high,16)<<16)|int(low,16)):
            raise ValueError('HAGAR receipt is not its exact raw compare tuple')
        tag=raw>>16
        if control!='0100': seen=0
        elif tag==0x4100: route=raw&0xffff; seen|=1
        elif tag==0x4400: pll=raw&0xffff; seen|=2
        elif tag==0x4a80: configuration=raw&0xffff; seen|=4
        elif tag!=0x4680: seen=0
        if (int(actual_seen),int(actual_route,16),int(actual_pll,16),int(actual_configuration,16))!=(seen,route,pll,configuration):
            raise ValueError('HAGAR observed command state differs from independent replay')
        expected=[]
        if seen==7 and (route,configuration) in ((0x1054,0x0402),(0x082a,0x0208)):
            try: expected=candidates_for_configuration(0x44000000|pll,0x4a800000|configuration)
            except ValueError: pass
        if len(expected)>1: raise ValueError('ambiguous joint command-state candidate')
        expected_key=expected[0]['key'] if expected else 0
        expected_frequency=expected[0]['rf_hz'] if expected else 0
        if (int(candidate),int(key),int(frequency))!=(int(bool(expected)),expected_key,expected_frequency):
            raise ValueError('HAGAR arithmetic candidate differs or uses a fallback')
        if expected: keys.add(expected_key)
        states['candidate' if expected else 'unresolved']+=1
    return dict(delivered_compares=len(rows),candidate_keys=sorted(keys),states=dict(states),
                physical_latch_proved=False,rf_readiness=False,samples_supplied=False)
