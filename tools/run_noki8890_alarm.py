"""Own-ROM physical 8890 alarm entry, natural expiry and Stop acceptance."""
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
    'confirm': '08abb8a86e22c06418114339b180f4e4d6e622eb4dda89cfe031484e31964df6',
    'expiry_4': '5c6d10da719b404c66c41dbe165792c8c509a7e4858202a1bf1db6b0cadbb567',
    'stopped': 'e40423cea2d28b4aebf15cf10c650216ed2137f1caacfd294dcc31e856b90524',
}
SNOOZE_FRAMES = {
    'snooze': 'e795c0845db3095db7702b9885c436939a1bcd370265dfd62322cb6aacbc2121',
    'repeated': 'e22c4548014aea94a8a82b70ba8a9d466cb2e527a296ce5265995b53601fe594',
    'stopped': '77b0fc25848be29c70ded6efcb2d4921ff1850e4b2a07a9c215f0ba727f693f1',
}


def check_alarm(text, *, snooze=False):
    actions = ['menu', 'menu_2', 'menu_3', 'menu_4', 'settings', 'alarm']
    actions += [f'time_{index}' for index in range(1, 5)] + ['confirm', 'idle']
    actions += (['snooze', 'stop'] if snooze else ['stop'])
    if re.findall(r'8890_alarm_physical: action=([^\r\n]+)', text) != actions:
        raise ValueError('8890 physical alarm action sequence differs')
    cursor = 0
    events = [
        '8890_alarm_physical: action=confirm\n',
        'event=alarm_write reg=0b data=30 armed=1 ',
        'event=alarm_write reg=0c data=0d armed=1 ',
        '8890_alarm_physical: action=idle\n',
        'event=second time=13:48:00 day=0 ',
        'event=cause_read data=b3 ',
        'event=status_ack data=a0 ',
        'buzzer: enabled=1 ',
        '8890_alarm_physical: action=stop\n',
        'buzzer: enabled=0 ',
    ]
    for event in events:
        position = text.find(event, cursor)
        if position < 0:
            raise ValueError('missing ordered 8890 alarm evidence: ' + event)
        cursor = position + len(event)
    states = re.findall(r'buzzer: enabled=([01]) ', text[cursor:])
    if states and states[-1] != '0':
        raise ValueError('8890 alarm buzzer remains enabled after Stop')
    if '8890_clock_physical:' in text or 'ccont_power: event=wake' in text:
        raise ValueError('8890 alarm replaced clock input or introduced rail wake')
    deadline = re.search(r'event=second time=13:48:00 day=0 [^\n]*t=([0-9.]+)', text)
    if deadline is None or float(deadline[1]) != 60:
        raise ValueError('8890 alarm did not reach its natural RTC deadline')
    if snooze:
        cursor = text.index('8890_alarm_physical: action=snooze\n')
        for event in ['event=alarm_write reg=0b data=36 armed=1 ',
                      'event=alarm_write reg=0c data=0d armed=1 ',
                      'buzzer: enabled=0 ',
                      'event=second time=13:54:00 day=0 ',
                      'event=cause_read data=b3 ',
                      'event=status_ack data=a0 ',
                      'buzzer: enabled=1 ',
                      '8890_alarm_physical: action=stop\n',
                      'buzzer: enabled=0 ']:
            position = text.find(event, cursor)
            if position < 0:
                raise ValueError('missing ordered 8890 Snooze evidence: ' + event)
            cursor = position + len(event)
        deadline = re.search(r'event=second time=13:54:00 day=0 [^\n]*t=([0-9.]+)', text)
        if deadline is None or float(deadline[1]) != 420:
            raise ValueError('8890 Snooze did not reach its programmed RTC deadline')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--snooze', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    mame = (args.mame or root / 'mame/mame').resolve()
    try:
        run.mkdir(parents=True, exist_ok=False)
        subprocess.run([sys.executable, str(root / 'tools/run_noki8890_cold_clock.py'),
                        str(run / 'clock'), '--mame', str(mame)], check=True, timeout=400)
        alarm = run / 'alarm'
        alarm.mkdir()
        for name in ('nvram', 'cfg'):
            shutil.copytree(run / 'clock/seed' / name, alarm / name)
        command = [str(mame), 'nsb6hle', '-rompath', str(root / 'roms'),
                   '-nvram_directory', 'nvram', '-cfg_directory', 'cfg',
                   '-snapshot_directory', 'snap', '-noreadconfig', '-debug',
                   '-debugger', 'none', '-verbose', '-log', '-video', 'none',
                   '-sound', 'none', '-nothrottle', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / 'tools' / (
                       'noki8890_alarm_snooze.lua' if args.snooze else 'noki8890_alarm_input.lua')),
                   '-seconds_to_run', '445' if args.snooze else '85']
        with (alarm / 'console.log').open('w') as output:
            subprocess.run(command, cwd=alarm, stdout=output, stderr=subprocess.STDOUT,
                           check=True, timeout=400 if args.snooze else 180)
        check_output((alarm / 'console.log').read_text(errors='replace'))
        text = (alarm / 'error.log').read_text(errors='replace')
        check_alarm(text, snooze=args.snooze)
        verify_stage(text, runtime=True, selftest=True)
        verify_registration(text, configured_gsm900=True, preserved_location=True)
        from PIL import Image
        frames = {**FRAMES, **SNOOZE_FRAMES} if args.snooze else FRAMES
        for name, expected in frames.items():
            with Image.open(alarm / f'snap/8890_alarm_{name}.png') as frame:
                if frame.size != (84, 48) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != expected:
                    raise ValueError('8890 alarm frame differs: ' + name)
        print('8890 alarm: PASS ' + ('physical Snooze and natural 13:54 recurrence, ' if args.snooze else '') +
              'own physical entry, natural RTC expiry, buzzer control, '
              'physical Stop and reviewed idle; research HLE, not audible-output acceptance')
        return 0
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f'8890 alarm: FAIL: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
