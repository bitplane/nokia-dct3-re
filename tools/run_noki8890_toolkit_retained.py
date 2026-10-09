"""Fresh own-PMM clock seeding and retained NSB-6 DISPLAY TEXT acceptance."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.dct3_toolkit_check import verify_display_text, verify_interactive, verify_menu
from tools.sim_toolkit_trace_check import require_in_order
from tools.noki8890_registration_check import verify as verify_registration

FRAMES = {
    '8890_toolkit_display.png': '6a0bd20bfe0a7f57ae82a570eceb3a0f29fdc8a9a1e047fb294e85d61e610f6f',
    '8890_toolkit_after_dismiss.png': '8187cbe68f4b7fe0a15cf10c16b742b3236f3240af3f79556c319d5ed337f2ee',
}
INTERACTIVE_FRAMES = {
    '8890_toolkit_interactive_display.png': '6a0bd20bfe0a7f57ae82a570eceb3a0f29fdc8a9a1e047fb294e85d61e610f6f',
    '8890_toolkit_interactive_inkey.png': '7b31b9248d0fe79ded4ff5f6ea40136ee11f2257b0969513d878c7bfee71ff17',
    '8890_toolkit_interactive_input.png': '2adee3ccafa0c386614b674ff9ec35685e57223e0aee691756a8eb04f7d62191',
    '8890_toolkit_interactive_entered.png': '16ed0527e2ff95760f4c4e68897f185357fcdcc66d54ad565284e4b02f6f4b0d',
    '8890_toolkit_interactive_idle.png': '8187cbe68f4b7fe0a15cf10c16b742b3236f3240af3f79556c319d5ed337f2ee',
}
MENU_FRAMES = {
    '8890_toolkit_menu_entry.png': 'f216cca9df295bfeb68ab9592ecfe9206834ef523947da3f8cda2f45b52ab2a3',
    '8890_toolkit_menu_items.png': 'd1852f346e850b38c93750d646742dd13d55223d4ab09a76a809018cce9468eb',
    '8890_toolkit_menu_result.png': 'd1852f346e850b38c93750d646742dd13d55223d4ab09a76a809018cce9468eb',
    '8890_toolkit_menu_idle.png': '8187cbe68f4b7fe0a15cf10c16b742b3236f3240af3f79556c319d5ed337f2ee',
}


def verify_interactive_protocol(text, *, menu=False, sms=False):
    if sms and not menu:
        raise ValueError('Toolkit SMS requires physical card-menu selection')
    if menu:
        verify_menu(text, '8890', selection_status='9124' if sms else '9000')
    else:
        verify_interactive(text, '8890')
    if sms:
        from tools.sim_toolkit_sms_trace_check import verify as verify_sms
        verify_sms(text, cp=0x39, message_reference=1)
    actions = re.findall(r'8890_toolkit_interactive: action=(\w+)\b', text)
    expected = ['dismiss', 'inkey_5', 'inkey_confirm', 'input_4', 'input_2', 'confirm']
    if menu:
        expected.extend(['menu', 'menu_last', 'menu_open', 'menu_select', 'menu_exit'])
    if actions != expected:
        raise ValueError('NSB-6 Toolkit physical reply sequence differs')
    # The NSB-6 editor requires OK; other products submit INKEY immediately.
    require_in_order(text.replace('[:sim_card] ', ''), [
        '8890_toolkit_interactive: action=inkey_5',
        '8890_toolkit_interactive: action=inkey_confirm',
        'terminal-response data=8103022200020282810301000d020435',
        '8890_toolkit_interactive: action=input_4',
    ])


def verify(run, *, interactive=False, menu=False, sms=False):
    from PIL import Image
    text = (run / 'error.log').read_text(errors='replace')
    if sms and not menu:
        raise ValueError('Toolkit SMS requires the card-menu scenario')
    if menu and not interactive:
        raise ValueError('card-menu acceptance requires interactive prerequisites')
    if interactive:
        verify_interactive_protocol(text, menu=menu, sms=sms)
    else:
        verify_display_text(text, '8890')
    verify_registration(text, preserved_location=True)
    card = (run / 'nvram/nsb6hle/sim_card').read_bytes()
    if len(card) < 1611 or card[1604:1609] != bytes.fromhex('00f1100001') or card[1610] != 0:
        raise ValueError('retained SIM location differs')
    expected_frames = dict(INTERACTIVE_FRAMES if interactive else FRAMES)
    if menu:
        expected_frames.update(MENU_FRAMES)
    if sms:
        expected_frames['8890_toolkit_network_result.png'] = MENU_FRAMES['8890_toolkit_menu_items.png']
        # The network wait crosses the retained clock's next minute boundary.
        expected_frames['8890_toolkit_menu_idle.png'] = 'f3785458b0b7e3fb6017a0b523b1fbad2c5a5a32c6d2ace53bba4ee3ba8fb756'
    for name, digest in expected_frames.items():
        with Image.open(run / 'snap' / name) as frame:
            if frame.size != (84, 48) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != digest:
                raise ValueError('unexpected retained Toolkit frame: ' + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--interactive', action='store_true',
                        help='require own GET INKEY/GET INPUT physical replies')
    parser.add_argument('--menu', action='store_true',
                        help='extend interactive replies with physical card-menu selection')
    parser.add_argument('--sms', action='store_true',
                        help='require card-requested SMS through laboratory GSM transport')
    args = parser.parse_args()
    if args.sms:
        args.menu = True
    if args.menu:
        args.interactive = True
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    try:
        run.mkdir(parents=True, exist_ok=False)
        seed = run / 'seed'
        command = [sys.executable, str(root / 'tools/run_noki8xxx_supplementary.py'),
                   '8890', str(seed), '--service', 'toolkit-busy']
        if args.mame:
            command += ['--mame', str(args.mame.resolve())]
        subprocess.run(command, check=True)
        warm = run / 'retained'
        warm.mkdir()
        for name in ('nvram', 'cfg'):
            shutil.copytree(seed / name, warm / name)
        (warm / 'snap').mkdir()
        if args.interactive:
            config_path = warm / 'cfg/nsb6hle.cfg'
            config = ET.parse(config_path)
            port = config.find("./system/input/port[@tag=':SATCFG']")
            if port is None:
                raise ValueError('retained seed lacks the card-profile configuration')
            port.set('value', '5' if args.sms else '4' if args.menu else '3')
            config.write(config_path, encoding='utf-8', xml_declaration=True)
        command = json.loads((seed / 'acceptance.json').read_text())['command']
        script = 'noki8890_toolkit_interactive_input.lua' if args.interactive else 'noki8890_toolkit_retained_input.lua'
        if args.menu:
            script = 'noki8890_toolkit_menu_input.lua'
        if args.sms:
            script = 'noki8890_toolkit_sms_input.lua'
        command[command.index('-autoboot_script') + 1] = str(root / 'tools' / script)
        command[command.index('-seconds_to_run') + 1] = '140' if args.sms else '120' if args.menu else '105' if args.interactive else '80'
        with (warm / 'console.log').open('w') as console:
            subprocess.run(command, cwd=warm, stdout=console, stderr=subprocess.STDOUT,
                           check=True, timeout=180)
        verify(warm, interactive=args.interactive, menu=args.menu, sms=args.sms)
        (run / 'acceptance.json').write_text(json.dumps({
            'product': '8890', 'result': 'pass', 'seed': 'fresh physical clock/date; own acquired PMM',
            'scope': ('retained-clock interactive Toolkit with laboratory SEND SHORT MESSAGE' if args.sms else
                      'retained-clock DISPLAY TEXT/GET INKEY/GET INPUT/SET UP MENU' if args.menu else
                      'retained-clock DISPLAY TEXT/GET INKEY/GET INPUT' if args.interactive else
                      'retained-clock DISPLAY TEXT') + ' and laboratory registration; research HLE, not native speech',
            'command': command,
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'8890 retained Toolkit FAIL: {error}\n')
    print('8890 fresh-seed/retained Toolkit PASS')


if __name__ == '__main__':
    main()
