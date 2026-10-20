"""Qualify the known native NSK-3 packet-supervision failure, not stable boot."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk3_static_check import inspect as inspect_source

FRAGMENTS={
    0x2ab4fc:'b5f04d582700706fe08e2001f7f7fa9fe08b8868300180681c20f7fffe72',
    0x2ab64a:'88682800d100e759201c4903f7bafcbc806fe7e3',
    0x2ab660:'0010c5a4000009cd',
    0x2a2a74:'466978094201d0012468e00024002000',
}


def check_static(image):
    for address,encoded in FRAGMENTS.items():
        expected=bytes.fromhex(encoded)
        if image[address-0x200000:address-0x200000+len(expected)]!=expected:
            raise ValueError(f'own supervision contract differs at {address:08x}')


def check_failure(trace):
    windows=re.findall(r'nsk3_liveness_window: counter=([0-9a-f]+) pointer=([0-9a-f]+) cycles=([0-9a-f]+) observer_writes=0',trace)
    if [(count,pointer) for count,pointer,_ in windows]!=[('0001','0010c5a4'),('0001','0010c5a4'),('0000','0010c5a4')]:
        raise ValueError('known packet-window failure changed')
    cycles=[int(row[2],16) for row in windows]
    if any(abs(value/13_000_000-31*(i+1))>.05 for i,value in enumerate(cycles)):
        raise ValueError('configured supervision timing differs')
    policy=re.findall(r'nsk3_liveness_policy: argument=([0-9a-f]+) caller=([0-9a-f]+) cycles=([0-9a-f]+) observer_writes=0',trace)
    if len(policy)!=1 or policy[0][:2]!=('00000001','002ab50d') or int(policy[0][2],16)<=cycles[-1]:
        raise ValueError('own empty-window policy route differs')
    resets=re.findall(r'nsk3_reset_request: command=([0-9a-f]+) pc=([0-9a-f]+) t=([0-9.]+) reason=([0-9a-f]+)',trace)
    if len(resets)!=1 or resets[0][:2]!=('05','002ac696') or resets[0][3]!='68' or not 93<float(resets[0][2])<95:
        raise ValueError('known reason68 restart differs')
    calls=re.findall(r'nsk3_restart_entry: reason=([0-9a-f]+) caller=([0-9a-f]+)',trace)
    if calls!=[('00000068','0023d83b')]:raise ValueError('restart owner differs')
    if re.search(r'illegal|unsupported|unhandled|fatal|\[LUA ERROR\]',trace,re.I):
        raise ValueError('execution/observer error is not the qualified failure')
    return {'stable_boot':False,'failure':'native packet supervision',
            'packet_windows':[1,1,0],'reset_reason':'0x68','reset_seconds':float(resets[0][2])}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path)
    parser.add_argument('run',type=Path)
    args=parser.parse_args()
    try:
        _,image=inspect_source(args.source,'boot4')
        check_static(image)
        if (args.run/'nvram/nsk3r4t/flash').read_bytes()!=image:
            raise ValueError('own executable flash changed')
        print(check_failure((args.run/'error.log').read_text()))
    except (ValueError,OSError) as error:
        print(f'NSK-3 supervision diagnostic failed: {error}',file=sys.stderr)
        return 1
    print('Expected native failure reproduced; this is NOT long-run stability acceptance')
    return 0


if __name__=='__main__':raise SystemExit(main())
