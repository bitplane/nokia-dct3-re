"""Own NSK-1 physical phone-code controls; not fitted-mask or radio proof."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk1_rom4_compare import prepare,summarize,INPUTS,SECURITY_PROMPT_RGB_SHA256
from tools.nsk1_record_exchange_check import check as check_records


PHONEBOOK_LIST_RGB_SHA256 = '3600e3d1cab564b185d03403552ea401e1e76edd07b110184716700828a5df2a'
CODE_ERROR_RGB_SHA256 = 'fd5b8fb1ab0c3397bf958d567a46aee84a72f75123802a00d5bc802a1814543e'
CONTACTS = ((2,2),(3,2),(4,2),(2,4),(3,4),(1,4),(1,4),(1,4),(1,4))


def check(trace, snapshots, initialized_retry, following_keys=()):
    from PIL import Image
    if '[LUA ERROR]' in trace:
        raise ValueError('security fixture Lua error')
    rows = re.findall(r'nsk1_code_contact: index=(\d+) column=(\d) mask=([0-9a-f]{2}) down=([01]) t=([0-9.]+) physical_only=1',trace)
    if len(rows) != 18 or len(rows) != trace.count('nsk1_code_contact:'):
        raise ValueError('missing or malformed physical security contacts')
    expected = []
    for index,(column,mask) in enumerate(CONTACTS,1):
        when = 11+index if index <= 6 else 21+(index-7)*2
        expected.extend((index,column,mask,down,when+delay) for down,delay in ((1,0),(0,0.2)))
    for row,wanted in zip(rows,expected):
        actual = (int(row[0]),int(row[1]),int(row[2],16),int(row[3]))
        if actual != wanted[:4] or not wanted[4] <= float(row[4]) < wanted[4]+0.1:
            raise ValueError('physical security contact order/time differs')
    keys = re.findall(r'nsk1_code_key: value=([0-9a-f]{2}) t=[0-9.]+ observer_writes=0',trace)
    if len(keys) != trace.count('nsk1_code_key:') or [int(key,16) for key in keys] != [1,2,3,4,5,0x19,0x19,0x19,0x19]+list(following_keys):
        raise ValueError('physical security/menu key decoding differs')
    gates = re.findall(r'nsk1_code_gate: match=([0-9a-f]{8}) retry=([0-9a-f]{2}) cycles=[0-9a-f]{16} observer_writes=0',trace)
    if (not gates or len(gates) != trace.count('nsk1_code_gate:')
            or gates[0] != ('00000001','00' if initialized_retry else 'ff')
            or initialized_retry and len(gates) != 1
            or not initialized_retry and any(retry != 'ff' for _,retry in gates)):
        raise ValueError('own matching-code/retry gate differs')
    states = re.findall(r'nsk1_code_state: checklist=([0-9a-f]{2}) root=([0-9a-f]{2}) retry=([0-9a-f]{2}) t=([0-9.]+) observer_writes=0',trace)
    if (len(states) != 7 or len(states) != trace.count('nsk1_code_state:')
            or any(bits != '0f' or retry != ('00' if initialized_retry else 'ff')
                   or not when <= float(time) < when+0.1
                   for (bits,_,retry,time),when in zip(states,(18,20,22,24,26,28,30)))):
        raise ValueError('security readiness/retry snapshots differ')
    expected_hash = PHONEBOOK_LIST_RGB_SHA256 if initialized_retry else SECURITY_PROMPT_RGB_SHA256
    for seconds in (26,28,30):
        frame_hash = CODE_ERROR_RGB_SHA256 if not initialized_retry and seconds == 26 else expected_hash
        with Image.open(snapshots/f'nsk1_code_{seconds}s.png') as source:
            frame = source.convert('RGB')
            if frame.size != (84,48) or hashlib.sha256(frame.tobytes()).hexdigest() != frame_hash:
                raise ValueError('security control or phonebook-list pixels differ')
    return {'code_matches':True,'retry':0 if initialized_retry else 255,
            'phonebook_list_accepted':initialized_retry,'rgb_sha256':expected_hash,
            'scope':'own physical phone-code/menu comparison; not factory provisioning, persistent contacts or radio'}


def run_machine(binary, run, script, seconds, verbose=False):
    command = [str(binary.resolve()),'nsk1r4t','-bios','561a','-rompath','inputs',
               '-noreadconfig','-video','none','-sound','none','-nothrottle','-log',
               '-seconds_to_run',str(seconds),'-debug','-debugger','none','-autoboot_delay','0',
               '-autoboot_script',str(script.resolve()),*(['-verbose'] if verbose else [])]
    with (run/'console.log').open('w') as output:
        process = subprocess.run(command,cwd=run,stdout=output,stderr=subprocess.STDOUT,timeout=180)
    if process.returncode:
        raise ValueError(f'MAME exited {process.returncode}')
    return command


def check_phase(run, initial, initialized_retry=True, through=30, minimum_records=2):
    trace = (run/'error.log').read_text()
    if '[LUA ERROR]' in (run/'console.log').read_text():
        raise ValueError('security fixture console Lua error')
    for name,digest in INPUTS:
        if hashlib.sha1((run/'inputs/nsk1r4t'/Path(name).name).read_bytes()).hexdigest() != digest:
            raise ValueError('own compatibility input changed')
    image = (run/'inputs/nsk1r4t/5130f561a.fls').read_bytes()
    summary = summarize(trace,image,'standard')
    adc = re.findall(r'ccont_adc2_config: raw=([0-9a-f]{3}) external_input=1',trace)
    if not adc or any(value != '22a' for value in adc):
        raise ValueError('nominal ADC2 configuration was not applied')
    storage = run/'nvram/nsk1r4t'
    final = (storage/'eeprom').read_bytes()
    if (hashlib.sha1((storage/'flash').read_bytes()).hexdigest() != INPUTS[0][1]
            or len(initial) != 2048 or len(final) != 2048 or final[:0x20] != initial[:0x20]
            or final[0x2a0:0x2a4] != initial[0x2a0:0x2a4]
            or final[0x2a4] != (0 if initialized_retry else 255)
            or summary['native_illegal_observed']
            or [row['time'] for row in summary['late_samples']] != [time for time in (10,20,30,40,50,59) if time <= through]):
        raise ValueError('native/storage/late observation boundary differs')
    notifications = [line for line in trace.splitlines() if 'nsk1_record_reply:' in line and 'payload=35340000' in line]
    summary['record_arithmetic'] = check_records('\n'.join(line for line in trace.splitlines() if line not in notifications),minimum=minimum_records)
    summary['record_update_notifications_unqualified'] = notifications
    return summary,trace


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary',type=Path)
    parser.add_argument('roms',type=Path)
    parser.add_argument('capture',type=Path)
    parser.add_argument('run',type=Path)
    parser.add_argument('--initialized-retry',action='store_true')
    args = parser.parse_args()
    run = args.run.resolve()
    try:
        initial = prepare(args.roms.resolve(),run,args.capture.resolve(),True,True,True,True,args.initialized_retry)
        command = run_machine(args.binary,run,Path(__file__).with_name('nsk1_security_fixture.lua'),31)
        summary,trace = check_phase(run,initial,args.initialized_retry)
        summary['physical_security'] = check(trace,run/'snap',args.initialized_retry)
        summary['command'] = command
        (run/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
        print(json.dumps(summary['physical_security'],indent=2))
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        parser.exit(1,f'NSK-1 security comparison failed: {error}; inspect {run}\n')


if __name__ == '__main__':
    main()
