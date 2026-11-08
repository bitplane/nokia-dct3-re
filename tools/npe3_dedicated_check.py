"""Read-only MCU evidence; submitting L3 to the DSP is not RF transmission."""
import math
import re

from tools.npe3_rach_transmit_check import check_agch

COMMIT=re.compile(r'npe3_mcu_tx_commit: sequence=(\d+) t=([\d.]+) previous=([0-9a-f]{4}) producer=([0-9a-f]{4}) words=([0-9a-f,]+) pc_context=([0-9a-f]{8}) observer_writes=0')
COMPLETE=re.compile(r'npe3_mcu_tx_complete: commits=(\d+) cap=256 invalid=0 begin=0 end=24 observer_writes=0')


def packets(text):
    rows=list(COMMIT.finditer(text))
    complete=COMPLETE.findall(text)
    if (len(complete)!=1 or len(rows)!=int(complete[0]) or not 0<len(rows)<=256 or
            len(rows)!=text.count('npe3_mcu_tx_commit:') or 'npe3_mcu_tx_invalid:' in text):
        raise ValueError('missing complete early-through-dedicated MCU publication window')
    previous=0
    previous_time=-1
    result=[]
    for index,row in enumerate(rows,1):
        sequence,stamp,start,end,words,_=row.groups()
        stamp=float(stamp);start=int(start,16);end=int(end,16)
        words=[int(word,16) for word in words.split(',')]
        if (int(sequence)!=index or not math.isfinite(stamp) or not previous_time<=stamp<=24 or
                not 0<=start<82 or not 0<=end<82 or start!=previous or
                (end-start)%82!=len(words) or not words or any(word>65535 for word in words)):
            raise ValueError('inconsistent MCU ring publication')
        size,kind=words[0]>>8,words[0]&255
        if len(words)!=1+(size+1)//2:
            raise ValueError('MCU packet length differs from committed words')
        body=b''.join(word.to_bytes(2,'big') for word in words[1:])[:size]
        result.append(dict(time=stamp,kind=kind,body=body))
        previous=end;previous_time=stamp
    return result


def check(text):
    agch=check_agch(text)
    after=float(agch['native_publication_times'][-1])
    stream=packets(text)
    configure=[p for p in stream if p['time']>after and p['kind']==2 and
               len(p['body'])==24 and p['body'][8]==0x80]
    if len(configure)!=1:
        raise ValueError('missing unique assigned-SDCCH configuration after native AGCH')
    outgoing=[p for p in stream if p['time']>configure[0]['time'] and p['kind']==0x1b and
              len(p['body'])==28 and p['body'][:2]==bytes([0,0x80])]
    if len(outgoing)!=1:
        raise ValueError('missing unique native SEND_BLOCK for assigned SDCCH')
    lapdm=outgoing[0]['body'][2:25]
    length=lapdm[2]>>2
    layer3=lapdm[3:3+length]
    if (lapdm[0]!=1 or lapdm[1]!=0x3f or lapdm[2]&3!=1 or
            length!=18 or layer3[:2]!=bytes([5,8]) or layer3[9]!=8):
        raise ValueError('assigned SEND_BLOCK is not SABM with a Location Updating Request')
    return dict(mcu_assignment_acceptance_qualified=True,
                configuration_time=configure[0]['time'],send_block_time=outgoing[0]['time'],
                lapdm=lapdm.hex(),location_updating_request=layer3.hex(),
                native_dedicated_transmission_qualified=False,registration_qualified=False)
