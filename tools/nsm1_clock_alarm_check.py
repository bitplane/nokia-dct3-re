"""Own 6150 clock initialization and physical alarm/Stop lifecycle."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_toolkit_check import prepare as prepare_card,check_configuration
from tools.nsm1_native_state_check import check_storage
from tools.nsm1_security_check import check_trace as check_phone
from tools.nsk3_pin_check import check_trace as check_pin
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_toolkit_display_check import wire,check_observer_console
from tools.sim_fdn_protocol_check import apdus

EEPROM='f8da8575410d5a83aca7a73dffd9b3e320aefae4ce935df4190238ce886323d3'
RTC_INPUT=bytes.fromhex('1a000c010000005000')
RTC_ENDPOINT=bytes.fromhex('0e230c00001e0e5001')
FRAMES={
    'clock_required':'b756853be083142f76ec7aca8397ac4bc8aedcd78e05467f2124577aa7fd3445',
    'time_1234':'48396da059bea79780fabbbe1477f3858daa5da1e3a4ebccbd5a6bff52a37bd5',
    'time_confirm':'8c2311d07ffd160e6dd9ef6499f904496410d1564052dc110714dd3e945a2e0a',
    'date_01012000':'4336458b4cd36cfb0ecb31d2323d7a0e34344a1584c9176122d37c3b5a29cd54',
    'date_confirm':'65aa13c4b04fab313192a8de242ae54a2732b0d5a573e205859610c539500fc1',
    'alarm_1235':'296809a670ab2583c48c6c5beed6bb2b2a52d2a6fad9f86168a8db84b6779017',
    'alarm_confirm':'780783de7c0bafeb3ac63a14b9396072184e11adc3bdff72850795065781e7db',
    'idle':'5194acd4b2092e6455d528e9dc8b7a66fae0fe2d2fa143092eb48666d7f104ba',
    'watch_104':'00c2c46ddffa4da4c0a8735e379302e129bc3fd79a51b175c31f96689c858921',
}
IDLE_AFTER='45b9a5a8c4ff0cc64ef641410bcc6452051fd0a2612031c1e6387d64a740f674'
CONTACTS=[(25,'Left Softkey',0x19),(27,'Keypad 4',4),
          (29,'Left Softkey',0x19),(31,'Left Softkey',0x19)]
CONTACTS += [(33+i,'Keypad '+str(v),v) for i,v in enumerate((1,2,3,4))]
CONTACTS += [(37,'Left Softkey',0x19)]
CONTACTS += [(41+i,'Keypad '+str(v),v or 0x0a) for i,v in enumerate((0,1,0,1,2,0,0,0))]
CONTACTS += [(49,'Left Softkey',0x19)]
CONTACTS += [(55+i,'Keypad '+str(v),v) for i,v in enumerate((1,2,3,5))]
CONTACTS += [(59,'Left Softkey',0x19),(63,'End / Mode',0x0f)]


def check_authentication(text,*,wire_limit=8192):
    check_observer_console(text)
    check_pin(text,True,prefix='nsm1',tx_limit=4096)
    check_phone(text,True,())
    if any(s in text for s in ('nsm1_restart_request:','unimplemented C54x','unsupported C54x')):
        raise ValueError('restart or unsupported native execution')
    commands=list(apdus(wire(text,'TX','nsm1',limit=wire_limit)))
    wire(text,'RX','nsm1',limit=wire_limit)
    if (not commands or any(row[0] not in (0xa4,0x20,0xb0,0xb2,0xc0,0xf2) for row in commands)
            or 'read-binary fid=6fae offset=0 length=1 first=02' not in text):
        raise ValueError('unrelated or proactive SIM operation')


def check_trace(text,*,control=False):
    check_authentication(text)
    contacts=CONTACTS+([] if control else [(105,'Left Softkey',0x19)])
    expected=[(name,f'{key:02x}',str(down),f'{time+(0 if down else .2):.9f}')
              for time,name,key in contacts for down in (1,0)]
    rows=re.findall(r'nsm1_clock_contact: name=([^\n]+?) key=([0-9a-f]{2}) down=([01]) t=([0-9.]+) physical_only=1',text)
    if rows!=expected or text.count('nsm1_clock_contact:')!=len(expected):
        raise ValueError('physical clock/alarm contact sequence differs')
    writes=re.findall(r'ccont_rtc: event=alarm_write reg=([0-9a-f]{2}) data=([0-9a-f]{2}) armed=([01]) t=([0-9.]+)',text)
    if (not any(r[:2]==('0b','22') and 38<float(r[3])<40 for r in writes)
            or not any(r[:2]==('0c','8c') and 38<float(r[3])<40 for r in writes)
            or [r[:2] for r in writes if 59<float(r[3])<60]!=[('0b','23'),('0c','0c')]):
        raise ValueError('firmware clock latch or alarm programming differs')
    rollover='ccont_rtc: event=second time=12:35:00 day=0 status=b3 mask=50 t=98.000000000'
    acknowledgements=re.findall(r'ccont_rtc: event=status_ack data=a0 old=b3 t=([0-9.]+)',text)
    if text.count(rollover)!=1 or len(acknowledgements)!=1 or not 98<float(acknowledgements[0])<98.001:
        raise ValueError('RTC alarm/minute delivery or firmware acknowledgement differs')
    pulses=[float(t) for t in re.findall(r'buzzer: enabled=1 divider=5285 frequency=2459 volume=3 t=([0-9.]+)',text)]
    if sum(98<t<105 for t in pulses)<10 or any(65<t<97 for t in pulses):
        raise ValueError('organic alarm buzzer onset differs')
    if (not control and any(105.5<t<=112 for t in pulses)) or (control and sum(106<t<=112 for t in pulses)<5):
        raise ValueError('Stop/control buzzer lifecycle differs')
    native=re.findall(r'nsm1_clock_native: pc=([0-9a-f]{4}) stop=0 t=111.500000000 observer_writes=0',text)
    if len(native)!=1 or text.count('nsm1_clock_native:')!=1:
        raise ValueError('late native execution observation missing')
    return dict(alarm_time='12:35',alarm_event_seconds=98,physical_stop=not control,
                buzzer_pulses_before_stop=sum(98<t<105 for t in pulses),
                native_speech_claim=False,physical_clock_accuracy_proved=False)


def prepare(seed,run):
    if (seed/'nvram/nsm1r4t/ccont').read_bytes()!=RTC_INPUT:
        raise ValueError('qualified retained-clock input differs')
    prepare_card(seed,run,control=True)
    (run/'initial_ccont.bin').write_bytes(RTC_INPUT)


def execute(binary,seed,run,*,control=False):
    prepare(seed,run)
    script=Path(__file__).with_name('nsm1_clock_alarm_control.lua' if control else 'nsm1_clock_alarm.lua').resolve()
    command=[str(binary),'nsm1r4t','-rompath',str(Path(__file__).resolve().parents[1]/'roms'),
             '-noreadconfig','-video','none','-sound','none','-nothrottle','-joystickprovider','none',
             '-midiprovider','none','-debug','-debugger','none','-log','-verbose',
             '-seconds_to_run','112','-autoboot_delay','0','-autoboot_script',str(script)]
    (run/'invocation.json').write_text(json.dumps(dict(command=command,
        mame_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
        script_sha256=hashlib.sha256(script.read_bytes()).hexdigest()),indent=2)+'\n')
    with (run/'console.log').open('w') as output:
        result=subprocess.run(command,cwd=run,stdout=output,stderr=subprocess.STDOUT,timeout=240)
    (run/'exit_status.json').write_text(json.dumps(dict(returncode=result.returncode))+'\n')


def check(seed,run,*,control=False):
    before=check_storage(seed)
    after=check_storage(run,eeprom_sha256=EEPROM)
    if before['flash']!=after['flash'] or before['sim_card']!=after['sim_card']:
        raise ValueError('clock fixture altered card or executable')
    metadata=dict(source=str(seed.resolve()),source_sha256={k:hashlib.sha256(v).hexdigest() for k,v in before.items()},
                  copied_directories=['nvram','cfg'],ram_or_save_state_copied=False)
    if json.loads((run/'cold_input.json').read_text())!=metadata:
        raise ValueError('cold provenance differs')
    for name in ('eeprom','sim_card'):
        if (run/f'initial_{name}.bin').read_bytes()!=before[name]:
            raise ValueError('cold initial storage differs')
    if (seed/'nvram/nsm1r4t/ccont').read_bytes()!=RTC_INPUT or (run/'initial_ccont.bin').read_bytes()!=RTC_INPUT:
        raise ValueError('retained clock input differs')
    if json.loads((run/'exit_status.json').read_text())!={'returncode':0}:
        raise ValueError('MAME did not complete')
    check_configuration(seed,run,control=True)
    check_observer_console((run/'console.log').read_text())
    result=check_trace((run/'error.log').read_text(),control=control)
    for name,digest in FRAMES.items():
        frame_check(run/f'snap/clock_{name}.png',digest)
    if not control:
        frame_check(run/'snap/clock_final.png',IDLE_AFTER)
    rtc=(run/'nvram/nsm1r4t/ccont').read_bytes()
    if rtc!=RTC_ENDPOINT:
        raise ValueError('complete retained RTC endpoint differs')
    return result


def check_pair(seed,run,control):
    result=check(seed,run)
    check(seed,control,control=True)
    identities=[json.loads((path/'invocation.json').read_text()).get('mame_sha256')
                for path in (run,control)]
    if (not isinstance(identities[0],str) or not re.fullmatch('[0-9a-f]{64}',identities[0])
            or identities[0]!=identities[1]):
        raise ValueError('Stop and control were not captured with the same executable')
    result['matched_no_stop_control']=True
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('seed',type=Path)
    parser.add_argument('run',type=Path)
    parser.add_argument('--mame',type=Path)
    parser.add_argument('--control',action='store_true')
    parser.add_argument('--matched-control',type=Path)
    args=parser.parse_args()
    try:
        seed,run=args.seed.resolve(),args.run.resolve()
        if args.control and args.matched_control:
            raise ValueError('matched control attaches to the Stop run only')
        if args.mame:
            execute(args.mame.resolve(),seed,run,control=args.control)
        result=(check_pair(seed,run,args.matched_control.resolve()) if args.matched_control
                else check(seed,run,control=args.control))
        (run/'clock_alarm_result.json').write_text(json.dumps(result,indent=2)+'\n')
    except (OSError,ValueError,KeyError,subprocess.TimeoutExpired) as exc:
        parser.exit(1,f'NSM-1 clock/alarm: FAIL: {exc}\n')
    print('NSM-1 physical clock/date/alarm lifecycle PASS; native speech and physical clock accuracy unproved')


if __name__=='__main__':
    main()
