"""Bounded own NSK-3 packet activity and type8b routing, not radio acceptance."""
import argparse
import hashlib
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


def check_completion_wait(trace):
    waits=re.findall(r'nsk3_completion_wait: cycles=([0-9a-f]+) observer_writes=0',trace)
    samples=re.findall(r'nsk3_boot_sample: t=([0-9.]+)',trace)
    if (len(waits)!=1 or not samples or max(map(float,samples))<45
            or 'nsk3_completion_receive:' in trace or 'nsk3_completion_construct:' in trace
            or 'nsk3_completion_post:' in trace):
        raise ValueError('bounded empty completion-server observation differs')
    return {'observed_seconds':max(map(float,samples)),'receive_entries':1,'receive_returns':0,
            'all_possible_ingress_closed':False}


def check_native_control(trace,expected_words):
    writes=re.findall(r'nsk3_native_control_write: address=([0-9a-f]+) value=([0-9a-f]+) pc=([0-9a-f]+) t=([0-9.]+) observer_writes=0',trace)
    samples=re.findall(r'nsk3_native_control_sample: t=([0-9.]+) pending=([0-9a-f]+) completed=([0-9a-f]+) control06e3=([0-9a-f]+) state1949=([0-9a-f]+) writes_pending=(\d+) writes_completed=(\d+) cap=64 observer_writes=0',trace)
    words=re.findall(r'nsk3_native_live_words: address=([0-9a-f]+) words=([0-9a-f]+) observer_writes=0',trace)
    if (len(writes)!=trace.count('nsk3_native_control_write:')
            or len(samples)!=trace.count('nsk3_native_control_sample:')
            or len(words)!=trace.count('nsk3_native_live_words:')
            or len(samples) not in (4,6) or len(words)!=4 or dict(words)!=expected_words):
        raise ValueError('native control/program observation incomplete or changed')
    pending=[int(value,16) for address,value,_,_ in writes if address=='06d0']
    completed=[int(value,16) for address,value,_,_ in writes if address=='06d1']
    if pending!=[0,1,2,8,3] or not completed or completed[-1]!=0x10e:
        raise ValueError('native request/completion lifecycle differs')
    if any(address not in ('06d0','06d1') for address,_,_,_ in writes):
        raise ValueError('unexpected control address')
    times=[float(row[3]) for row in writes]
    if times!=sorted(times):raise ValueError('native write ordering differs')
    for row,want in zip(samples,(5,20,40,45,65,90)):
        time,p,c,gate,state,np,nc=row
        if (abs(float(time)-want)>0.05 or (p,c,gate,state)!=('0003','010e','0000','0000')
                or int(np)!=len(pending) or int(nc)!=len(completed)
                or max(int(np),int(nc))>=64):
            raise ValueError('native control samples/caps differ')
    return {'request_indexes':pending[1:],'completed_word':'0x010e',
            'live_regions_match_declared_rom':True,'fitted_mask_or_rf_contract_proved':False}


SCAN_FRAGMENTS={0x2420:'f0730a71',0x0a71:'76f812511a80e800',
                0x0ae6:'6ff8219b0d2077151266fa4f0b08771412576bf8134400016ff8198d0d20fa4f',
                0x0b08:'77161262e80150866b8c0001f8460cb84e8661850001f8200ae410f8'}


def check_native_scan(trace,program):
    if len(program)!=131072:raise ValueError('live program capture extent differs')
    for address,raw in SCAN_FRAGMENTS.items():
        expected=bytes.fromhex(raw)
        if program[address*2:address*2+len(expected)]!=expected:
            raise ValueError('live uploaded scan gateway/qualification code differs')
    rows=re.findall(r'nsk3_native_scan_sample: t=([0-9.]+) counter=([0-9a-f]{8}) qualified=([0-9a-f]+) flags=([0-9a-f]+) producer=([0-9a-f]+) consumer=([0-9a-f]+) threshold=([0-9a-f]+) observer_writes=0',trace)
    if len(rows)!=6 or len(rows)!=trace.count('nsk3_native_scan_sample:'):
        raise ValueError('native scan observation incomplete')
    late=[]
    for row,want in zip(rows,(5,20,40,45,65,90)):
        time,counter,qualified,flags,producer,consumer,threshold=row
        if abs(float(time)-want)>0.05 or (qualified,flags,threshold)!=('0000','0000','0574'):
            raise ValueError('native scan qualification/timing differs')
        if want>=40:
            value=int(counter,16)
            if not value&0x80000000 or producer!=consumer:
                raise ValueError('late scan counter/queue boundary differs')
            late.append(value)
    if any(a>=b for a,b in zip(late,late[1:])):raise ValueError('scan counter is not advancing')
    return {'observed_through_seconds':90,'late_counter_steps':late[-1]-late[0],
            'qualifying_results':0,'sampled_queue_cursors_match':True,
            'rf_fidelity_or_timeout_cause_proved':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path)
    parser.add_argument('run',type=Path)
    parser.add_argument('--delivery',action='store_true',help='require same-object forward, dequeue and consumer observations')
    parser.add_argument('--cache',action='store_true',help='require exact body cache and state3 continuation')
    parser.add_argument('--controller',action='store_true',help='require ROM reply, actual result and controller wait')
    parser.add_argument('--mode-chain',action='store_true',help='require measured09ec-to043c MCU sequence')
    parser.add_argument('--completion-wait',action='store_true',help='qualify the bounded empty server receive')
    parser.add_argument('--native-control',type=Path,help='qualify control census against declared native program ROM')
    parser.add_argument('--native-scan',type=Path,help='qualify90s scan samples against captured live program')
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
        if args.completion_wait:summary['completion_wait']=check_completion_wait(trace)
        if args.native_control:
            program=args.native_control.read_bytes()
            if hashlib.sha1(program).hexdigest()!='a05a1e96a8c36ec5a47e1ea059d15afa54ca5739':
                raise ValueError('declared native compatibility program differs')
            expected={f'{address:04x}':program[address*2:address*2+16].hex()
                      for address in (0x3000,0x313f,0x47f6,0x4a49)}
            summary['native_control']=check_native_control(trace,expected)
        if args.native_scan:summary['native_scan']=check_native_scan(trace,args.native_scan.read_bytes())
        if (args.run/'nvram/nsk3r4t/flash').read_bytes()!=image:
            raise ValueError('own executable flash changed')
        print(summary)
    except (ValueError,OSError) as error:
        print(f'NSK-3 packet census failed: {error}',file=sys.stderr)
        return 1
    print('Bounded packet census qualified; task12 continuation and radio remain open')
    return 0


if __name__=='__main__':raise SystemExit(main())
