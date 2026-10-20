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
           0x2823b0:'b5101c04483b80201c2021a8f7fbff58200c1c21f7e4fb32bd10',
           0x2824a4:'000013b8',0x21a2a4:'0021a2ea'}


def parse(trace):
    rows=re.findall(r'nsk3_liveness_packet: pointer=([0-9a-f]+) data=([0-9a-f]+) t=([0-9.]+) observer_writes=0',trace)
    if len(rows)!=trace.count('nsk3_liveness_packet: '):raise ValueError('malformed packet observation')
    packets=[]
    for pointer,raw,time in rows:
        data=bytes.fromhex(raw)
        if not 0x100000<=int(pointer,16)<=0x10ff00 or len(data)<4 or len(data)!=data[2]+4:
            raise ValueError('packet capture differs from declared payload length')
        packets.append({'type':data[3],'payload':data[4:],'time':float(time),'pointer':pointer})
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
            'last_seconds':last['time'],'last_packet_pointer':last['pointer'],
            'leading_word':prefix,'serialized_pair_capacity':len(pairs),
            'signed_pair_value':-148,'channels':[channel for channel,_ in pairs],
            'radio_or_physical_units_proved':False,'own_forward_target':12,
            'own_forward_header':'0x13b8','trace_label_not_primitive':'0xa8'}


def check_delivery(trace):
    packet=parse(trace)
    forward=re.findall(r'nsk3_packet_forward: target=([0-9a-f]+) pointer=([0-9a-f]+) header=([0-9a-f]+) cycles=([0-9a-f]+) observer_writes=0',trace)
    receive=re.findall(r'nsk3_packet_receive: task=([0-9a-f]+) pointer=([0-9a-f]+) header=([0-9a-f]+) caller=([0-9a-f]+) cycles=([0-9a-f]+) observer_writes=0',trace)
    consume=re.findall(r'nsk3_packet_consumer: pointer=([0-9a-f]+) header=([0-9a-f]+) owner=([0-9a-f]+) state=([0-9a-f]+) phase=([0-9a-f]+) cache=([0-9a-f]+) cycles=([0-9a-f]+) observer_writes=0',trace)
    pointer=packet['last_packet_pointer']
    if (len(forward)!=1 or len(receive)!=1 or len(consume)!=1
            or forward[0][:3]!=('0c',pointer,'13b8a28b')
            or receive[0][:4]!=('0c',pointer,'13b8a28b','0021a1dd')
            or consume[0][:2]!=(pointer,'13b8a28b')):
        raise ValueError('same-object task12 delivery/dispatch differs')
    times=[int(forward[0][3],16),int(receive[0][4],16),int(consume[0][6],16)]
    if not times[0]<times[1]<times[2]:raise ValueError('delivery/dispatch ordering differs')
    return {'owner':consume[0][2],'state':consume[0][3],'phase':consume[0][4],
            'cache':consume[0][5],'header':'0x13b8','task':12,'dispatch_case':'0x21a2ea'}


def check_cache(trace):
    delivery=check_delivery(trace)
    copies=re.findall(r'nsk3_packet_cache: pointer=([0-9a-f]+) source=([0-9a-f]+) data=([0-9a-f]+) cycles=([0-9a-f]+) observer_writes=0',trace)
    parks=re.findall(r'nsk3_packet_park: state=([0-9a-f]+) phase=([0-9a-f]+) cache=([0-9a-f]+) cycles=([0-9a-f]+) observer_writes=0',trace)
    if len(copies)!=1 or len(parks)!=1:raise ValueError('cache/park observation missing or repeated')
    pointer,source,data,cycle=copies[0]
    packets=re.findall(r'nsk3_liveness_packet: pointer=([0-9a-f]+) data=([0-9a-f]+)',trace)
    if (source!=packets[-1][0] or data!=packets[-1][1][12:]
            or not 0x100000<=int(pointer,16)<0x10ff00
            or delivery['state']!='03' or delivery['phase']!='01'
            or parks[0][:3]!=('03','01',pointer)
            or int(cycle,16)>=int(parks[0][3],16)):
        raise ValueError('cache body or parked continuation differs')
    return {'cached_pair_bytes':len(bytes.fromhex(data)),'processing_skipped_in_state3':True}


def check_controller(trace):
    """Qualify the observed transaction boundary, not its success semantics."""
    sends=re.findall(r'nsk3_043c_send: caller=([0-9a-f]+) pointer=([0-9a-f]+) cycles=([0-9a-f]+) observer_writes=0',trace)
    results=re.findall(r'nsk3_controller_result: value=([0-9a-f]+) cycles=([0-9a-f]+) observer_writes=0',trace)
    waits=re.findall(r'nsk3_controller_wait: owner=([0-9a-f]+) selector=([0-9a-f]+) status=([0-9a-f]+) cycles=([0-9a-f]+) observer_writes=0',trace)
    if (len(sends)!=1 or sends[0][:2]!=('00212c9f','002c6750')
            or len(results)!=1 or results[0][0]!='043c'
            or len(waits)!=1 or waits[0][:3]!=('0010ade0','0047','043c')):
        raise ValueError('controller ROM reply/result/wait boundary differs')
    if not int(sends[0][2],16)<int(results[0][1],16)<int(waits[0][3],16):
        raise ValueError('controller response ordering differs')
    return {'rom_reply':'0x043c','destination':0x12,'wait_selector':'0x47',
            'success_or_failure_semantics_proved':False}


def check_mode_chain(trace):
    check_controller(trace)
    patterns=(
        r'nsk3_mode_decision: status=09ec flag=00000000 owner=0010a7d8 mode=01 cycles=([0-9a-f]+) observer_writes=0',
        r'nsk3_07d6_construct: caller=0020e397 argument=00000000 cycles=([0-9a-f]+) observer_writes=0',
        r'nsk3_07d6_post: target=11 pointer=00101760 header=07d601fb caller=00208e3f cycles=([0-9a-f]+) observer_writes=0',
        r'nsk3_mode_construct: caller=002541a7 cycles=([0-9a-f]+) observer_writes=0',
        r'nsk3_mode_post: target=0b pointer=00102eb4 header=03e90100 caller=00287d77 cycles=([0-9a-f]+) observer_writes=0',
        r'nsk3_radio_argument2: object=00102eb4 header=03e90100 cycles=([0-9a-f]+) observer_writes=0',
        r'nsk3_043c_send: caller=00212c9f pointer=002c6750 cycles=([0-9a-f]+) observer_writes=0')
    matches=[re.findall(pattern,trace) for pattern in patterns]
    if any(len(row)!=1 for row in matches):raise ValueError('qualified mode-chain event missing or repeated')
    cycles=[int(row[0],16) for row in matches]
    if any(a>=b for a,b in zip(cycles,cycles[1:])):raise ValueError('mode-chain order differs')
    return {'request':'0x09ec','owner_mode':1,'queued_status':'0x07d6',
            'radio_command':'0x03e9','reply':'0x043c','mode_semantics_proved':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path)
    parser.add_argument('run',type=Path)
    parser.add_argument('--delivery',action='store_true',help='require same-object forward, dequeue and consumer observations')
    parser.add_argument('--cache',action='store_true',help='require exact body cache and state3 continuation')
    parser.add_argument('--controller',action='store_true',help='require ROM reply, actual result and controller wait')
    parser.add_argument('--mode-chain',action='store_true',help='require measured09ec-to043c MCU sequence')
    args=parser.parse_args()
    try:
        _,image=inspect(args.source,'boot4')
        for address,encoded in FRAGMENTS.items():
            expected=bytes.fromhex(encoded)
            if image[address-0x200000:address-0x200000+len(expected)]!=expected:
                raise ValueError('own type8b route differs')
        trace=(args.run/'error.log').read_text()
        summary=parse(trace)
        if args.delivery:summary['delivery']=check_delivery(trace)
        if args.cache:summary['cache']=check_cache(trace)
        if args.controller:summary['controller']=check_controller(trace)
        if args.mode_chain:summary['mode_chain']=check_mode_chain(trace)
        if (args.run/'nvram/nsk3r4t/flash').read_bytes()!=image:
            raise ValueError('own executable flash changed')
        print(summary)
    except (ValueError,OSError) as error:
        print(f'NSK-3 packet census failed: {error}',file=sys.stderr)
        return 1
    print('Bounded packet census qualified; task12 continuation and radio remain open')
    return 0


if __name__=='__main__':raise SystemExit(main())
