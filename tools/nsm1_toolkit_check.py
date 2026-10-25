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
from tools.sim_toolkit_display_check import check_display,check_observer_console
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
ITEM_FRAMES = {
    'prompt':'2fd7bf03bc43f6e81353678de00f40c96ef9a7dad56cf47672f8793552fe394d',
    'selected':'1a9cdbfa614372e7d78e887b5cc3be697a8e065e46b8ab0303b0caf13cd0f0ee',
    'result':MENU_FRAMES['open'],
}
REFRESH_EEPROM='5d740b1c59aea38015e374499e3e8c4aeaccd46584c5077a00a0e46d4819e34e'
REFRESH_88='f9d0aea359d55cc7672b88d0a5bf6691351e2c8ddaaafe4e3b27e4f38977d3f8'
TONE_FRAME='0af5d568c3b7d2a7a52dbf7308695172244ec42abf4545d952b44849e34b7de4'


def profile_for(control=False, interactive=False, menu_item=None, select_item=False, refresh=False, tone=None):
    if tone is not None and (tone not in ('cancel','hold','late') or refresh or select_item or menu_item not in (None,1)):
        raise ValueError('PLAY TONE requires distinct Continue profile')
    if refresh and (select_item or menu_item not in (None,1)):
        raise ValueError('REFRESH requires distinct Continue profile')
    if menu_item not in (None,1,2):
        raise ValueError('unknown installed-menu item')
    if select_item and menu_item not in (None,1):
        raise ValueError('follow-up item requires Continue')
    if control and (interactive or menu_item is not None or select_item or refresh or tone is not None):
        raise ValueError('interactive card and Phase-2 control are distinct')
    return 0 if control else 11 if tone is not None else 9 if refresh else 7 if select_item else 4 if menu_item is not None else 3 if interactive else 1


def prepare(seed, run, control=False, interactive=False, menu_item=None, select_item=False, refresh=False, tone=None):
    profile=profile_for(control,interactive,menu_item,select_item,refresh,tone)
    prepare_cold(seed, run)
    path = run / 'cfg/nsm1r4t.cfg'
    tree = ET.parse(path)
    inputs = tree.find('./system/input')
    if inputs.find("port[@tag=':SATCFG']") is not None:
        raise ValueError('seed already overrides external card input')
    ET.SubElement(inputs, 'port', tag=':SATCFG', type='CONFIG',
                  mask='15', defvalue='0', value=str(profile))
    tree.write(path, encoding='utf-8', xml_declaration=True)


def check_trace(text, control=False, interactive=False, menu_item=None, select_item=False, refresh=False, tone=None):
    profile_for(control,interactive,menu_item,select_item,refresh,tone)
    if select_item or refresh or tone is not None:
        menu_item=1
    check_pin(text, True, prefix='nsm1', tx_limit=4096)
    check_phone(text, True, ())
    if interactive or menu_item is not None:
        check_interactive(text, prefix='nsm1', terminal_profile=PROFILE,
                          reset_markers=('nsm1_reset_request:', 'nsm1_restart_request:'),
                          menu_item=menu_item,select_item=select_item,refresh=refresh,tone=tone)
        if menu_item is not None:
            expected=('main','up1','open')+(('next',) if menu_item==2 else ())+('select',)
            if (re.findall(r'nsm1_toolkit_menu: action=(\w+) down=([01])',text) !=
                    [(action,down) for action in expected for down in ('1','0')]
                    or text.count('nsm1_toolkit_menu:') != len(expected)*2):
                raise ValueError('own physical installed-menu route differs')
    else:
        check_display(text, prefix='nsm1', terminal_profile=PROFILE, control=control,
                      reset_markers=('nsm1_reset_request:', 'nsm1_restart_request:'))


def check_configuration(seed, run, control=False, interactive=False, menu_item=None, select_item=False, refresh=False, tone=None, *, machine='nsm1r4t'):
    profile=profile_for(control,interactive,menu_item,select_item,refresh,tone)
    expected = signature(seed / 'cfg/nsm1r4t.cfg')
    actual = signature(run / f'cfg/{machine}.cfg', machine=machine)
    card = ('port', sorted(dict(tag=':SATCFG', type='CONFIG', mask='15',
                               defvalue='0', value=str(profile)).items()), '')
    # MAME omits the explicitly selected zero/default after saving CFG.
    if actual != expected + [card] and not (control and actual == expected):
        raise ValueError('unrelated external inputs changed')


def check_inputs(seed, run, control=False, interactive=False, menu_item=None, select_item=False, refresh=False, tone=None):
    """Whole cold inputs and outputs shared by ordinary and restoration gates."""
    profile_for(control,interactive,menu_item,select_item,refresh,tone)
    files = check_storage(seed)
    if refresh:
        check_storage(run,eeprom_sha256=REFRESH_EEPROM)
    else:
        check_storage(run)
    metadata = json.loads((run / 'cold_input.json').read_text())
    if metadata != dict(source=str(seed.resolve()), source_sha256={
            name: hashlib.sha256(data).hexdigest() for name, data in files.items()},
            copied_directories=['nvram', 'cfg'], ram_or_save_state_copied=False):
        raise ValueError('cold provenance differs')
    for name in ('eeprom', 'sim_card'):
        if (run / f'initial_{name}.bin').read_bytes() != files[name]:
            raise ValueError('initial complete storage differs')
    check_configuration(seed, run, control, interactive, menu_item, select_item,refresh,tone)
    if json.loads((run / 'exit_status.json').read_text()) != dict(returncode=0):
        raise ValueError('MAME did not complete')
    check_observer_console((run/'console.log').read_text())
    return files


def check_tone_native(text):
    rows=re.findall(r'nsm1_toolkit_tone_native: phase=(\w+) pc=([0-9a-f]{4}) idle=([01]) stop=([01]) t=([0-9.]+) observer_writes=0',text)
    if (len(rows)!=3 or text.count('nsm1_toolkit_tone_native:')!=3
            or [(r[0],r[3],r[4]) for r in rows] != [(p,'0',f'{t:.9f}') for p,t in (('before',85),('active',87),('after',91))]):
        raise ValueError('bounded native tone execution observation differs')
    return rows


def check(seed, run, control=False, interactive=False, menu_item=None, select_item=False, refresh=False, tone=None):
    check_inputs(seed,run,control,interactive,menu_item,select_item,refresh,tone)
    if select_item or refresh or tone is not None:
        menu_item=1
    interactive=interactive or menu_item is not None
    text=(run / 'error.log').read_text()
    check_trace(text, control, interactive, menu_item, select_item,refresh,tone)
    frame_check(run / 'snap/nsm1_toolkit_before.png', IDLE if control else PROMPT)
    frame_check(run / 'snap/nsm1_toolkit_after.png',
                INTERACTIVE_FRAMES['inkey'] if interactive else MENU if control else IDLE)
    if interactive:
        for label,expected in INTERACTIVE_FRAMES.items():
            frame_check(run / f'snap/nsm1_toolkit_{label}.png',expected)
    if menu_item is not None:
        for label,expected in MENU_FRAMES.items():
            frame_check(run/f'snap/nsm1_toolkit_menu_{label}.png',expected)
        if not select_item and not refresh and tone is None:
            frame_check(run/'snap/nsm1_toolkit_menu_select.png',MENU_WAIT)
        if menu_item==2:
            frame_check(run/'snap/nsm1_toolkit_menu_next.png',MENU_NEXT)
    if select_item:
        for label,expected in ITEM_FRAMES.items():
            frame_check(run/f'snap/nsm1_toolkit_item_{label}.png',expected)
    if refresh:
        frame_check(run/'snap/nsm1_toolkit_refresh_88.png',REFRESH_88)
        frame_check(run/'snap/nsm1_toolkit_refresh_91.png',IDLE)
    if tone is not None:
        frame_check(run/'snap/nsm1_toolkit_tone_before.png',MENU_FRAMES['open'])
        frame_check(run/'snap/nsm1_toolkit_tone_active.png',TONE_FRAME)
        frame_check(run/'snap/nsm1_toolkit_tone_after.png',MENU_FRAMES['open'] if tone=='cancel' else TONE_FRAME)
        check_tone_native(text)
        if tone=='late':
            frame_check(run/'snap/nsm1_toolkit_tone_late.png',MENU_FRAMES['open'])
            from tools.nsm1_toolkit_tone_timing_check import check_trace as check_timing
            check_timing(text)


def execute(binary, seed, run, control=False, interactive=False, menu_item=None, select_item=False, refresh=False, tone=None):
    prepare(seed, run, control, interactive, menu_item, select_item,refresh,tone)
    script = Path(__file__).with_name('nsm1_toolkit_tone_late.lua' if tone=='late' else 'nsm1_toolkit_tone_hold.lua' if tone=='hold' else 'nsm1_toolkit_tone.lua' if tone=='cancel' else 'nsm1_toolkit_refresh.lua' if refresh else 'nsm1_toolkit_select_item.lua' if select_item else
        'nsm1_toolkit_menu_exit.lua' if menu_item==2 else
        'nsm1_toolkit_menu.lua' if menu_item==1 else
        'nsm1_toolkit_interactive.lua' if interactive else 'nsm1_toolkit_input.lua').resolve()
    command = [str(binary), 'nsm1r4t', '-rompath', str(Path(__file__).resolve().parents[1] / 'roms'),
               '-noreadconfig', '-video', 'none', '-sound', 'none', '-nothrottle',
               '-joystickprovider', 'none', '-midiprovider', 'none',
               '-debug', '-debugger', 'none', '-log', '-verbose', '-seconds_to_run',
               '94' if tone=='late' else '92' if select_item or refresh or tone is not None else '90' if menu_item is not None else '84' if interactive else '74',
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
    mode.add_argument('--select-item',action='store_true')
    mode.add_argument('--refresh',action='store_true')
    mode.add_argument('--tone',choices=('cancel','hold','late'))
    parser.add_argument('--mame', type=Path)
    args = parser.parse_args()
    try:
        seed, run = args.seed.resolve(), args.run.resolve()
        if args.mame:
            execute(args.mame.resolve(), seed, run, args.control, args.interactive, args.menu_item,args.select_item,args.refresh,args.tone)
        check(seed, run, args.control, args.interactive, args.menu_item,args.select_item,args.refresh,args.tone)
    except (OSError, ValueError, KeyError, ET.ParseError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSM-1 Toolkit FAIL: {error}\n')
    print('NSM-1 Toolkit PASS: ' + ('matched Phase-2 control' if args.control else
          'PLAY TONE organic late completion on current Timer1 cadence; audio unproved' if args.tone=='late' else
          f'PLAY TONE {args.tone}; completion beyond this window/native audio unproved' if args.tone is not None else
          'REFRESH re-profile, success and idle return; next application cycle unproved' if args.refresh else
          'physical follow-up SELECT ITEM; native RF/speech unproved' if args.select_item else
          f'physical installed-menu item{args.menu_item} envelope; final settlement unproved' if args.menu_item is not None else
          'physical DISPLAY TEXT, GET INKEY 5 and GET INPUT 42' if args.interactive else
          'card-originated DISPLAY TEXT, physical dismissal and successful response'))


if __name__ == '__main__':
    main()
