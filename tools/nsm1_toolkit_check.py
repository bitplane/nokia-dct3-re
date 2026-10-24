"""Own NSM-1 physical Toolkit display/input/menu and matched Phase-2 control."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsm1_native_state_check import prepare as prepare_cold, check_storage, signature
from tools.nsm1_security_check import check_trace as check_phone, MENU
from tools.nsk3_pin_check import check_trace as check_pin
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_toolkit_display_check import check_display
from tools.sim_toolkit_interactive_check import check_interactive

PROFILE = bytes.fromhex('0b41ff7700')
PROMPT = '0b61ca941a012e9feffe2d13b834c720031b71295220fc32139decd034172b2c'
IDLE = '1d8403273fd916cd003bb18471343ed23d6a405a3b31ef2a165c5c0e276f3aa1'
INTERACTIVE_FRAMES = {
    'inkey': 'e94ea9d879033d48822bb76a8f080f95e12456c3b897a1717be10fb4ddbbb77f',
    'input': '2b1546243a290d99ea42d5e70e173315b3d620903595782d2dc5f6fec483836c',
    'input_filled': '169b9f1bb144e9cd4e061a74980b1479956c9431c17482149b7701300b6e83b0',
    'interactive_final': IDLE,
}
MENU_FRAMES = {
    'main': MENU,
    'up1':'563fc6bf0d9db44646d477ab823dc24f62d36ec66aa4c9d024b043f1a772612b',
    'open':'a5a9b402a8ca6111d5b4fd24ab253a6bc98f73f8119d11bc3bca646915e66b8f',
}
MENU_WAIT='8c0b4212dca1d75c790bceb26d1380e55f3e3359bfa2fbb47be9dc7ad5931ce1'
MENU_NEXT='18301608793594717b299ae316605cb7c4ee1ee90b787027de1b33b940f1c4ae'


def profile_for(control=False, interactive=False, menu_item=None):
    if menu_item not in (None,1,2):
        raise ValueError('unknown installed-menu item')
    if control and (interactive or menu_item is not None):
        raise ValueError('interactive card and Phase-2 control are distinct')
    return 0 if control else 4 if menu_item is not None else 3 if interactive else 1


def prepare(seed, run, control=False, interactive=False, menu_item=None):
    profile=profile_for(control,interactive,menu_item)
    prepare_cold(seed, run)
    path = run / 'cfg/nsm1r4t.cfg'
    tree = ET.parse(path)
    inputs = tree.find('./system/input')
    if inputs.find("port[@tag=':SATCFG']") is not None:
        raise ValueError('seed already overrides external card input')
    ET.SubElement(inputs, 'port', tag=':SATCFG', type='CONFIG',
                  mask='15', defvalue='0', value=str(profile))
    tree.write(path, encoding='utf-8', xml_declaration=True)


def check_trace(text, control=False, interactive=False, menu_item=None):
    profile_for(control,interactive,menu_item)
    check_pin(text, True, prefix='nsm1', tx_limit=4096)
    check_phone(text, True, ())
    if interactive or menu_item is not None:
        check_interactive(text, prefix='nsm1', terminal_profile=PROFILE,
                          reset_markers=('nsm1_reset_request:', 'nsm1_restart_request:'),
                          menu_item=menu_item)
        if menu_item is not None:
            expected=('main','up1','open')+(('next',) if menu_item==2 else ())+('select',)
            if (re.findall(r'nsm1_toolkit_menu: action=(\w+) down=([01])',text) !=
                    [(action,down) for action in expected for down in ('1','0')]
                    or text.count('nsm1_toolkit_menu:') != len(expected)*2):
                raise ValueError('own physical installed-menu route differs')
    else:
        check_display(text, prefix='nsm1', terminal_profile=PROFILE, control=control,
                      reset_markers=('nsm1_reset_request:', 'nsm1_restart_request:'))


def check_configuration(seed, run, control=False, interactive=False, menu_item=None):
    profile=profile_for(control,interactive,menu_item)
    expected = signature(seed / 'cfg/nsm1r4t.cfg')
    actual = signature(run / 'cfg/nsm1r4t.cfg')
    card = ('port', sorted(dict(tag=':SATCFG', type='CONFIG', mask='15',
                               defvalue='0', value=str(profile)).items()), '')
    # MAME omits the explicitly selected zero/default after saving CFG.
    if actual != expected + [card] and not (control and actual == expected):
        raise ValueError('unrelated external inputs changed')


def check(seed, run, control=False, interactive=False, menu_item=None):
    profile_for(control,interactive,menu_item)
    interactive=interactive or menu_item is not None
    files = check_storage(seed)
    check_storage(run)
    metadata = json.loads((run / 'cold_input.json').read_text())
    if metadata != dict(source=str(seed.resolve()), source_sha256={
            name: hashlib.sha256(data).hexdigest() for name, data in files.items()},
            copied_directories=['nvram', 'cfg'], ram_or_save_state_copied=False):
        raise ValueError('cold provenance differs')
    for name in ('eeprom', 'sim_card'):
        if (run / f'initial_{name}.bin').read_bytes() != files[name]:
            raise ValueError('initial complete storage differs')
    check_configuration(seed, run, control, interactive, menu_item)
    if json.loads((run / 'exit_status.json').read_text()) != dict(returncode=0):
        raise ValueError('MAME did not complete')
    check_trace((run / 'error.log').read_text(), control, interactive, menu_item)
    frame_check(run / 'snap/nsm1_toolkit_before.png', IDLE if control else PROMPT)
    frame_check(run / 'snap/nsm1_toolkit_after.png',
                INTERACTIVE_FRAMES['inkey'] if interactive else MENU if control else IDLE)
    if interactive:
        for label,expected in INTERACTIVE_FRAMES.items():
            frame_check(run / f'snap/nsm1_toolkit_{label}.png',expected)
    if menu_item is not None:
        for label,expected in MENU_FRAMES.items():
            frame_check(run/f'snap/nsm1_toolkit_menu_{label}.png',expected)
        frame_check(run/'snap/nsm1_toolkit_menu_select.png',MENU_WAIT)
        if menu_item==2:
            frame_check(run/'snap/nsm1_toolkit_menu_next.png',MENU_NEXT)


def execute(binary, seed, run, control=False, interactive=False, menu_item=None):
    prepare(seed, run, control, interactive, menu_item)
    script = Path(__file__).with_name('nsm1_toolkit_menu_exit.lua' if menu_item==2 else
        'nsm1_toolkit_menu.lua' if menu_item==1 else
        'nsm1_toolkit_interactive.lua' if interactive else 'nsm1_toolkit_input.lua').resolve()
    command = [str(binary), 'nsm1r4t', '-rompath', str(Path(__file__).resolve().parents[1] / 'roms'),
               '-noreadconfig', '-video', 'none', '-sound', 'none', '-nothrottle',
               '-joystickprovider', 'none', '-midiprovider', 'none',
               '-debug', '-debugger', 'none', '-log', '-verbose', '-seconds_to_run',
               '90' if menu_item is not None else '84' if interactive else '74',
               '-autoboot_delay', '0', '-autoboot_script', str(script)]
    (run / 'invocation.json').write_text(json.dumps(dict(command=command,
        mame_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
        script_sha256=hashlib.sha256(script.read_bytes()).hexdigest()), indent=2)+'\n')
    with (run / 'console.log').open('w') as output:
        result = subprocess.run(command, cwd=run, stdout=output,
                                stderr=subprocess.STDOUT, timeout=180)
    (run / 'exit_status.json').write_text(json.dumps(dict(returncode=result.returncode))+'\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('seed', type=Path)
    parser.add_argument('run', type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--control', action='store_true')
    mode.add_argument('--interactive', action='store_true')
    mode.add_argument('--menu-item',type=int,choices=(1,2))
    parser.add_argument('--mame', type=Path)
    args = parser.parse_args()
    try:
        seed, run = args.seed.resolve(), args.run.resolve()
        if args.mame:
            execute(args.mame.resolve(), seed, run, args.control, args.interactive, args.menu_item)
        check(seed, run, args.control, args.interactive, args.menu_item)
    except (OSError, ValueError, KeyError, ET.ParseError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSM-1 Toolkit FAIL: {error}\n')
    print('NSM-1 Toolkit PASS: ' + ('matched Phase-2 control' if args.control else
          f'physical installed-menu item{args.menu_item} envelope; final settlement unproved' if args.menu_item is not None else
          'physical DISPLAY TEXT, GET INKEY 5 and GET INPUT 42' if args.interactive else
          'card-originated DISPLAY TEXT, physical dismissal and successful response'))


if __name__ == '__main__':
    main()
