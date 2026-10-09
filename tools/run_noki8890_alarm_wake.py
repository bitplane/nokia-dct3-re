"""Own-provisioned 8890 power-off, RTC wake, security acceptance and alarm Stop."""
import argparse
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.noki8890_staged_check import verify as verify_stage
from tools.noki8890_registration_check import verify as verify_registration
from tools.run_noki8890_host_sms import check_output

FRAMES = {
    'off': '7d9978ed11e23fdb98a9251da90ed9a3c299066e104d69c32f161a9b86d119b9',
    'security': '377d72ab8f14506d541afd5cb32873c0e18c4492c52a2c18574e1af0067eebf0',
    'security_6': 'e6b40b0c8e7d3dbddd20d27db699b60044e946cde97a94f73ce809750f379810',
    'ringing': '1672d5a5cf0bacdb3338a17ce2768e7aa5832860b9bb807d5cf34bfa67231078',
    'stopped': '61452d14f7c581b28ebfd720920333752c3d86e0f7cc51fde46fd3ca679b4973',
}


def check_wake(text):
    actions = ['menu', 'menu_2', 'menu_3', 'menu_4', 'settings', 'alarm']
    actions += [f'time_{i}' for i in range(1, 5)] + ['confirm', 'idle', 'power_off', 'power_release']
    actions += [f'security_{i}' for i in range(1, 7)] + ['stop']
    if re.findall(r'8890_alarm_wake_physical: action=([^\r\n]+)', text) != actions:
        raise ValueError('8890 alarm-wake physical sequence differs')
    rails = re.findall(r'ccont_power: event=(off|wake)(?: cause=([0-9a-f]+))? t=([0-9.]+)', text)
    if len(rails) != 2 or rails[0][0] != 'off' or not 38 < float(rails[0][2]) < 45:
        raise ValueError('expected one physical power-off before RTC wake')
    if rails[1] != ('wake', '80', '60.000000000'):
        raise ValueError('expected one natural alarm-only RTC wake at second 60')
    deadline = re.search(r'event=second time=13:49:00 day=0 [^\n]*t=([0-9.]+)', text)
    if deadline is None or float(deadline[1]) != 60:
        raise ValueError('alarm wake did not coincide with its programmed RTC minute')
    compare = '8890_changed_code: event=compare result=00000000 stored=d87d3698 input=d87d3698'
    if re.findall(r'8890_changed_code: event=input bytes=([^\r\n]+)', text) != ['353433323100'] * 2:
        raise ValueError('both physical code entries must be the saved five digits')
    comparisons = re.findall(r'8890_changed_code: event=compare[^\r\n]*', text)
    if comparisons != [compare] * 2:
        raise ValueError('both cold and alarm-wake validators must match')
    cursor = 0
    for event in [compare, '8890_alarm_wake_physical: action=confirm\n',
                  'event=alarm_write reg=0b data=31 armed=1 ',
                  'event=alarm_write reg=0c data=0d armed=1 ',
                  '8890_alarm_wake_physical: action=power_off\n',
                  '8890_alarm_wake_physical: action=power_release\n',
                  'ccont_power: event=off ', 'ccont_power: event=wake cause=80 ',
                  'event=cause_read data=b3 ', 'event=status_ack data=a0 ',
                  'buzzer: enabled=1 ', '8890_alarm_wake_physical: action=security_6\n',
                  compare, '8890_alarm_wake_physical: action=stop\n']:
        position = text.find(event, cursor)
        if position < 0:
            raise ValueError('missing ordered alarm-wake evidence: ' + event)
        cursor = position + len(event)
    states = re.findall(r'buzzer: enabled=([01]) ', text)
    if not states or states[-1] != '0':
        raise ValueError('alarm buzzer remains enabled at the final idle boundary')
    # A digit may silence the alarm before Stop; do not invent a Stop-only edge.
    wake = text.index('ccont_power: event=wake cause=80 ')
    line_start = text.rfind('\n', 0, wake) + 1
    return text[:line_start], text[line_start:]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    mame = (args.mame or root / 'mame/mame').resolve()
    try:
        run.mkdir(parents=True, exist_ok=False)
        subprocess.run([sys.executable, str(root / 'tools/run_noki8890_code_change.py'),
                        str(run / 'code'), '--mame', str(mame)], check=True, timeout=800)
        alarm = run / 'alarm'
        alarm.mkdir()
        for name in ('nvram', 'cfg'):
            shutil.copytree(run / 'code/save' / name, alarm / name)
        command = [str(mame), 'nsb6hle', '-rompath', str(root / 'roms'),
                   '-nvram_directory', 'nvram', '-cfg_directory', 'cfg',
                   '-snapshot_directory', 'snap', '-noreadconfig', '-debug',
                   '-debugger', 'none', '-verbose', '-log', '-video', 'none',
                   '-sound', 'none', '-nothrottle', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / 'tools/noki8890_alarm_wake_input.lua'),
                   '-seconds_to_run', '100']
        with (alarm / 'console.log').open('w') as output:
            subprocess.run(command, cwd=alarm, stdout=output, stderr=subprocess.STDOUT,
                           check=True, timeout=180)
        check_output((alarm / 'console.log').read_text(errors='replace'))
        phases = check_wake((alarm / 'error.log').read_text(errors='replace'))
        for phase in phases:
            verify_stage(phase, runtime=True, selftest=True)
            verify_registration(phase, configured_gsm900=True, preserved_location=True)
        from PIL import Image
        for name, expected in FRAMES.items():
            with Image.open(alarm / f'snap/8890_alarm_wake_{name}.png') as image:
                if image.size != (84, 48) or hashlib.sha256(image.convert('L').tobytes()).hexdigest() != expected:
                    raise ValueError('reviewed alarm-wake frame differs: ' + name)
        print('8890 alarm wake: PASS own physical provisioning, RTC-only rail wake, code acceptance and Stop to registered idle; research HLE, no activation-choice/native-audio claim')
        return 0
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f'8890 alarm wake: FAIL: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
