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
from tools.nsm1_native_upload_check import check_upload

PROGRAM_SHA256='b03957d9a60e9e3048826fa16fc407422b87407688cbc0cab75ee31a95f6a89e'
RETAINED_PROGRAM_SHA256='1d3506034405fb546cb8b4c60a26f6196e2cd5c9400217fc856dd6ce3aabf107'


def program_profile(program,trace,retained_upload=False):
    expected=RETAINED_PROGRAM_SHA256 if retained_upload else PROGRAM_SHA256
    if len(program)!=131072 or hashlib.sha256(program).hexdigest()!=expected:
        raise ValueError('own live program capture differs for selected profile')
    if retained_upload:check_upload(trace)


def samples(trace,long_run=False):
    pattern=(r'nsm1_native_radio_sample: t=([0-9.]+) pending=([0-9a-f]{4}) completed=([0-9a-f]{4}) '
             r'scan=([0-9a-f]{8}) qualified=([0-9a-f]{4}) flags=([0-9a-f]{4}) '
             r'producer=([0-9a-f]{4}) consumer=([0-9a-f]{4}) threshold=([0-9a-f]{4}) '
             r'packets=(\d+) queue_writes=(\d+)/(\d+) observer_writes=0')
    matches=re.findall(pattern,trace)
    if len(matches)!=(10 if long_run else 7) or len(matches)!=trace.count('nsm1_native_radio_sample: '):raise ValueError('native sample census incomplete')
    rows=[(float(row[0]),*[int(v,16) for v in row[1:9]],*[int(v) for v in row[9:]]) for row in matches]
    targets=(5,20,40,45,65,90,100,120,140,160) if long_run else (5,20,40,45,65,90,100)
    for target,row in zip(targets,rows):
        if not target<=row[0]<target+0.1:raise ValueError('native sample horizon differs')
        if row[1:3]!=(3,0x10e) or row[4:6]!=(0,0) or row[8]!=0x574:raise ValueError('sampled control/qualification fields differ')
    if any(row[3] for row in rows[:4]):raise ValueError('initial quiet scan window changed')
    for old,new in zip(rows[4:7],rows[5:8] if long_run else rows[5:7]):
        delta=(new[3]-old[3])&0xffffffff
        if delta<100 or new[10]-old[10]!=delta or new[11]-old[11]!=delta or new[6]!=new[7]:
            raise ValueError('late queue/counter activity differs')
    if long_run:
        if rows[7][9]!=7 or any(row[9]!=9 for row in rows[8:]):
            raise ValueError('second-cycle packet timing differs')
        if any(row[3] for row in rows[8:]) or rows[8][10:]!=rows[9][10:]:
            raise ValueError('second-cycle quiet queue window differs')
    return rows


def packets(trace,long_run=False):
    pattern=r'nsm1_native_radio_packet: index=(\d+) type=([0-9a-f]{2}) pointer=([0-9a-f]+) bytes=([0-9a-f]+) t=([0-9.]+) observer_writes=0'
    rows=re.findall(pattern,trace)
    if len(rows)!=(10 if long_run else 7) or len(rows)!=trace.count('nsm1_native_radio_packet: '):raise ValueError('packet census incomplete/capped')
    expected=(0x74,0x74,0x74,0x74,0x8a,0x87,0x8b)
    if long_run:expected+=(0x8a,0x87,0x8b)
    for index,(row,kind) in enumerate(zip(rows,expected),1):
        number,packet_type,pointer,raw,time=row;body=bytes.fromhex(raw)
        if len(body)<4 or int(number)!=index or int(packet_type,16)!=kind or body[3]!=kind or len(body)!=body[2]+4:
            raise ValueError('packet index/type/length differs')
    payload=bytes.fromhex(rows[6][3])[4:]
    words=[int.from_bytes(payload[i:i+2],'big') for i in range(0,len(payload),2)]
    metrics={value-65536 if value&0x8000 else value for value in words[2::2]}
    if len(payload)!=162 or words[0]!=0x80 or len(words[1::2])!=40 or metrics!={-129,-130}:
        raise ValueError('own native measurement packet differs')
    if not 49<float(rows[6][4])<50:raise ValueError('measurement publication window differs')
    if long_run:
        if bytes.fromhex(rows[7][3])[4:]!=bytes.fromhex('0375') or bytes.fromhex(rows[8][3])[4:]!=b'\x00':
            raise ValueError('second native scan request/result differs')
        if not 122<float(rows[7][4])<float(rows[8][4])<124 or not 161<float(rows[9][4])<162:
            raise ValueError('second native scan publication window differs')
        if bytes.fromhex(rows[9][3])[4:]!=payload:
            raise ValueError('second scan measurement payload differs')


def metric_source(trace,program):
    fragments={0x0b25:'6ff8219b0d20',0x0b47:'77161262e8015086',
               0x4701:'f0742424',0x47e6:'7712125477131255308a218a20823083',0x330a:'74d60027'}
    if len(program)!=131072:raise ValueError('live measurement program extent differs')
    for address,raw in fragments.items():
        expected=bytes.fromhex(raw)
        if program[address*2:address*2+len(expected)]!=expected:raise ValueError(f'own measurement anchor differs at {address:04x}')
    rows=re.findall(r'nsm1_scan_metric: index=(\d+) pc=([0-9a-f]+) a=([0-9a-f]+) threshold=([0-9a-f]+) producer=([0-9a-f]+) consumer=([0-9a-f]+) t=([0-9.]+) observer_writes=0',trace)
    if len(rows)!=8 or len(rows)!=trace.count('nsm1_scan_metric: '):raise ValueError('measurement census missing/malformed')
    previous=None
    for index,(number,pc,acc,threshold,producer,consumer,time) in enumerate(rows,1):
        pointer=int(producer,16)
        if int(number)!=index or int(pc,16)!=0x0b26 or int(acc,16)!=0 or int(threshold,16)!=0x574:
            raise ValueError('own measurement return/comparison differs')
        if producer!=consumer or not 0x1a00<=pointer<0x1a48 or not 50<float(time)<51:
            raise ValueError('measurement queue/window differs')
        if previous is not None and (pointer-previous)%0x48!=4:raise ValueError('measurement queue did not advance by four words')
        previous=pointer
    source=re.findall(r'nsm1_scan_rf_input: index=(\d+) value=([0-9a-f]{4}) pc=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0',trace)
    if len(source)!=8 or len(source)!=trace.count('nsm1_scan_rf_input: '):raise ValueError('RF input census incomplete')
    previous=50
    for index,(number,value,pc,time) in enumerate(source):
        timestamp=float(time)
        if int(number)!=index+1 or int(value,16)!=0 or int(pc,16)!=(0x330c,0x3312,0x3318,0x331e)[index%4]:
            raise ValueError('RF input value/callsite differs')
        if not previous<timestamp<51:raise ValueError('RF observation ordering differs')
        previous=timestamp
    queue=re.findall(r'nsm1_scan_rf_queue: start=1a00 words=72 data=([0-9a-f]+) observer_writes=0',trace)
    if queue!=['0000'*72] or trace.count('nsm1_scan_rf_queue: ')!=1:raise ValueError('RF queue is not exactly72 zero words')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('run',type=Path)
    parser.add_argument('--metric-source',action='store_true')
    parser.add_argument('--long-run',action='store_true')
    parser.add_argument('--retained-upload',action='store_true',help='select the corrected upload-bank profile, not the historical capture')
    args=parser.parse_args()
    try:
        run=args.run;trace=(run/'error.log').read_text();samples(trace,args.long_run);packets(trace,args.long_run)
        records(trace,True);check_authenticated(trace)
        if any(label in trace for label in ('nsm1_restart_request:','nsm1_reset_control:','nsm1_native_long_restart:')):raise ValueError('observed restart/reset invalidates bounded run')
        horizons=re.findall(r'nsm1_native_snapshot: .* t=([0-9.]+)',trace)
        horizon=165 if args.long_run else 105
        if not horizons or float(horizons[-1])<horizon:raise ValueError(f'{horizon}-second horizon missing')
        program=(run/'nsm1_native_program_45s.bin').read_bytes()
        program_profile(program,trace,args.retained_upload)
        if args.metric_source:metric_source(trace,program)
        for address,raw in ((0x2420,'f0730a8d'),(0x0b25,'6ff8219b0d20'),(0x330a,'74d60027')):
            expected=bytes.fromhex(raw)
            if program[address*2:address*2+len(expected)]!=expected:raise ValueError('live candidate anchor differs')
        storage=run/'nvram/nsm1r4t'
        if hashlib.sha1((storage/'flash').read_bytes()).hexdigest()!=SHA1:raise ValueError('own flash differs')
        if (run/'initial_sim_card.bin').read_bytes()!=(storage/'sim_card').read_bytes():raise ValueError('baseline card changed')
        frame_check(run/'snap/nsm1_pin_menu.png',MENU)
    except (OSError,ValueError) as error:
        print(f'NSM-1 native radio baseline failed: {error}',file=sys.stderr);return 1
    print(f'NSM-1 bounded{horizon}s native packet/queue baseline PASS; fitted_mask=unproved RF_semantics=unproved registration=0')
    if args.metric_source:print('NSM-1 own live comparison and zero port27/72-word queue PASS; physical_sample_format=unproved')
    return 0


if __name__=='__main__':raise SystemExit(main())
