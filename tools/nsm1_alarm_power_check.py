"""Observed native 6150 rail-off alarm contract, not full clock-gating proof."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))

from tools.power_domain_contract import require_endpoint_silence
from tools.nsm1_clock_alarm_check import prepare,check_authentication,CONTACTS,RTC_INPUT,FRAMES
from tools.nsm1_native_state_check import check_storage
from tools.nsm1_toolkit_check import check_configuration
from tools.nsm1_security_check import check_trace as check_phone
from tools.sim_security_trace_check import statuses
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_toolkit_display_check import check_observer_console
from tools.sim_toolkit_display_check import wire
from tools.sim_fdn_protocol_check import apdus

EEPROM='1ce678a609574b51dfccdaa7c091ea520302c718f996f46cf011f1fb1c0d6660'
BLANK='5fdab89cdada4b6ac821d32facb589dfb541e9856fe3b569e8da49646fc3d249'


def check_power_trace(text, *, activate=False, horizon=140):
    rails=list(re.finditer(r'ccont_power: event=(off|wake)(?: cause=([0-9a-f]{2}))? t=([0-9.]+)',text))
    expected=['off','wake'] if activate else ['off','wake','off']
    if [row[1] for row in rails]!=expected:
        raise ValueError('unexpected rail transitions')
    off,wake=rails[:2]
    if not 70<float(off[3])<72 or wake[2]!='80' or float(wake[3])!=98:
        raise ValueError('physical shutdown or retained alarm wake differs')
    require_endpoint_silence(text[off.end():wake.start()], 'powered endpoint active before alarm wake')
    ticks=re.findall(r'ccont_rtc: event=second time=(\d+):(\d+):(\d+) day=(\d+) .*? t=([0-9.]+)',text)
    off_ticks=[row for row in ticks if float(off[3])<float(row[4])<98]
    if len(off_ticks)!=27:
        raise ValueError('retained RTC observation incomplete while off')
    for left,right in zip(off_ticks,off_ticks[1:]):
        def seconds(row):
            return int(row[3])*86400+int(row[0])*3600+int(row[1])*60+int(row[2])
        if seconds(right)-seconds(left)!=1 or float(right[4])-float(left[4])!=1:
            raise ValueError('retained RTC discontinuity')
    acknowledgements=re.findall(r'ccont_rtc: event=status_ack data=a0 old=b3 t=([0-9.]+)',text)
    if len(acknowledgements)!=1 or not 100<float(acknowledgements[0])<101:
        raise ValueError('post-wake firmware alarm acknowledgement differs')
    pulses=[float(t) for t in re.findall(r'buzzer: enabled=1 divider=5285 frequency=2459 volume=3 t=([0-9.]+)',text)]
    if sum(100<t<115 for t in pulses)<10 or any(116<t<=horizon for t in pulses):
        raise ValueError('alarm ringing or physical Stop lifecycle differs')
    if not activate:
        final_off=rails[2]
        if not 119<float(final_off[3])<126:
            raise ValueError('No choice did not shut down')
        require_endpoint_silence(text[final_off.end():], 'powered endpoint active after No shutdown')
        late=[float(row[4]) for row in ticks if float(row[4])>float(final_off[3])]
        if not late or max(late)-float(final_off[3])<10:
            raise ValueError('final shutdown observation too short')
    return dict(alarm_wake_seconds=98,activation=activate,rtc_ticks_while_off=len(off_ticks),
                full_clock_gating_proved=False,native_radio_claim=False)


def check(seed,run,*,activate=False):
    before=check_storage(seed)
    after=check_storage(run,eeprom_sha256=EEPROM)
    if any(before[n]!=after[n] for n in ('flash','sim_card')):
        raise ValueError('card or executable changed')
    expected=dict(source=str(seed.resolve()),source_sha256={k:hashlib.sha256(v).hexdigest() for k,v in before.items()},
                  copied_directories=['nvram','cfg'],ram_or_save_state_copied=False)
    if json.loads((run/'cold_input.json').read_text())!=expected:
        raise ValueError('cold source provenance differs')
    for name in ('eeprom','sim_card'):
        if (run/f'initial_{name}.bin').read_bytes()!=before[name]:
            raise ValueError('initial storage differs')
    if (run/'initial_ccont.bin').read_bytes()!=RTC_INPUT or (seed/'nvram/nsm1r4t/ccont').read_bytes()!=RTC_INPUT:
        raise ValueError('retained RTC input differs')
    if json.loads((run/'exit_status.json').read_text())!={'returncode':0}:
        raise ValueError('MAME did not complete')
    check_configuration(seed,run,control=True)
    check_observer_console((run/'console.log').read_text())
    text=(run/'error.log').read_text()
    if any(s in text for s in ('unimplemented C54x','unsupported C54x')):
        raise ValueError('unsupported native execution')
    # Own 2818dc loads reason0c before calling 2c2a02 on alarm activation.
    restarts=re.findall(r'nsm1_restart_request: reason=([0-9a-f]+) caller=([0-9a-f]+) task=([0-9a-f]+) t=([0-9.]+)',text)
    if (activate and (len(restarts)!=1 or restarts[0][:3]!=('0000000c','002818e3','01')
                     or not 119<float(restarts[0][3])<120)) or (not activate and restarts):
        raise ValueError('unexpected restart outside physical alarm activation')
    commands=list(apdus(wire(text,'TX','nsm1',limit=16384)))
    wire(text,'RX','nsm1',limit=16384)
    if not commands or any(row[0] not in (0xa4,0x20,0xb0,0xb2,0xc0,0xf2) for row in commands):
        raise ValueError('unrelated or proactive SIM command')
    check_authentication(text.split('ccont_power: event=wake')[0])
    check_phone(text,True,(),extra_codes=('3132333435000000',) if activate else ())
    if statuses(text,0x20)!=[(0x9000,3,10)]*(2 if activate else 1):
        raise ValueError('PIN authentication count/status differs')
    if any(statuses(text,ins) for ins in (0x24,0x26,0x28,0x2c)):
        raise ValueError('unexpected SIM security mutation')
    contacts=[(115,'Left Softkey',0x19),(119,'Left Softkey' if activate else 'Right Softkey',0x19 if activate else 0x1a)]
    if activate:
        contacts += [(127+i*.5,name,code) for i,(name,code) in enumerate([
            ('Keypad 1',1),('Keypad 2',2),('Keypad 3',3),('Keypad 4',4),('Left Softkey',0x19)])]
        contacts += [(135+i*.5,name,code) for i,(name,code) in enumerate([
            ('Keypad 1',1),('Keypad 2',2),('Keypad 3',3),('Keypad 4',4),('Keypad 5',5),('Left Softkey',0x19)])]
        contacts += [(143,'Left Softkey',0x19),(145,'Down',0x18)]
    for marker,sequence in (('nsm1_clock_contact',CONTACTS),('nsm1_alarm_power_contact',contacts)):
        rows=re.findall(marker+r': name=([^\n]+?) key=([0-9a-f]{2}) down=([01]) t=([0-9.]+) physical_only=1',text)
        expected=[(name,f'{code:02x}',str(down),f'{time+(0 if down else .2):.9f}')
                  for time,name,code in sequence for down in (1,0)]
        if rows!=expected:
            raise ValueError('physical clock/power-menu contact sequence differs')
    power=re.findall(r'nsm1_alarm_power_contact: name=Power down=([01]) t=([0-9.]+) physical_only=1',text)
    if power!=[('1','70.000000000'),('0','72.000000000')]:
        raise ValueError('physical power contact differs')
    result=check_power_trace(text,activate=activate,horizon=152 if activate else 140)
    frames={'80':BLANK,'104':'00c2c46ddffa4da4c0a8735e379302e129bc3fd79a51b175c31f96689c858921',
            'stop':'d40e0db4046e11ca40f455af0a938d1e95fa7ebe2d0ad337fdca4a9f295bf06a'}
    if activate:
        frames.update(final='a12b3c3fdb4a3d1e8ffeb5fc8986c3bc008bb86b1f2e927440848fe0420ffc0c',
            after_pin='24cbab0ad5b13376772a44f0eec7835a7ea865795c9ee229c7631fa2e0ea1fb2',
            menu='51a20a16acead610184225c78632724f9f20025565bd29a93beffec0c271d3a3',
            menu_final='ca560f3225d31b7446516549cf5357403bc674cbad5762050397e043a70a423f')
        if len(re.findall(r'nsm1_alarm_power_native: pc=[0-9a-f]{4} stop=0 t=151.500000000 observer_writes=0',text))!=1:
            raise ValueError('late native execution observation missing')
    else:
        frames.update(final=BLANK,off_final=BLANK)
    for name,digest in frames.items():
        frame_check(run/f'snap/alarm_power_{name}.png',digest)
    for name,digest in FRAMES.items():
        if name!='watch_104':
            frame_check(run/f'snap/clock_{name}.png',digest)
    rtc=bytes.fromhex('36230c00001e0e5001' if activate else '2a230c00001e0e5001')
    if (run/'nvram/nsm1r4t/ccont').read_bytes()!=rtc:
        raise ValueError('whole RTC endpoint differs')
    return result


def execute(binary,seed,run,*,activate=False):
    prepare(seed,run)
    script=Path(__file__).with_name('nsm1_alarm_power_yes.lua' if activate else 'nsm1_alarm_power.lua').resolve()
    command=[str(binary),'nsm1r4t','-rompath',str(Path(__file__).resolve().parents[1]/'roms'),
             '-noreadconfig','-video','none','-sound','none','-nothrottle','-joystickprovider','none',
             '-midiprovider','none','-debug','-debugger','none','-log','-verbose',
             '-seconds_to_run','152' if activate else '140','-autoboot_delay','0','-autoboot_script',str(script)]
    (run/'invocation.json').write_text(json.dumps(dict(command=command,
        mame_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
        script_sha256=hashlib.sha256(script.read_bytes()).hexdigest()),indent=2)+'\n')
    with (run/'console.log').open('w') as output:
        result=subprocess.run(command,cwd=run,stdout=output,stderr=subprocess.STDOUT,timeout=240)
    (run/'exit_status.json').write_text(json.dumps(dict(returncode=result.returncode))+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('seed',type=Path)
    parser.add_argument('run',type=Path)
    parser.add_argument('--mame',type=Path)
    parser.add_argument('--activate',action='store_true')
    args=parser.parse_args()
    try:
        seed,run=args.seed.resolve(),args.run.resolve()
        if args.mame:
            execute(args.mame.resolve(),seed,run,activate=args.activate)
        result=check(seed,run,activate=args.activate)
        (run/'alarm_power_result.json').write_text(json.dumps(result,indent=2)+'\n')
    except (OSError,ValueError,KeyError,subprocess.TimeoutExpired) as exc:
        parser.exit(1,f'NSM-1 powered-off alarm: FAIL: {exc}\n')
    print('NSM-1 powered-off alarm lifecycle PASS; native radio and full clock gating unproved')


if __name__=='__main__':
    main()
