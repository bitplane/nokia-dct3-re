"""Own NSM-1 native restoration of an organically fetched pending card command."""
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
from tools.nsm1_toolkit_check import prepare,check_inputs,PROFILE,PROMPT,IDLE
from tools.nsm1_native_state_check import check_pin,check_phone,records
from tools.nse2_native_state_check import check_state
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_toolkit_replay_check import check_replay


def check_trace(text,before,after,reference,restored):
    result=check_state(text,before,after,reference,restored,prefix='nsm1',
        checkpoint='65.000000000',card_size=3570,menu_frame=IDLE,ram_limit=0x120000)
    # Only already-validated architectural records contain ILLEGAL as a name.
    protocol=re.sub(r'^(?:\[luaengine\] )?nsm1_state: phase=(?:saved|restored) t=[^\n]+\n?',
                    '',text,flags=re.M)
    check_pin(protocol,True,prefix='nsm1',tx_limit=4096)
    check_phone(protocol,True,())
    records(protocol,True)
    check_replay(protocol,prefix='nsm1',terminal_profile=PROFILE,
                 reset_markers=('nsm1_reset_request:','nsm1_restart_request:'))
    markers=('SIM status ins=20 sw=9000','nsm1_security_submit:',
             'nsm1_security_accepted:','nsm1_state: phase=saved',
             'nsm1_state_contact: phase=reference down=1')
    positions=[text.index(marker) for marker in markers]
    if positions != sorted(positions):
        raise ValueError('authentication/save/contact order differs')
    return result


def check(seed,run):
    files=check_inputs(seed,run)
    pixels=[]
    for phase in ('reference','restored'):
        with Image.open(run/f'snap/nsm1_state_{phase}.png') as source:
            if source.size != (84,48):
                raise ValueError('own LCD geometry differs')
            pixels.append(source.convert('RGB').tobytes())
    result=check_trace((run/'error.log').read_text(),files['sim_card'],
        (run/'nvram/nsm1r4t/sim_card').read_bytes(),*pixels)
    frame_check(run/'snap/nsm1_toolkit_state_prompt.png',PROMPT)
    saved=run/'sta/nsm1r4t/nsm1_native_ui.sta'
    if not saved.is_file() or not saved.stat().st_size:
        raise ValueError('nonempty saved-state artifact missing')
    with saved.open('rb') as stream:
        result['state_sha256']=hashlib.file_digest(stream,'sha256').hexdigest()
    return result


def execute(binary,seed,run):
    prepare(seed,run)
    script=Path(__file__).with_name('nsm1_toolkit_state.lua').resolve()
    command=[str(binary),'nsm1r4t','-rompath',str(Path(__file__).resolve().parents[1]/'roms'),
        '-noreadconfig','-video','none','-sound','none','-nothrottle',
        '-joystickprovider','none','-midiprovider','none','-debug','-debugger','none',
        '-log','-verbose','-seconds_to_run','74','-autoboot_delay','0','-autoboot_script',str(script)]
    (run/'invocation.json').write_text(json.dumps(dict(command=command,
        mame_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
        script_sha256=hashlib.sha256(script.read_bytes()).hexdigest()),indent=2)+'\n')
    with (run/'console.log').open('w') as output:
        process=subprocess.run(command,cwd=run,stdout=output,stderr=subprocess.STDOUT,timeout=180)
    (run/'exit_status.json').write_text(json.dumps(dict(returncode=process.returncode))+'\n')


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
        parser.exit(1,f'NSM-1 pending Toolkit restoration FAIL: {error}\n')
    print(f"NSM-1 pending Toolkit restoration PASS: two physical completions, {result['io_replay_events']} exact DSP I/O writes; native RF/speech unproved")


if __name__=='__main__':
    main()
