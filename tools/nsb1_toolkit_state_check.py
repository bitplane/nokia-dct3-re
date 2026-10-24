"""Own clean-idle NSB-1 native restoration of a fetched pending SIM command."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from PIL import Image

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsb1_native_state_check import check_storage
from tools.nsb1_toolkit_check import check_inputs,TERMINAL_PROFILE,DISPLAY_FRAME,INTERACTIVE_FRAMES
from tools.nsb1_toolkit_prepare import prepare
from tools.nsb1_rom4_compare import run_mame,check_frame
from tools.nsb1_pin_roundtrip import check_contacts
from tools.nse2_native_state_check import check_state
from tools.sim_toolkit_replay_check import check_replay
from tools.sim_security_trace_check import statuses

IDLE=INTERACTIVE_FRAMES['interactive_final']


def check_trace(text,before,after,reference,restored):
    result=check_state(text,before,after,reference,restored,prefix='nsb1',
        checkpoint='65.000000000',card_size=3570,menu_frame=IDLE)
    if re.search(r'illegal instruction|unsupported|unimplemented|unhandled|fatal|LUA ERROR|Error in',text,re.I):
        raise ValueError('execution or observer failed')
    check_replay(text,prefix='nsb1',terminal_profile=TERMINAL_PROFILE,
                 reset_markers=('nsb1_reset_reason:',))
    prefix=text.split('nsb1_state: phase=saved',1)[0]
    if statuses(text,0x20) != [(0x9000,3,10)] or statuses(prefix,0x20) != [(0x9000,3,10)]:
        raise ValueError('PIN acceptance must precede checkpoint')
    check_contacts(prefix,'pin',5)
    check_contacts(prefix,'phone',6)
    return result


def check(seed,run):
    files=check_storage(seed)
    check_storage(run)
    text,card=check_inputs(seed,run,1,74)
    pixels=[]
    for phase in ('reference','restored'):
        with Image.open(run/f'snap/nsb1_state_{phase}.png') as image:
            if image.size != (84,48):
                raise ValueError('own LCD geometry differs')
            pixels.append(image.convert('RGB').tobytes())
    result=check_trace(text,files['sim_card'],card,*pixels)
    check_frame(run/'snap/nsb1_toolkit_state_prompt.png',DISPLAY_FRAME)
    saved=run/'sta/nsb1r4t/nsb1_native_ui.sta'
    if not saved.is_file() or not saved.stat().st_size:
        raise ValueError('nonempty saved artifact missing')
    with saved.open('rb') as stream:
        result['state_sha256']=hashlib.file_digest(stream,'sha256').hexdigest()
    return result


def execute(binary,seed,run):
    check_storage(seed)
    prepare(seed,run)
    script=Path(__file__).with_name('nsb1_toolkit_state.lua').resolve()
    (run/'state_invocation.json').write_text(json.dumps(dict(
        mame=str(binary),mame_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
        script=str(script),script_sha256=hashlib.sha256(script.read_bytes()).hexdigest(),
        seconds=74,profile=1),indent=2)+'\n')
    run_mame(binary,run/'inputs',run,script,74,True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('seed',type=Path)
    parser.add_argument('run',type=Path)
    parser.add_argument('--mame',type=Path)
    args=parser.parse_args()
    try:
        seed,run=args.seed.resolve(),args.run.resolve()
        if args.mame:
            execute(args.mame.resolve(),seed,run)
        result=check(seed,run)
    except (OSError,ValueError,KeyError,ET.ParseError,subprocess.TimeoutExpired) as error:
        parser.exit(1,f'NSB-1 pending Toolkit restoration FAIL: {error}\n')
    print(f"NSB-1 pending Toolkit restoration PASS: two physical completions, {result['io_replay_events']} exact DSP I/O writes; native RF/speech unproved")


if __name__=='__main__':
    main()
