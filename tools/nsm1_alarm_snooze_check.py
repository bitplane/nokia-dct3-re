"""Own native 6150 physical Snooze/RTC re-alarm/Stop contract."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))

from tools.nsm1_clock_alarm_check import prepare,check_authentication,CONTACTS,FRAMES,RTC_INPUT,EEPROM
from tools.nsm1_native_state_check import check_storage
from tools.nsm1_toolkit_check import check_configuration
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_toolkit_display_check import check_observer_console


def check_trace(text):
    rows=re.findall(r'nsm1_alarm_snooze_contact: name=([^\n]+?) key=([0-9a-f]{2}) down=([01]) t=([0-9.]+) physical_only=1',text)
    expected=[(name,code,str(down),f'{time+(0 if down else .2):.9f}')
              for time,name,code in ((105,'Right Softkey','1a'),(465,'Left Softkey','19'))
              for down in (1,0)]
    if rows!=expected or text.count('nsm1_alarm_snooze_contact:')!=4:
        raise ValueError('physical Snooze/Stop contacts differ')
    writes=re.findall(r'ccont_rtc: event=alarm_write reg=([0-9a-f]{2}) data=([0-9a-f]{2}) armed=([01]) t=([0-9.]+)',text)
    if [r[:3] for r in writes if 105<float(r[3])<106]!=[('0b','29','1'),('0c','0c','1')]:
        raise ValueError('Snooze did not program retained alarm12:41')
    for time,clock in ((98,'12:35:00'),(458,'12:41:00')):
        marker=f'ccont_rtc: event=second time={clock} day=0 status=b3 mask=50 t={time}.000000000'
        if text.count(marker)!=1:
            raise ValueError('first or snoozed RTC alarm missing')
    acks=[float(t) for t in re.findall(r'ccont_rtc: event=status_ack data=a0 old=b3 t=([0-9.]+)',text)]
    if len(acks)!=2 or not all(time<t<time+.001 for time,t in zip((98,458),acks)):
        raise ValueError('alarm acknowledgements differ')
    pulses=[float(t) for t in re.findall(r'buzzer: enabled=1 divider=5285 frequency=2459 volume=3 t=([0-9.]+)',text)]
    if sum(98<t<105 for t in pulses)<10 or sum(458<t<465 for t in pulses)<10:
        raise ValueError('first or snoozed native buzzer onset missing')
    if any(106<t<458 or 466<t<=480 for t in pulses):
        raise ValueError('Snooze or Stop failed to silence buzzer')
    if text.count('nsm1_alarm_snooze_native:')!=1 or not re.search(
            r'nsm1_alarm_snooze_native: pc=[0-9a-f]{4} stop=0 t=479.500000000 observer_writes=0',text):
        raise ValueError('late native execution observation missing')
    if 'ccont_power: event=off' in text or 'ccont_power: event=wake' in text:
        raise ValueError('unexpected power transition in awake Snooze fixture')
    return dict(snoozed_alarm_seconds=458,physical_snooze=True,physical_stop=True,
                physical_duration_accuracy_proved=False,native_speech_claim=False)


def check(seed,run):
    before=check_storage(seed)
    after=check_storage(run,eeprom_sha256=EEPROM)
    if any(before[n]!=after[n] for n in ('flash','sim_card')):
        raise ValueError('Snooze changed card or executable')
    metadata=dict(source=str(seed.resolve()),source_sha256={k:hashlib.sha256(v).hexdigest() for k,v in before.items()},
                  copied_directories=['nvram','cfg'],ram_or_save_state_copied=False)
    if json.loads((run/'cold_input.json').read_text())!=metadata:
        raise ValueError('Snooze cold provenance differs')
    for name in ('eeprom','sim_card'):
        if (run/f'initial_{name}.bin').read_bytes()!=before[name]:
            raise ValueError('Snooze initial storage differs')
    if (run/'initial_ccont.bin').read_bytes()!=RTC_INPUT or (seed/'nvram/nsm1r4t/ccont').read_bytes()!=RTC_INPUT:
        raise ValueError('Snooze initial retained clock differs')
    if json.loads((run/'exit_status.json').read_text())!={'returncode':0}:
        raise ValueError('Snooze process did not complete')
    check_configuration(seed,run,control=True)
    check_observer_console((run/'console.log').read_text())
    text=(run/'error.log').read_text()
    check_authentication(text,wire_limit=32768)
    expected=[(name,f'{code:02x}',str(down),f'{time+(0 if down else .2):.9f}')
              for time,name,code in CONTACTS for down in (1,0)]
    rows=re.findall(r'nsm1_clock_contact: name=([^\n]+?) key=([0-9a-f]{2}) down=([01]) t=([0-9.]+) physical_only=1',text)
    if rows!=expected or text.count('nsm1_clock_contact:')!=len(expected):
        raise ValueError('Snooze clock setup contacts differ')
    result=check_trace(text)
    for name,digest in FRAMES.items():
        frame_check(run/f'snap/clock_{name}.png',digest)
    for name,digest in {
            'selected':'eca93b34d4b6f49924ec63b62f9e678eae875f36eca92dec98434becaadb7782',
            '464':'4e689b878b4b2e4e4bb8430af591861657bb61276a004424b0a6c0ad64b8161b',
            'final':'6e352dac9e494e73466f4a118e1bd03e6cc00bc8293b268304a4f6a4ca74320f'}.items():
        frame_check(run/f'snap/snooze_{name}.png',digest)
    if (run/'nvram/nsm1r4t/ccont').read_bytes()!=bytes.fromhex('16290c00001e0e5001'):
        raise ValueError('whole Snooze RTC endpoint differs')
    return result


def execute(binary,seed,run):
    prepare(seed,run)
    script=Path(__file__).with_name('nsm1_alarm_snooze.lua').resolve()
    command=[str(binary),'nsm1r4t','-rompath',str(Path(__file__).resolve().parents[1]/'roms'),
             '-noreadconfig','-video','none','-sound','none','-nothrottle','-joystickprovider','none',
             '-midiprovider','none','-debug','-debugger','none','-log','-verbose',
             '-seconds_to_run','480','-autoboot_delay','0','-autoboot_script',str(script)]
    (run/'invocation.json').write_text(json.dumps(dict(command=command,
        mame_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
        script_sha256=hashlib.sha256(script.read_bytes()).hexdigest()),indent=2)+'\n')
    with (run/'console.log').open('w') as output:
        result=subprocess.run(command,cwd=run,stdout=output,stderr=subprocess.STDOUT,timeout=600)
    (run/'exit_status.json').write_text(json.dumps(dict(returncode=result.returncode))+'\n')


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
        (run/'alarm_snooze_result.json').write_text(json.dumps(result,indent=2)+'\n')
    except (OSError,ValueError,KeyError,subprocess.TimeoutExpired) as exc:
        parser.exit(1,f'NSM-1 Snooze: FAIL: {exc}\n')
    print('NSM-1 physical Snooze/re-alarm/Stop PASS; physical timing and native speech unproved')


if __name__=='__main__':
    main()
