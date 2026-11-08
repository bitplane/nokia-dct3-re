"""Qualify the bounded raw-SI3 laboratory decode, not camping or tuning."""
import csv
import re
from tools.gsm_xcch_signal import control_bits
from tools.rom6_adc_measure_check import check_rx_publications

SI3=bytes.fromhex('49061b000100f11000014000000000000000002b2b2b2b')
FIRE=re.compile(r'npe3_bcch_fire: sequence=(\d+) t=([\d.]+) status=([0-9a-f]{4}) frame=([0-9a-f]{4}) words=([0-9a-f,]+) read_owner=([0-9a-f]{6}) observer_writes=0')


def expected_fire_words():
    bits=control_bits(SI3)
    bits=bits[:184]+tuple(bit^1 for bit in bits[184:224])
    return tuple(sum(bits[16*word+bit]<<bit for bit in range(16)) for word in range(14))


def check(text,writes):
    expected=expected_fire_words()
    controls,words=[],[]
    transactions=[]
    for row in csv.reader(writes.splitlines(),delimiter='\t'):
        if len(row)!=5: raise ValueError('malformed DSP I/O journal')
        stamp=float(row[0])
        if not 3.2<=stamp<=4: continue
        port,value,mask=(int(field,16) for field in row[2:])
        if port not in (0x38,0x39,0x3c): continue
        if mask!=0xffff: raise ValueError('partial FIRE bus write')
        if port==0x3c:
            if value==3: controls,words=[],[]
            controls.append(value)
        elif port==0x39: words.append(value)
        elif value&~0x10==5:
            if controls!=[3,5,0xfffc,2] or tuple(words)!=expected:
                raise ValueError('native FIRE transaction differs from independently encoded SI3/parity')
            transactions.append(stamp)
    all_rows=list(FIRE.finditer(text))
    if len(all_rows)!=text.count('npe3_bcch_fire:') or any(int(row[1])!=index for index,row in enumerate(all_rows,1)):
        raise ValueError('malformed FIRE read observations')
    rows=[row for row in all_rows if row[6]=='003ae7']
    if len(rows)!=3 or len(transactions)!=3:
        raise ValueError('missing three complete BCCH FIRE transactions/results')
    packets=[packet for packet in check_rx_publications(text)['packets']
             if packet['type']==0x80 and packet['payload'].startswith('50') and float(packet['time'])<4]
    if len(packets)!=3: raise ValueError('missing three native BCCH publications')
    for index,(row,transaction,packet) in enumerate(zip(rows,transactions,packets),1):
        sequence,stamp,status,frame,words,owner=row.groups()
        body=bytes.fromhex(packet['payload'])
        if (int(status,16)&0x60!=0x20 or
                tuple(int(word,16) for word in words.split(','))!=expected or
                len(body)!=33 or body[:4]!=bytes.fromhex('502a0000') or body[10:]!=SI3 or
                int(frame,16)!=719+51*(index-1) or int.from_bytes(body[4:6],'big')!=int(frame,16) or
                not transaction<=float(stamp)<=float(packet['time'])):
            raise ValueError('native BCCH result,parity,frame or publication differs')
    return dict(native_laboratory_bcch_qualified=True,blocks=3,bursts=12,
                information_bits_per_block=184,parity_bits_per_block=40,octets=SI3.hex(),
                first_block_frame=719,last_block_frame=821,
                physical_capture_qualified=False,camping_qualified=False,registration_qualified=False)
