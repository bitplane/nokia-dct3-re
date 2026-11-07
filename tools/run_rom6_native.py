"""Fresh-process 8210 native-ROM6 UI/storage acceptance; no RF or speech claim."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
if __package__ in (None,''): sys.path.insert(0,str(ROOT))
from tools.noki8210_pmm_check import base_record_fixture
from tools.noki8210_phonebook_check import check as check_phonebook, check_frame as check_contact
from tools.rom6_native_check import MCU_SHA1, CALL_REGISTER_TEXT_SHA256, check_composition, check_trace, check_uploads, check_menu, check_state

PMM_SHA256='31f51bcd69e183f23c39136574bd6a44864eb2ba1939417b9d848a3e0639ec59'


def prepare(run,mcu,pmm,*,machine='nsm3r6'):
    if hashlib.sha1(mcu).hexdigest()!=MCU_SHA1 or hashlib.sha256(pmm).hexdigest()!=PMM_SHA256:
        raise ValueError('native fixture requires pinned own MCU/PMM inputs')
    storage=run/'nvram'/machine
    storage.mkdir(parents=True,exist_ok=False)
    (storage/'flash').write_bytes(mcu+base_record_fixture(pmm))
    # Share the acquired DSP pack without an invalid clone-of-clone or
    # copies in the permanent ROM collection. Parent MCU files stay there.
    roms=run/'roms'
    roms.mkdir()
    (roms/machine).symlink_to(ROOT/'roms/nsm3r6',target_is_directory=True)


def execute(run,phase,script,seconds,binary,*,persist=False,verbose=False,machine='nsm3r6'):
    directory=run/phase
    directory.mkdir()
    rompath=f'{run/"roms"};{ROOT/"roms"}'
    command=[str(binary),machine,'-rompath',rompath,
             '-nvram_directory',str(run/'nvram'),'-cfg_directory','cfg',
             '-state_directory',str(run/'sta'),'-snapshot_directory','snap',
             '-noreadconfig','-joystickprovider','none','-midiprovider','none',
             '-keyboardprovider','none','-mouseprovider','none','-lightgunprovider','none',
             '-video','none','-sound','none','-nothrottle','-debug','-debugger','none',
             '-log','-autoboot_delay','0','-autoboot_script',str(ROOT/'tools'/script),
             '-seconds_to_run',str(seconds)]
    if not persist: command.append('-nonvram_save')
    if verbose: command.append('-verbose')
    environment={key:value for key,value in os.environ.items() if not key.startswith('NOKIA_DCT3_')}
    with (directory/'console.log').open('w') as console:
        subprocess.run(command,cwd=directory,env=environment,stdout=console,
                       stderr=subprocess.STDOUT,check=True,timeout=600)
    text=(directory/'error.log').read_text(errors='replace')+'\n'+(directory/'console.log').read_text(errors='replace')
    check_trace(text,seconds-1)
    return directory,text


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scenario',choices=('all','menu','state','phonebook','radio','tuning','config','serial','receive','reset-policy'),default='all')
    parser.add_argument('--output-root',type=Path,default=ROOT/'tmp/native-rom6-acceptance')
    parser.add_argument('--mame',type=Path,default=ROOT/'mame/mame')
    args=parser.parse_args()
    try:
        from PIL import Image
        subprocess.run([sys.executable,str(ROOT/'tools/dsp_rom6_candidate_check.py'),
                        str(ROOT/'roms/research/nsm3-rom6'),'--prepare',str(ROOT/'roms/nsm3r6')],check=True)
        args.output_root.mkdir(parents=True,exist_ok=True)
        output=Path(tempfile.mkdtemp(prefix='acceptance-',dir=args.output_root.resolve()))
        print(f'Native ROM6 evidence: {output}',flush=True)
        configuration=subprocess.run([str(args.mame.resolve()),'nsm3r6','-noreadconfig','-listxml'],
                                     check=True,capture_output=True,text=True).stdout
        check_composition(configuration)
        (output/'composition.xml').write_text(configuration)
        mcu=(ROOT/'roms/noki8210/8210_5.31ppm_c.fls').read_bytes()
        pmm=(ROOT/'roms/noki8210/8210 virgin eeprom 003d0000.fls').read_bytes()
        for scenario in (('menu','state','phonebook') if args.scenario=='all' else (args.scenario,)):
            run=output/scenario
            prepare(run,mcu,pmm)
            script,seconds={'menu':('rom6_native_ui.lua',60),'radio':('rom6_native_radio.lua',60),
                            'reset-policy':('rom6_reset_policy_observe.lua',65),
                            'tuning':('rom6_native_tuning.lua',60),'state':('rom6_native_state.lua',30),
                            'config':('rom6_native_config.lua',60),
                            'serial':('rom6_native_config.lua',60),
                            'receive':('rom6_native_receive.lua',60),
                            'phonebook':('noki8210_phonebook_input.lua',32)}[scenario]
            phase,text=execute(run,'primary',script,seconds,args.mame.resolve(),
                               persist=scenario=='phonebook',verbose=scenario in ('phonebook','serial','receive'))
            check_uploads(mcu,(phase/'rom6_native_loader2.be16').read_bytes(),
                          (phase/'rom6_native_installed.be16').read_bytes())
            if scenario in ('menu','radio','tuning','config','serial','receive','reset-policy'):
                with Image.open(phase/'snap/rom6_native_menu.png') as frame: check_menu(frame)
                with Image.open(phase/'snap/rom6_native_late_menu.png') as frame: check_menu(frame,CALL_REGISTER_TEXT_SHA256)
                position=text.find('rom6_native_physical: key=Scroll Down t=54.500000')
                if position<0 or text.find('rom6_native_key: decoded=18 observer_writes=0',position)<0:
                    raise ValueError('sustained native run lacks late physical Down/decode')
                if scenario in ('radio','tuning','config','serial','receive'):
                    from tools.rom6_radio_census import census, check_output_overlay
                    with (phase/'rom6_radio_writes.tsv').open('rb') as stream:
                        report=census(text,stream)
                    report['own_upload_writer']=check_output_overlay(mcu,(phase/'rom6_radio_first_output.be16').read_bytes())
                    from tools.rom6_timed_output_check import check as check_timed_output
                    with (phase/'rom6_radio_writes.tsv').open('rb') as stream:
                        report['compare_boundary' if scenario in ('serial','receive') else 'compare_prefix']=check_timed_output(text,stream,full_boundary=scenario in ('serial','receive'))
                    if scenario in ('tuning','config','serial','receive'):
                        from tools.rom6_tuning_contract import check as check_tuning
                        with (phase/'rom6_radio_writes.tsv').open('rb') as stream:
                            report['tuning']=check_tuning(text,stream,(ROOT/'roms/nsm3r6/nsm3_rom6_program.be16').read_bytes())
                    if scenario in ('config','serial','receive'):
                        from tools.rom6_config_contract import check as check_config
                        with (phase/'rom6_radio_writes.tsv').open('rb') as stream:
                            report['configuration']=check_config(text,stream,
                                (phase/'rom6_config_uploaded_prefix.be16').read_bytes(),mcu,
                                (phase/'rom6_radio_first_output.be16').read_bytes(),
                                (ROOT/'roms/nsm3r6/nsm3_rom6_program.be16').read_bytes())
                    if scenario=='receive':
                        from tools.rom6_receive_contract import check as check_receive, check_programming, check_capture_programming
                        with (phase/'rom6_radio_writes.tsv').open('rb') as stream:
                            report['receive_prefix']=check_receive(text,stream,
                                (phase/'rom6_receive_first_program.be16').read_bytes(),mcu)
                        with (phase/'rom6_radio_writes.tsv').open('rb') as stream:
                            report['receive_programming']=check_programming(text,stream,
                                (phase/'rom6_receive_short_frame_program.be16').read_bytes(),mcu)
                        with (phase/'rom6_radio_writes.tsv').open('rb') as stream:
                            report['capture_programming']=check_capture_programming(text,stream,
                                (phase/'rom6_receive_first_program.be16').read_bytes(),mcu,
                                (ROOT/'roms/nsm3r6/nsm3_rom6_program.be16').read_bytes())
                    if scenario in ('serial','receive'):
                        from tools.rom6_hagar_contract import check as check_hagar
                        report['hagar_commands']=check_hagar(text)
                    (run/'radio-census.json').write_text(json.dumps(report,indent=2)+'\n')
            elif scenario=='state':
                with Image.open(phase/'snap/nsm3r6_state_reference.png') as reference, Image.open(phase/'snap/nsm3r6_state_restored.png') as restored:
                    check_state(text,reference,restored,phase/'nsm3r6_reference.io',phase/'nsm3r6_restored.io')
            else:
                card=run/'nvram/nsm3r6/sim_card'
                before=card.read_bytes()
                cold,cold_text=execute(run,'cold','noki8210_phonebook_read.lua',28,args.mame.resolve(),verbose=True)
                check_uploads(mcu,(cold/'rom6_native_loader2.be16').read_bytes(),(cold/'rom6_native_installed.be16').read_bytes())
                check_phonebook(text,cold_text,before)
                if before!=card.read_bytes(): raise ValueError('cold readback changed retained SIM bytes')
                with Image.open(cold/'snap/8210_phonebook_read_contact.png') as frame: check_contact(frame)
            (run/'acceptance.json').write_text(json.dumps(dict(machine='nsm3r6',scenario=scenario,
                passed=True,provisioning='acquired base record; later journal omitted',
                dsp_hle=False,sustained_ui=scenario in ('menu','radio','tuning','config','serial','receive'),dsp_clock_hz=45_500_000,
                partial_port36_handshake=True,
                native_registration=False,native_speech=False),indent=2)+'\n')
            print(f'Native ROM6 {scenario}: PASS',flush=True)
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        parser.exit(1,f'Native ROM6 acceptance FAIL: {error}\n')


if __name__=='__main__': main()
