"""Bounded own NSK-3 packet activity and type8b routing, not radio acceptance."""
import argparse
from pathlib import Path
import re
import struct
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk3_static_check import inspect

FRAGMENTS={0x2ab560:'002ab58c',0x2ab58c:'1c20f7d6ff0fe048',
           0x2823b0:'b5101c04483b80201c2021a8f7fbff58200c1c21f7e4fb32bd10'}


def parse(trace):
    rows=re.findall(r'nsk3_liveness_packet: pointer=([0-9a-f]+) data=([0-9a-f]+) t=([0-9.]+) observer_writes=0',trace)
    if len(rows)!=trace.count('nsk3_liveness_packet: '):raise ValueError('malformed packet observation')
    packets=[]
    for pointer,raw,time in rows:
        data=bytes.fromhex(raw)
        if not 0x100000<=int(pointer,16)<=0x10ff00 or len(data)<4 or len(data)!=data[2]+4:
            raise ValueError('packet capture differs from declared payload length')
        packets.append({'type':data[3],'payload':data[4:],'time':float(time)})
    if [packet['type'] for packet in packets]!=[0x74,0x74,0x74,0x74,0x99,0x8b]:
        raise ValueError('bounded native packet sequence differs')
    if [packet['time'] for packet in packets]!=sorted(packet['time'] for packet in packets):
        raise ValueError('packet order differs')
    last=packets[-1]
    if len(last['payload'])!=162 or not 38<last['time']<40:
        raise ValueError('late measurement-shaped packet differs')
    prefix=int.from_bytes(last['payload'][:2],'big')
    pairs=list(struct.iter_unpack('>Hh',last['payload'][2:]))
    if prefix!=0x20 or len(pairs)!=40 or any(value!=-148 for _,value in pairs):
        raise ValueError('measurement-shaped body differs')
    return {'packet_types':[hex(p['type']) for p in packets],
            'last_seconds':last['time'],'leading_word':prefix,'serialized_pair_capacity':len(pairs),
            'signed_pair_value':-148,'channels':[channel for channel,_ in pairs],
            'radio_or_physical_units_proved':False,'own_forward_target':12,'own_forward_primitive':'0xa8'}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path)
    parser.add_argument('run',type=Path)
    args=parser.parse_args()
    try:
        _,image=inspect(args.source,'boot4')
        for address,encoded in FRAGMENTS.items():
            expected=bytes.fromhex(encoded)
            if image[address-0x200000:address-0x200000+len(expected)]!=expected:
                raise ValueError('own type8b route differs')
        trace=(args.run/'error.log').read_text()
        summary=parse(trace)
        if (args.run/'nvram/nsk3r4t/flash').read_bytes()!=image:
            raise ValueError('own executable flash changed')
        print(summary)
    except (ValueError,OSError) as error:
        print(f'NSK-3 packet census failed: {error}',file=sys.stderr)
        return 1
    print('Bounded packet census qualified; task12 continuation and radio remain open')
    return 0


if __name__=='__main__':raise SystemExit(main())
