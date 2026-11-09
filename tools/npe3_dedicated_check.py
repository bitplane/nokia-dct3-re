"""Dedicated-channel evidence; submitting L3 to the DSP is not RF transmission."""
import math
import re

from tools.npe3_rach_transmit_check import check_agch

COMMIT=re.compile(r'npe3_mcu_tx_commit: sequence=(\d+) t=([\d.]+) previous=([0-9a-f]{4}) producer=([0-9a-f]{4}) words=([0-9a-f,]+) pc_context=([0-9a-f]{8}) observer_writes=0')
COMPLETE=re.compile(r'npe3_mcu_tx_complete: commits=(\d+) cap=(\d+) invalid=0 begin=0 end=(\d+) observer_writes=0')


def check_sacch(text,ms_power_level):
    from tools.rom6_adc_measure_check import check_rx_publications
    expected=[]
    for index in (1,2):
        rows=re.findall(rf'rf_adc_broadcast_si: index={index} arfcn=35 octets=([0-9a-f]{{46}}) physical_claim=0',text)
        if len(rows)!=1:raise ValueError('SACCH lacks unique source-cell SI2/SI3')
        expected.append(bytes.fromhex(rows[0]))
    si2,si3=expected
    blocks=[bytes([ms_power_level,0,3,3,0x49,6,0x1d])+si2[3:19],
            bytes([ms_power_level,0,3,3,0x2d,6,0x1e])+si3[3:10]+bytes([si3[13],0xff])+b'\x2b'*7]
    pattern=re.compile(r'npe3_bts_sacch: index=([01]) starts=42 period=102 octets=([0-9a-f]{46}) tsc=0 raw_adc_only=1 physical_claim=0')
    rows=list(pattern.finditer(text))
    if (len(rows)!=2 or len(rows)!=text.count('npe3_bts_sacch:') or
            [(int(row[1]),bytes.fromhex(row[2])) for row in rows]!=list(enumerate(blocks))):
        raise ValueError('SACCH source differs from cell data and physical header policy')
    good=[];failed=0;types=set()
    for packet in check_rx_publications(text)['packets']:
        body=bytes.fromhex(packet['payload'])
        if packet['type']!=128 or body[:1]!=b'\x70':continue
        if len(body)!=33 or body[1]!=42:raise ValueError('malformed SACCH publication')
        if body[2]:failed+=1;continue
        frame=int.from_bytes(body[3:6],'big');kind=(frame//102)&1
        if (frame%102!=45 or body[6:8]!=b'\0\x23' or body[10:]!=blocks[kind] or
                float(packet['time'])<=frame*60000/13000000):
            raise ValueError('native SACCH differs from the exact scheduled SI5/SI6 waveform')
        good.append(frame);types.add(kind)
    if len(good)<2 or types!={0,1}:raise ValueError('native SI5/SI6 SACCH reception incomplete')
    return dict(native_sacch_decode_qualified=True,successful_end_frames=good,
                failed_blocks=failed,si_types=[5,6],physical_rf_tuning_qualified=False,
                stable_camping_qualified=False)


def packets(text):
    rows=list(COMMIT.finditer(text))
    complete=COMPLETE.findall(text)
    if (len(complete)!=1 or
            not 24<=int(complete[0][2])<=120 or
            int(complete[0][1])!=(256 if int(complete[0][2])==24 else 2048) or
            len(rows)!=int(complete[0][0]) or not 0<len(rows)<=int(complete[0][1]) or
            len(rows)!=text.count('npe3_mcu_tx_commit:') or 'npe3_mcu_tx_invalid:' in text):
        raise ValueError('missing complete early-through-dedicated MCU publication window')
    previous=0
    previous_time=-1
    result=[]
    for index,row in enumerate(rows,1):
        sequence,stamp,start,end,words,_=row.groups()
        stamp=float(stamp);start=int(start,16);end=int(end,16)
        words=[int(word,16) for word in words.split(',')]
        if (int(sequence)!=index or not math.isfinite(stamp) or not previous_time<=stamp<=int(complete[0][2]) or
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


def check(text,*,allow_normal=False):
    agch=check_agch(text,allow_normal=allow_normal)
    after=float(agch['native_publication_times'][-1])
    stream=packets(text)
    configure=[p for p in stream if p['time']>after and p['kind']==2 and
               len(p['body'])==24 and p['body'][8]==0x80]
    if len(configure)!=1:
        raise ValueError('missing unique assigned-SDCCH configuration after native AGCH')
    outgoing=[p for p in stream if p['time']>configure[0]['time'] and p['kind']==0x1b and
              len(p['body'])==28 and p['body'][:2]==bytes([0,0x80])]
    if not outgoing or not allow_normal and len(outgoing)!=1:
        raise ValueError('missing native initial SEND_BLOCK for assigned SDCCH')
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
