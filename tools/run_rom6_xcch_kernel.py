"""Qualify native ROM6 control Viterbi/packing, stopping before hardware FIRE."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
if __package__ in (None,''): sys.path.insert(0,str(ROOT))
from tools.gsm_xcch_signal import check_vector,control_bits


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--entry-dir',type=Path,required=True,help='passive SCH capture containing resident program and DROM')
    parser.add_argument('--octets',default='49061b000100f11000014000000000000000002b2b2b2b',help='exactly23 radio octets, hex')
    parser.add_argument('--fire-transaction',action='store_true',help='verify request ports; halt before hardware status polling')
    parser.add_argument('--bursts',action='store_true',help='run native four-burst deinterleaving and bit decoding')
    args=parser.parse_args()
    if args.bursts and args.fire_transaction: parser.error('choose one isolated boundary per run')
    octets=bytes.fromhex(args.octets)
    control_bits(octets) # Reject malformed input before building.
    parent=ROOT/'tmp/rom6-receive-kernel';parent.mkdir(exist_ok=True)
    run=Path(tempfile.mkdtemp(prefix='xcch-',dir=parent))
    print(f'ROM6 isolated xCCH evidence: {run}',flush=True)
    for name in ('program','data'):
        snapshot=(args.entry_dir/f'rom6_sch_decode_{name}.le16').read_bytes()
        original=(ROOT/f'roms/nsm3r6/nsm3_rom6_{name}.be16').read_bytes()
        first=0x2000 if name=='program' else 0xb000
        if len(snapshot)!=131072 or any(snapshot[2*i:2*i+2]!=original[2*i:2*i+2][::-1] for i in range(first,65536)):
            raise ValueError('resident code or DROM differs from acquired ROM6')
        shutil.copyfile(args.entry_dir/f'rom6_sch_decode_{name}.le16',run/f'rom6_sch_decode_{name}.le16')
    exporter=run/'gsm_xcch_vector'
    subprocess.run(['g++','-std=c++17','-O2',str(ROOT/'tools/gsm_xcch_vector.cpp'),
                    str(ROOT/'driver/gsm_tch_f_l1.cpp'),'-o',str(exporter)],check=True)
    vector=json.loads(subprocess.run([str(exporter)],input=octets,capture_output=True,check=True).stdout)
    report=check_vector(vector)
    bits=list(control_bits(octets))
    # The native caller complements FIRE bits before handing224 bits to hardware.
    bits=bits[:184]+[bit^1 for bit in bits[184:224]]+bits[224:]
    words=[32 if bit=='0' else 0xffe0 for bit in vector['coded']]
    words.extend(sum(bit<<(index%16) for index,bit in enumerate(bits) if index//16==word) for word in range(15))
    (run/'xcch_vector.le16').write_bytes(b''.join(word.to_bytes(2,'little') for word in words))
    burst_words=[]
    for burst in vector['normal_bursts']:
        burst_words.extend(3 if bit=='0' else 12 for bit in burst[3:60]+burst[88:145])
    (run/'xcch_bursts.le16').write_bytes(b''.join(word.to_bytes(2,'little') for word in burst_words))
    (run/'source.json').write_text(json.dumps(vector,indent=2)+'\n')
    env=os.environ.copy();env['C54X_ROM6_RECEIVE_DIR']=str(run)
    if args.fire_transaction: env['C54X_ROM6_FIRE_TRANSACTION']='1'
    else: env.pop('C54X_ROM6_FIRE_TRANSACTION',None)
    if args.bursts: env['C54X_ROM6_XCCH_BURSTS']='1'
    else: env.pop('C54X_ROM6_XCCH_BURSTS',None)
    result=subprocess.run([sys.executable,str(ROOT/'tools/run_c54x_section.py'),'rom6_xcch',
        '--fixture-header',str(ROOT/'tests/firmware/rom6_xcch_kernel.h'),'--fixture-cases','1'],
        env=env,capture_output=True,text=True)
    (run/'kernel.log').write_text(result.stdout+result.stderr)
    report.update(native_decoding_qualified=result.returncode==0,hardware_fire_qualified=False,
        fire_request_qualified=args.fire_transaction and result.returncode==0,
        burst_deinterleaver_qualified=args.bursts and result.returncode==0,sample_frontend_qualified=False,
        physical_capture_qualified=False,entry_directory=str(args.entry_dir.resolve()),
        source_sha256=hashlib.sha256((run/'source.json').read_bytes()).hexdigest())
    (run/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    print(result.stdout+result.stderr,end='');result.check_returncode()


if __name__=='__main__':
    try: main()
    except (OSError,ValueError,subprocess.CalledProcessError) as error:
        raise SystemExit(f'ROM6 isolated xCCH fixture rejected: {error}')
