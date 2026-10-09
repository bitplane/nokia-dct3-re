"""Physical 8890 phone-code change and independent cold retention acceptance."""
import argparse
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.run_noki8890_host_sms import check_output
from tools.noki8890_staged_check import verify as verify_stage

SAVE_HASH = '9cf9dd1b881e5e7df20f8ff2d9692f0bf238eaba121ca2d58681bdb243e1aad2'
COLD_HASH = '597ea9e4c5220e45bf33481f744da21dde9a5beffd877f357470123678f1e0b0'


def check_change(saved, cold):
    actions = ['menu', 'menu_2', 'menu_3', 'menu_4', 'settings']
    actions += [f'settings_{i}' for i in range(2, 7)] + ['security']
    actions += [f'security_{i}' for i in range(2, 6)] + ['access_codes', 'access_2', 'access_3', 'old_prompt']
    actions += [f'old_{i}' for i in range(1, 6)] + ['new_prompt']
    actions += [f'new_{i}' for i in range(5, 0, -1)] + ['confirm_prompt']
    actions += [f'confirm_{i}' for i in range(5, 0, -1)] + ['result']
    if re.findall(r'8890_code_change_physical: action=([^\r\n]+)', saved) != actions:
        raise ValueError('physical code-change sequence differs')
    confirm = saved.index('8890_code_change_physical: action=result\n')
    persist = re.findall(r'8890_code_change: event=persist encoded=([0-9a-f]+) caller=([0-9a-f]+)', saved)
    if persist != [('d87d3698', '002fa503')]:
        raise ValueError('missing unique firmware-owned persistent code write')
    if saved.index('8890_code_change: event=persist') < confirm:
        raise ValueError('code persisted before confirmation')
    keys = re.findall(r'8890_changed_code_physical: key=([^\r\n]+)', cold)
    if keys != ['Keypad 5', 'Keypad 4', 'Keypad 3', 'Keypad 2', 'Keypad 1', 'Menu']:
        raise ValueError('cold physical changed-code sequence differs')
    entered = re.findall(r'8890_changed_code: event=input bytes=([0-9a-f]+)', cold)
    results = re.findall(r'8890_changed_code: event=compare result=([0-9a-f]+) stored=([0-9a-f]+) input=([0-9a-f]+)', cold)
    if entered != ['353433323100'] or results != [('00000000', 'd87d3698', 'd87d3698')]:
        raise ValueError('cold validator did not accept the organically saved code')
    if not (cold.index('8890_changed_code_physical: key=Menu\n') <
            cold.index('8890_changed_code: event=input') <
            cold.index('8890_changed_code: event=compare')):
        raise ValueError('cold validation preceded physical confirmation')


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
        subprocess.run([sys.executable, str(root / 'tools/run_noki8890_cold_clock.py'),
                        str(run / 'clock'), '--mame', str(mame)], check=True, timeout=400)
        traces = []
        source = run / 'clock/seed'
        for name, script, seconds, frame, expected in [
                ('save', 'noki8890_code_change_input.lua', 80, '8890_code_change_result.png', SAVE_HASH),
                ('cold', 'noki8890_changed_code_input.lua', 30, '8890_changed_code_cold.png', COLD_HASH)]:
            stage = run / name
            stage.mkdir()
            for directory in ('nvram', 'cfg'):
                shutil.copytree(source / directory, stage / directory)
            command = [str(mame), 'nsb6hle', '-rompath', str(root / 'roms'),
                       '-nvram_directory', 'nvram', '-cfg_directory', 'cfg',
                       '-snapshot_directory', 'snap', '-noreadconfig', '-debug',
                       '-debugger', 'none', '-verbose', '-log', '-video', 'none',
                       '-sound', 'none', '-nothrottle', '-autoboot_delay', '0',
                       '-autoboot_script', str(root / 'tools' / script), '-seconds_to_run', str(seconds)]
            with (stage / 'console.log').open('w') as output:
                subprocess.run(command, cwd=stage, stdout=output, stderr=subprocess.STDOUT,
                               check=True, timeout=180)
            check_output((stage / 'console.log').read_text(errors='replace'))
            text = (stage / 'error.log').read_text(errors='replace')
            verify_stage(text, runtime=True, selftest=True)
            traces.append(text)
            from PIL import Image
            with Image.open(stage / 'snap' / frame) as image:
                if image.size != (84, 48) or hashlib.sha256(image.convert('L').tobytes()).hexdigest() != expected:
                    raise ValueError('reviewed code-change frame differs: ' + name)
            source = stage
        check_change(*traces)
        print('8890 code change: PASS physical old/new/confirmation, firmware persistence, independent cold acceptance; research HLE')
        return 0
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f'8890 code change: FAIL: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
