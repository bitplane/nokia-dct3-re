"""Bound the own NSM-1 native compatibility scan, not fitted RF semantics."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_pin_check import check_authenticated
from tools.nsm1_published_record_check import records
from tools.nsm1_v523_static_check import SHA1
from tools.nsm1_security_check import MENU
from tools.nsk1_phonebook_roundtrip import frame_check

PROGRAM_SHA256='b03957d9a60e9e3048826fa16fc407422b87407688cbc0cab75ee31a95f6a89e'


def samples(trace):
    pattern=(r'nsm1_native_radio_sample: t=([0-9.]+) pending=([0-9a-f]{4}) completed=([0-9a-f]{4}) '
             r'scan=([0-9a-f]{8}) qualified=([0-9a-f]{4}) flags=([0-9a-f]{4}) '
             r'producer=([0-9a-f]{4}) consumer=([0-9a-f]{4}) threshold=([0-9a-f]{4}) '
             r'packets=(\d+) queue_writes=(\d+)/(\d+) observer_writes=0')
    matches=re.findall(pattern,trace)
    if len(matches)!=7 or len(matches)!=trace.count('nsm1_native_radio_sample: '):raise ValueError('native sample census incomplete')
    rows=[(float(row[0]),*[int(v,16) for v in row[1:9]],*[int(v) for v in row[9:]]) for row in matches]
    for target,row in zip((5,20,40,45,65,90,100),rows):
        if not target<=row[0]<target+0.1:raise ValueError('native sample horizon differs')
        if row[1:3]!=(3,0x10e) or row[4:6]!=(0,0) or row[8]!=0x574:raise ValueError('sampled control/qualification fields differ')
    if any(row[3] for row in rows[:4]):raise ValueError('initial quiet scan window changed')
    for old,new in zip(rows[4:],rows[5:]):
        delta=(new[3]-old[3])&0xffffffff
        if delta<100 or new[10]-old[10]!=delta or new[11]-old[11]!=delta or new[6]!=new[7]:
            raise ValueError('late queue/counter activity differs')
    return rows


def packets(trace):
    pattern=r'nsm1_native_radio_packet: index=(\d+) type=([0-9a-f]{2}) pointer=([0-9a-f]+) bytes=([0-9a-f]+) t=([0-9.]+) observer_writes=0'
    rows=re.findall(pattern,trace)
    if len(rows)!=7 or len(rows)!=trace.count('nsm1_native_radio_packet: '):raise ValueError('packet census incomplete/capped')
    expected=(0x74,0x74,0x74,0x74,0x8a,0x87,0x8b)
    for index,(row,kind) in enumerate(zip(rows,expected),1):
        number,packet_type,pointer,raw,time=row;body=bytes.fromhex(raw)
        if len(body)<4 or int(number)!=index or int(packet_type,16)!=kind or body[3]!=kind or len(body)!=body[2]+4:
            raise ValueError('packet index/type/length differs')
    payload=bytes.fromhex(rows[-1][3])[4:]
    words=[int.from_bytes(payload[i:i+2],'big') for i in range(0,len(payload),2)]
    metrics={value-65536 if value&0x8000 else value for value in words[2::2]}
    if len(payload)!=162 or words[0]!=0x80 or len(words[1::2])!=40 or metrics!={-129,-130}:
        raise ValueError('own native measurement packet differs')
    if not 49<float(rows[-1][4])<50:raise ValueError('measurement publication window differs')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('run',type=Path)
    args=parser.parse_args()
    try:
        run=args.run;trace=(run/'error.log').read_text();samples(trace);packets(trace)
        records(trace,True);check_authenticated(trace)
        if 'nsm1_restart_request:' in trace or 'nsm1_reset_control:' in trace:raise ValueError('observed restart/reset invalidates bounded run')
        horizons=re.findall(r'nsm1_native_snapshot: .* t=([0-9.]+)',trace)
        if not horizons or float(horizons[-1])<105:raise ValueError('105-second horizon missing')
        program=(run/'nsm1_native_program_45s.bin').read_bytes()
        if len(program)!=131072 or hashlib.sha256(program).hexdigest()!=PROGRAM_SHA256:raise ValueError('own live program capture differs')
        for address,raw in ((0x2420,'f0730a8d'),(0x0b25,'6ff8219b0d20'),(0x330a,'74d60027')):
            expected=bytes.fromhex(raw)
            if program[address*2:address*2+len(expected)]!=expected:raise ValueError('live candidate anchor differs')
        storage=run/'nvram/nsm1r4t'
        if hashlib.sha1((storage/'flash').read_bytes()).hexdigest()!=SHA1:raise ValueError('own flash differs')
        if (run/'initial_sim_card.bin').read_bytes()!=(storage/'sim_card').read_bytes():raise ValueError('baseline card changed')
        frame_check(run/'snap/nsm1_pin_menu.png',MENU)
    except (OSError,ValueError) as error:
        print(f'NSM-1 native radio baseline failed: {error}',file=sys.stderr);return 1
    print('NSM-1 bounded105s native packet/queue baseline PASS; fitted_mask=unproved RF_semantics=unproved registration=0')
    return 0


if __name__=='__main__':raise SystemExit(main())
