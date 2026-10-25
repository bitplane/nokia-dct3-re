"""Own reset-bank cold Toolkit tone publication, not an audio acceptance test."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_factory_reset_check import check as check_reset, EEPROM as RESET_EEPROM
from tools.nsm1_native_state_check import check_storage
from tools.nsm1_toolkit_check import (check_configuration, check_tone_native, PROFILE,
                                    PROMPT, INTERACTIVE_FRAMES, MENU_FRAMES, TONE_FRAME)
from tools.nsk3_pin_check import check_trace as check_pin
from tools.sim_toolkit_interactive_check import check_interactive
from tools.sim_toolkit_display_check import check_observer_console
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.nsm1_toolkit_tone_boundary_check import contract

COLD_EEPROM='e316e0bcef54ffc57df56d8bea63668c81aca19ca022218f1b94c087de7a0111'
FIELDS=((9,0x20c),(0x25,0xbb9),(0x20,0xe10),(0x24,0x7fff),(0x2e,0),
        (0x25,0xbb8),(0x2e,0),(9,0x20d),(0x25,0xbb9),(0x20,0x1b58),(0x2e,0),
        (0x25,0xbb8),(0x2e,0),(9,0x20f),(0x25,1),(0x20,0),(0x24,0),(0x2e,0))


def check_serial(text):
    if (re.findall(r'nsm1_factory_wave_summary: early=(\d+) late=(\d+) control=([0-9a-f]{4}) extension=([0-9a-f]{4}) observer_writes=0',text)
            !=[('17','6','c8cc','0000')] or text.count('nsm1_factory_wave_summary:')!=1):
        raise ValueError('unclocked serial-data census differs')
    rows=re.findall(r'nsm1_factory_wave_data: phase=(early|late) address=([0-9a-f]{4}) value=([0-9a-f]{4}) mask=([0-9a-f]{4}) pc=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0',text)
    early=[r for r in rows if r[0]=='early']
    late=[r for r in rows if r[0]=='late']
    if (len(early)!=16 or len(late)!=6 or text.count('nsm1_factory_wave_data:')!=22
            or [r[1:5] for r in early[:4]]!=[
                ('0022','c008','ffff','0e22'),('0022','c0c8','ffff','0e24'),
                ('0021','0aaa','ffff','0e31'),('0021','ffd5','ffff','358b')]
            or any(not 0<=float(r[5])<5 for r in early)):
        raise ValueError('serial data lacks boot positive controls')
    if ([r[1:5] for r in late]!=[r for _ in range(3) for r in (
            ('0022','c00c','ffff','a4a1'),('0022','c0cc','ffff','a4a3'))]
            or any(not 86<float(r[5])<86.5 for r in late)):
        raise ValueError('unclocked reset/release or scoped sample absence differs')


def check_publication(text):
    summary=re.findall(r'nsm1_tone_boundary_summary: start=85 end=91.5 mcu_early=(\d+) mcu=(\d+) dsp=(\d+) io=(\d+) compiler_early=(\d+) compiler=(\d+) observer_writes=0',text)
    if summary != [('70528','18','10','2854','20','18')] or text.count('nsm1_tone_boundary_summary:')!=1:
        raise ValueError('reset-bank publication census differs')
    positives=re.findall(r'nsm1_tone_boundary_early: address=([0-9a-f]{8}) value=([0-9a-f]{8}) mask=([0-9a-f]{8}) t=([0-9.]+) observer_writes=0',text)
    if (len(positives)!=8 or text.count('nsm1_tone_boundary_early:')!=8
            or any(not 0<=float(t)<5 or not int(mask,16)
                   or not 0x10000<=(int(address,16)&~0x8f000)<=0x10fff
                   for address,_,mask,t in positives)):
        raise ValueError('shared-memory early positive controls differ')
    constructor=re.findall(r'nsm1_factory_tone_constructor: mode=0 tone=a caller=2965a1 t=([0-9.]+) observer_writes=0',text)
    admission=re.findall(r'nsm1_factory_tone_admission: descriptor=002db038 enabled=01 setting=04 task=06 t=([0-9.]+) observer_writes=0',text)
    if (len(constructor)!=1 or len(admission)!=1
            or text.count('nsm1_factory_tone_constructor:')!=1 or text.count('nsm1_factory_tone_admission:')!=1
            or not 86.05<float(constructor[0])<float(admission[0])<86.1):
        raise ValueError('actual Toolkit tone construction/admission differs')
    compiler=re.findall(r'nsm1_tone_boundary_compiler: field=([0-9a-f]{8}) value=([0-9a-f]{8}) commit=([0-9a-f]{8}) caller=([0-9a-f]{8}) t=([0-9.]+) observer_writes=0',text)
    late=[r for r in compiler if 85<=float(r[4])<91.5]
    early=[r for r in compiler if 0<=float(r[4])<5]
    if (len(compiler)!=26 or text.count('nsm1_tone_boundary_compiler:')!=26 or len(late)!=18
            or [int(r[0],16) for r in early]!=[8,9,0x28,0x2d,0x2e,8,9,0x25]
            or [(int(r[0],16),int(r[1],16)) for r in late]!=list(FIELDS)
            or any(int(c,16)!=(field==0x2e) or caller!=('002a2c51' if field==0x2e else '002a2c29')
                   for (field,_),(_,_,c,caller,_) in zip(FIELDS,late))):
        raise ValueError('compiler sequence differs')
    if not float(late[2][4])<float(constructor[0])<float(late[9][4])<float(late[15][4])<86.5:
        raise ValueError('softkey click/Toolkit tone/stop ordering differs')
    mcu=re.findall(r'nsm1_tone_boundary_mcu: address=([0-9a-f]{8}) canonical=([0-9a-f]{8}) value=([0-9a-f]{8}) mask=([0-9a-f]{8}) pc=([0-9a-f]{8}) lr=([0-9a-f]{8}) t=([0-9.]+) observer_writes=0',text)
    if len(mcu)!=18 or text.count('nsm1_tone_boundary_mcu:')!=18:
        raise ValueError('MCU publication count differs')
    for (field,value),comp,row in zip(FIELDS,late,mcu):
        address,shift={9:(0x100a8,16),0x25:(0x100ac,16),0x20:(0x100ac,0),0x24:(0x100b4,0),0x2e:(0x100e0,16)}[field]
        published=(0x9200|(value&0xff)) if field==9 else 1 if field==0x2e else value
        if (row[:6]!=(f'{address:08x}',f'{address:08x}',f'{published<<shift:08x}',f'{0xffff<<shift:08x}',
                      '002a4d3a' if field==0x2e else '002a4d2c','002a4a7f')
                or not float(comp[4])<float(row[6])<float(comp[4])+.001):
            raise ValueError('compiler/shared-memory projection differs')
    dsp=re.findall(r'nsm1_tone_boundary_dsp: address=([0-9a-f]{4}) value=([0-9a-f]{4}) mask=([0-9a-f]{4}) pc=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0',text)
    if ([r[:4] for r in dsp]!=[r for _ in range(5) for r in (('0866','0002','ffff','3775'),('0870','0000','ffff','37a1'))]
            or text.count('nsm1_tone_boundary_dsp:')!=10):
        raise ValueError('native DSP acknowledgement sequence differs')
    commits=[float(row[6]) for (field,_),row in zip(FIELDS,mcu) if field==0x2e]
    if any(not commit<float(dsp[2*i][4])<=float(dsp[2*i+1][4])<commit+.001 for i,commit in enumerate(commits)):
        raise ValueError('DSP acknowledgement preceded publication')
    io=re.findall(r'nsm1_tone_boundary_io: address=([0-9a-f]{4}) value=([0-9a-f]{4}) mask=([0-9a-f]{4}) pc=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0',text)
    expected=Counter({('0021','0e82'):1408,('0021','0682'):1408,('0000','009d'):19,
                      ('002c','0009'):5,('002c','0018'):3,('002d','0626'):3,
                      ('002c','0008'):3,('002d','020c'):2,('002d','020d'):2,('002d','020f'):1})
    if (Counter((a,v) for a,v,_,_,_ in io)!=expected or text.count('nsm1_tone_boundary_io:')!=2854
            or any(m!='ffff' or not 85<=float(t)<91.5 for _,_,m,_,t in io)):
        raise ValueError('bounded native I/O census differs')


def prepare(seed,reset,run):
    check_reset(seed,reset)
    files=check_storage(reset,eeprom_sha256=RESET_EEPROM)
    if run.exists():
        raise ValueError('fresh cold replay output required')
    for name in ('nvram','cfg'):
        shutil.copytree(reset/name,run/name)
    for name in ('eeprom','sim_card'):
        (run/f'initial_{name}.bin').write_bytes(files[name])
    (run/'cold_input.json').write_text(json.dumps(dict(source=str(reset.resolve()),source_sha256={
        name:hashlib.sha256(data).hexdigest() for name,data in files.items()},
        copied_directories=['nvram','cfg'],ram_or_save_state_copied=False),indent=2)+'\n')
    tree=ET.parse(run/'cfg/nsm1r4t.cfg')
    inputs=tree.find('./system/input')
    if inputs.find("port[@tag=':SATCFG']") is not None:
        raise ValueError('factory-reset CFG already selects a proactive card')
    ET.SubElement(inputs,'port',tag=':SATCFG',type='CONFIG',mask='15',defvalue='0',value='11')
    tree.write(run/'cfg/nsm1r4t.cfg',encoding='utf-8',xml_declaration=True)


def check_common(seed,reset,run,*,machine='nsm1r4t',active_replay=False):
    check_reset(seed,reset)
    before=check_storage(reset,eeprom_sha256=RESET_EEPROM)
    after=check_storage(run,eeprom_sha256=COLD_EEPROM,machine=machine)
    contract(after['flash'])
    if before['flash']!=after['flash'] or before['sim_card']!=after['sim_card']:
        raise ValueError('cold replay altered card or executable')
    for name in ('eeprom','sim_card'):
        if (run/f'initial_{name}.bin').read_bytes()!=before[name]:
            raise ValueError('cold replay seed differs')
    metadata=dict(source=str(reset.resolve()),source_sha256={
        name:hashlib.sha256(data).hexdigest() for name,data in before.items()},
        copied_directories=['nvram','cfg'],ram_or_save_state_copied=False)
    if json.loads((run/'cold_input.json').read_text())!=metadata:
        raise ValueError('reset-bank cold provenance differs')
    check_configuration(reset,run,tone='hold',machine=machine)
    if json.loads((run/'exit_status.json').read_text())!={'returncode':0}:
        raise ValueError('cold replay did not complete')
    check_observer_console((run/'console.log').read_text())
    text=(run/'error.log').read_text()
    if active_replay:
        if machine!='nsm1pcm':
            raise ValueError('active tone replay requires the declared lab machine')
        from tools.nsm1_tone_replay_check import canonicalize
        text,_=canonicalize(text)
    if any(marker in text for marker in ('nsm1_security_contact:','nsm1_security_submit:',
                                         'nsm1_security_gate:','nsm1_security_accepted:')):
        raise ValueError('obsolete phone-lock input/transaction in unlocked replay')
    check_pin(text,True,prefix='nsm1',tx_limit=4096)
    check_interactive(text,prefix='nsm1',terminal_profile=PROFILE,
                      reset_markers=('nsm1_reset_request:','nsm1_restart_request:'),menu_item=1,tone='hold')
    if re.findall(r'nsm1_toolkit_menu: action=(\w+) down=([01])',text)!=[
            (a,d) for a in ('main','up1','open','select') for d in ('1','0')] or text.count('nsm1_toolkit_menu:')!=8:
        raise ValueError('own physical installed-menu route differs')
    check_tone_native(text)
    check_publication(text)
    frames={'before':PROMPT,'after':INTERACTIVE_FRAMES['inkey'],
            **INTERACTIVE_FRAMES,**{'menu_'+k:v for k,v in MENU_FRAMES.items()},
            'tone_before':MENU_FRAMES['open'],'tone_active':TONE_FRAME,'tone_after':TONE_FRAME}
    for name,digest in frames.items():
        frame_check(run/f'snap/nsm1_toolkit_{name}.png',digest)
    return text


def check(seed,reset,run):
    text=check_common(seed,reset,run)
    check_serial(text)


def execute(binary,seed,reset,run):
    prepare(seed,reset,run)
    script=Path(__file__).with_name('nsm1_factory_tone.lua').resolve()
    command=[str(binary),'nsm1r4t','-rompath',str(Path(__file__).resolve().parents[1]/'roms'),
             '-noreadconfig','-video','none','-sound','none','-nothrottle','-joystickprovider','none',
             '-midiprovider','none','-debug','-debugger','none','-log','-verbose',
             '-seconds_to_run','92','-autoboot_delay','0','-autoboot_script',str(script)]
    (run/'invocation.json').write_text(json.dumps(dict(command=command,
        mame_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
        script_sha256=hashlib.sha256(script.read_bytes()).hexdigest()),indent=2)+'\n')
    with (run/'console.log').open('w') as output:
        result=subprocess.run(command,cwd=run,stdout=output,stderr=subprocess.STDOUT,timeout=180)
    (run/'exit_status.json').write_text(json.dumps(dict(returncode=result.returncode))+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('seed','reset','run'):
        parser.add_argument(name,type=Path)
    parser.add_argument('--mame',type=Path)
    args=parser.parse_args()
    try:
        seed,reset,run=(getattr(args,name).resolve() for name in ('seed','reset','run'))
        if args.mame:
            execute(args.mame.resolve(),seed,reset,run)
        check(seed,reset,run)
    except (OSError,ValueError,KeyError,subprocess.TimeoutExpired) as exc:
        parser.exit(1,f'NSM-1 reset-bank tone: FAIL: {exc}\n')
    print('NSM-1 reset-bank cold Toolkit tone publication PASS; audio unproved')


if __name__=='__main__':
    main()
