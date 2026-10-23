"""Reproduce bounded native word-port static and scheduling evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.run_nse2_native import prepare_native
from tools.run_nse2_puk2_exhaustion import run_stage
from tools.c54x_rom4_port_census import classify_candidates
from tools.nse2_sampling_check import (inspect_word_configuration_contract,
                                      inspect_word_host_command,
                                      inspect_word_pending_frames,
                                      inspect_word_frame_source,
                                      inspect_word_worker_objects,
                                      inspect_word_service_observation)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory',type=Path)
    parser.add_argument('--mame',type=Path)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    run=args.run_directory.resolve()
    mame=(args.mame or root/'mame/mame').resolve()
    try:
        run.mkdir(parents=True,exist_ok=False)
        results={}
        for name,script in [('export','nse2_native_control_export.lua'),
                            ('service','nse2_word_service_observe.lua')]:
            target=run/name
            report=prepare_native(root,target,record_checksum=True,nominal_vbat=True,
                                  insert_sim=True,phone_retry_zero=True)
            command=[str(mame),'nse2r4v','-rompath',str(target/'roms'),
                     '-nvram_directory',str(target/'nvram'),'-cfg_directory',str(target/'cfg'),
                     '-video','none','-sound','none','-joystickprovider','none',
                     '-midiprovider','none','-nothrottle','-noreadconfig','-log','-verbose',
                     '-seconds_to_run','9','-autoboot_delay','0',
                     '-autoboot_script',str(root/'tools'/script),'-debug','-debugger','none']
            with mame.open('rb') as stream:
                report['mame_sha256']=hashlib.file_digest(stream,'sha256').hexdigest()
            report.update(command=command,read_only_observer=True)
            (target/'inputs.json').write_text(json.dumps(report,indent=2)+'\n')
            with (target/'console.log').open('w') as stream:
                run_stage(command,target,stream)
            console=(target/'console.log').read_text(errors='replace')
            if 'LUA ERROR' in console or 'Error in' in console:
                raise ValueError('word-contract observer failed')
            if name=='export':
                program=(target/'nse2_native_program.bin').read_bytes()
                data=(target/'nse2_native_data.bin').read_bytes()
                results['configuration']=inspect_word_configuration_contract(program)
                results['host_command']=inspect_word_host_command(program,data)
                results['pending_frames']=inspect_word_pending_frames(data)
                disassemblies=[path.read_text() for path in sorted(target.glob('*.dasm'))]
                sites=classify_candidates(program,disassemblies,{0x34,0x35,0x36})
                if len(sites)!=38 or any(site['classification']!='linear_io' for site in sites):
                    raise ValueError('word-port linear decode coverage differs')
                negative=[site for site in classify_candidates(program,disassemblies,{0x0a})
                          if site['address']=='71ef']
                if len(negative)!=1 or negative[0]['classification']!='operand_word':
                    raise ValueError('CALLD operand negative control differs')
                results['linear_decode_census']=dict(sites=sites,negative_control=negative[0],
                                                     runtime_reachability_proved=False)
            else:
                text=(target/'error.log').read_text(errors='replace')
                results['service']=inspect_word_service_observation(text)
                results['frame_source']=inspect_word_frame_source(program,data,text)
                results['worker_objects']=inspect_word_worker_objects(text)
        results['physical_completion_proved']=False
        results['long_run_stability_proved']=False
        (run/'acceptance.json').write_text(json.dumps(results,indent=2)+'\n')
        print('NSE-2 word contract PASS; physical completion and sustained operation unproved')
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        parser.exit(1,f'NSE-2 word contract FAIL: {error}\n')


if __name__=='__main__':
    main()
