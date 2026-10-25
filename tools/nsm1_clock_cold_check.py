"""Reopen the own 6150 clock endpoint without RAM or repeated setup input."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_clock_alarm_check import check_pair,check_authentication,EEPROM as SOURCE_EEPROM,RTC_ENDPOINT
from tools.nsm1_native_state_check import check_storage
from tools.nsm1_toolkit_check import check_configuration
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_toolkit_display_check import check_observer_console

EEPROM='4fdb494b68a913e30f92a2c48cbff9cd2e17aff00395b6e84aeb9f1f40ac6e89'
FRAMES={
    'alarm_value':'3b220f572706d99823929465e5e3807e29d92f101bdb741a200c78ebc75c697e',
    'clock_options':'99e37220f3e6c5e84e9faa202ba580b11ad30ddee21ca1d189230b2d26098dd0',
    'time_editor':'a92a0d0910ad6482a0428c7f2c93b52381bb9711eaa3e81ed3f4e58012f2b576',
    'idle':'45b9a5a8c4ff0cc64ef641410bcc6452051fd0a2612031c1e6387d64a740f674',
    'minute':'d4220b5330be9000e0efa481e407429174fec00bfe053b8a40e15a6e3520cd19',
    'final':'d4220b5330be9000e0efa481e407429174fec00bfe053b8a40e15a6e3520cd19',
}
CONTACTS=[(25,'Left Softkey',0x19),(27,'Keypad 4',4),(29,'Left Softkey',0x19),
          (31,'Right Softkey',0x1a),(33,'Down',0x18),(35,'Left Softkey',0x19),
          (37,'Down',0x18),(39,'Left Softkey',0x19),(41,'Right Softkey',0x1a),
          (43,'End / Mode',0x0f)]


def check_trace(text):
    check_authentication(text)
    expected=[(name,f'{code:02x}',str(down),f'{time+(0 if down else .2):.9f}')
              for time,name,code in CONTACTS for down in (1,0)]
    rows=re.findall(r'nsm1_clock_cold_contact: name=([^\n]+?) key=([0-9a-f]{2}) down=([01]) t=([0-9.]+) physical_only=1',text)
    if rows!=expected or text.count('nsm1_clock_cold_contact:')!=len(expected):
        raise ValueError('cold inspection/cancellation contacts differ')
    if 'ccont_rtc: event=alarm_write' in text or 'ccont_rtc: event=counter_write' in text:
        raise ValueError('cold readback reprogrammed clock or alarm')
    for event in ('time=12:35:15 day=0 status=13 mask=50 t=1.000000000',
                  'time=12:36:00 day=0 status=33 mask=50 t=46.000000000'):
        if text.count('ccont_rtc: event=second '+event)!=1:
            raise ValueError('retained counter or organic minute rollover differs')
    acks=re.findall(r'ccont_rtc: event=status_ack data=20 old=33 t=([0-9.]+)',text)
    if len(acks)!=1 or not 46<float(acks[0])<46.001:
        raise ValueError('minute interrupt acknowledgement differs')
    if any(44<float(t)<=62 for t in re.findall(r'buzzer: enabled=1 [^\n]+? t=([0-9.]+)',text)):
        raise ValueError('expired alarm or unrelated alert repeated during readback')
    native=re.findall(r'nsm1_clock_cold_native: pc=([0-9a-f]{4}) stop=0 t=61.500000000 observer_writes=0',text)
    if len(native)!=1 or text.count('nsm1_clock_cold_native:')!=1:
        raise ValueError('cold late native observation missing')
    return dict(cold_clock_readback=True,clock_reprogrammed=False,expired_alarm_repeated=False,
                minute_rollover_seconds=46,physical_clock_accuracy_proved=False,
                calendar_year_interpretation_proved=False,native_speech_claim=False)


def inputs(source):
    files=check_storage(source,eeprom_sha256=SOURCE_EEPROM)
    files['ccont']=(source/'nvram/nsm1r4t/ccont').read_bytes()
    if files['ccont']!=RTC_ENDPOINT:
        raise ValueError('qualified alarm RTC source differs')
    return files


def metadata(source,files):
    return dict(source=str(source.resolve()),source_sha256={k:hashlib.sha256(v).hexdigest() for k,v in files.items()},
                copied_directories=['nvram','cfg'],ram_or_save_state_copied=False)


def prepare(seed,source,control,run):
    if run.exists():
        raise ValueError('fresh cold output required')
    check_pair(seed,source,control)
    files=inputs(source)
    for name in ('nvram','cfg'):
        shutil.copytree(source/name,run/name)
    for name in ('eeprom','sim_card','ccont'):
        (run/f'initial_{name}.bin').write_bytes(files[name])
    (run/'cold_input.json').write_text(json.dumps(metadata(source,files),indent=2)+'\n')


def execute(binary,seed,source,control,run):
    prepare(seed,source,control,run)
    script=Path(__file__).with_name('nsm1_clock_cold.lua').resolve()
    command=[str(binary),'nsm1r4t','-rompath',str(Path(__file__).resolve().parents[1]/'roms'),
             '-noreadconfig','-video','none','-sound','none','-nothrottle','-joystickprovider','none',
             '-midiprovider','none','-debug','-debugger','none','-log','-verbose',
             '-seconds_to_run','62','-autoboot_delay','0','-autoboot_script',str(script)]
    (run/'invocation.json').write_text(json.dumps(dict(command=command,
        mame_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
        script_sha256=hashlib.sha256(script.read_bytes()).hexdigest()),indent=2)+'\n')
    with (run/'console.log').open('w') as output:
        result=subprocess.run(command,cwd=run,stdout=output,stderr=subprocess.STDOUT,timeout=180)
    (run/'exit_status.json').write_text(json.dumps(dict(returncode=result.returncode))+'\n')


def check(seed,source,control,run):
    check_pair(seed,source,control)
    before=inputs(source)
    after=check_storage(run,eeprom_sha256=EEPROM)
    if any(before[n]!=after[n] for n in ('sim_card','flash')):
        raise ValueError('cold clock readback changed card or executable')
    if json.loads((run/'cold_input.json').read_text())!=metadata(source,before):
        raise ValueError('cold clock provenance differs')
    for name in ('eeprom','sim_card','ccont'):
        if (run/f'initial_{name}.bin').read_bytes()!=before[name]:
            raise ValueError('cold clock initial storage differs')
    identities=[json.loads((p/'invocation.json').read_text()).get('mame_sha256') for p in (source,run)]
    if identities[0]!=identities[1]:
        raise ValueError('cold clock binary differs from its qualified source')
    if json.loads((run/'exit_status.json').read_text())!={'returncode':0}:
        raise ValueError('cold process did not complete')
    check_observer_console((run/'console.log').read_text())
    check_configuration(seed,run,control=True)
    result=check_trace((run/'error.log').read_text())
    for name,digest in FRAMES.items():
        frame_check(run/f'snap/clock_cold_{name}.png',digest)
    if (run/'nvram/nsm1r4t/ccont').read_bytes()!=bytes.fromhex('10240c00001e0e5001'):
        raise ValueError('whole cold retained-clock endpoint differs')
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('seed','source','control','run'):
        parser.add_argument(name,type=Path)
    parser.add_argument('--mame',type=Path)
    args=parser.parse_args()
    try:
        paths=[getattr(args,n).resolve() for n in ('seed','source','control','run')]
        if args.mame:
            execute(args.mame.resolve(),*paths)
        result=check(*paths)
        (paths[-1]/'clock_cold_result.json').write_text(json.dumps(result,indent=2)+'\n')
    except (OSError,ValueError,KeyError,subprocess.TimeoutExpired) as exc:
        parser.exit(1,f'NSM-1 cold clock: FAIL: {exc}\n')
    print('NSM-1 cold clock/expired-alarm readback PASS; no physical timing or native speech claim')


if __name__=='__main__':
    main()
