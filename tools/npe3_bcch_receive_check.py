"""Qualify the bounded raw-SI3 laboratory decode, not camping or tuning."""
import csv
import re
from tools.gsm_xcch_signal import control_bits,broadcast_octets,broadcast_si_index
from tools.rom6_adc_measure_check import check_rx_publications

SI3=bytes.fromhex('49061b000100f11000014000000000000000002b2b2b2b')
FIRE=re.compile(r'npe3_bcch_fire: sequence=(\d+) t=([\d.]+) status=([0-9a-f]{4}) frame=([0-9a-f]{4}) words=([0-9a-f,]+) read_owner=([0-9a-f]{6}) observer_writes=0')


def expected_fire_words(octets=SI3):
    bits=control_bits(octets)
    bits=bits[:184]+tuple(bit^1 for bit in bits[184:224])
    return tuple(sum(bits[16*word+bit]<<bit for bit in range(16)) for word in range(14))


def fire_transactions(writes,last_time=4):
    controls,words=[],[]
    transactions=[]
    for row in csv.reader(writes.splitlines(),delimiter='\t'):
        if len(row)!=5: raise ValueError('malformed DSP I/O journal')
        stamp=float(row[0])
        if not 3.2<=stamp<=last_time: continue
        port,value,mask=(int(field,16) for field in row[2:])
        if port not in (0x38,0x39,0x3c): continue
        if mask!=0xffff: raise ValueError('partial FIRE bus write')
        if port==0x3c:
            if value==3: controls,words=[],[]
            controls.append(value)
        elif port==0x39: words.append(value)
        elif value&~0x10==5:
            transactions.append((stamp,tuple(controls),tuple(words)))
    return transactions


def check(text,writes):
    expected=expected_fire_words()
    transactions=fire_transactions(writes)
    if any(controls!=(3,5,0xfffc,2) or words!=expected for _,controls,words in transactions):
        raise ValueError('native FIRE transaction differs from independently encoded SI3/parity')
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
                not transaction[0]<=float(stamp)<=float(packet['time'])):
            raise ValueError('native BCCH result,parity,frame or publication differs')
    return dict(native_laboratory_bcch_qualified=True,blocks=3,bursts=12,
                information_bits_per_block=184,parity_bits_per_block=40,octets=SI3.hex(),
                first_block_frame=719,last_block_frame=821,
                physical_capture_qualified=False,camping_qualified=False,registration_qualified=False)


def check_broadcast(text,writes,*,dynamic_frames=False,combined_ccch=False):
    """Join the first complete SI set; do not classify later CCCH failures."""
    transactions=fire_transactions(writes,8)
    rows=list(FIRE.finditer(text))
    if len(rows)!=text.count('npe3_bcch_fire:') or any(int(row[1])!=index for index,row in enumerate(rows,1)):
        raise ValueError('malformed broadcast FIRE observations')
    packets=[packet for packet in check_rx_publications(text)['packets']
             if packet['type']==0x80 and packet['payload'].startswith('50')]
    frames=(719,770,821,872)
    if dynamic_frames:
        frames=[]
        seen=set()
        for packet in packets:
            body=bytes.fromhex(packet['payload'])
            if len(body)!=33 or body[2]!=0:continue
            frame=int.from_bytes(body[3:6],'big')
            frames.append(frame)
            seen.add(broadcast_si_index(frame))
            if len(seen)==4:break
        if len(seen)!=4:raise ValueError('dynamic capture lacks a complete successful SI cycle')
    kinds=[]
    for index,frame in enumerate(frames):
        kind=broadcast_si_index(frame)
        octets=broadcast_octets(kind,combined_ccch=combined_ccch)
        expected=expected_fire_words(octets)
        candidates=[row for row in rows if row[6]=='003ae7' and int(row[4],16)==frame]
        publications=[packet for packet in packets if int.from_bytes(bytes.fromhex(packet['payload'])[4:6],'big')==frame]
        if len(candidates)!=1 or len(publications)!=1:
            raise ValueError('missing complete native SI broadcast block')
        result,packet=candidates[0],publications[0]
        before=[transaction for transaction in transactions if transaction[0]<=float(result[2])]
        body=bytes.fromhex(packet['payload'])
        if (not before or before[-1][1:]!=((3,5,0xfffc,2),expected) or
                int(result[3],16)&0x60!=0x20 or tuple(int(word,16) for word in result[5].split(','))!=expected or
                len(body)!=33 or body[:4]!=bytes.fromhex('502a0000') or body[10:]!=octets or
                not float(result[2])<=float(packet['time'])):
            raise ValueError('broadcast block/parity/native publication differs')
        kinds.append(kind)
    for handler in ('003b9fd0','003b9ed4','003ba0c0','003ba060'):
        if not re.search(r'plmn_si3_parse: pc='+handler+r' hit=1 ',text):
            raise ValueError('missing MCU SI type consumer')
    if not re.search(r'plmn_si3_parse: pc=002d3e3c hit=\d+ r0=00000001 state=0002 flags=0f ',text):
        raise ValueError('complete broadcast set did not reach MCU acquisition-success return')
    return dict(native_complete_broadcast_qualified=True,blocks=len(frames),bursts=4*len(frames),
                si_indices=kinds,information_bits_per_block=184,parity_bits_per_block=40,
                mcu_si_consumers_qualified=True,mcu_acquisition_return=1,
                physical_capture_qualified=False,camping_qualified=False,registration_qualified=False)
