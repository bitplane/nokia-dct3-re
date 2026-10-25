"""Own NSB-1 physical Toolkit display/input/menu and matched Phase-2 control."""
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
from tools.make_sim_card_profile import make_current_profile
from tools.nsb1_pin_roundtrip import check_correct
from tools.nsb1_rom4_compare import check_card_run, check_cold_link, check_frame, run_mame
from tools.nsb1_toolkit_prepare import prepare, profile_metadata
from tools.sim_toolkit_display_check import DISPLAY, RESPONSE, check_display, check_observer_console, wire as display_wire
from tools.sim_toolkit_interactive_check import check_interactive
from tools.nsb1_native_state_check import check_storage

TERMINAL_PROFILE = bytes.fromhex('0f41ff7700')
DISPLAY_FRAME = 'c390ab782b18d054bb4e134ee5bf6b62e99cee154a90b986ec139db956bd0b64'
DISMISSED_FRAME = '8fe18df5185a637968b1e15fe088b069c1978d70688e6cd4ccf0eae1e587eef2'
INTERACTIVE_FRAMES = {
    'inkey':'852ac97234bdff92be255b4a9eb52f51253a833d5aa92205f3535c430958dc9f',
    'input':'a4c7f0629aff370c8230985e5f4ef9c8b933bded0ec8c2cf7d9142bbd1b085a6',
    'input_filled':'0a05a2522d6a8a1e5d6774961cdb33754c7a7584465feaec4f3978828553aa43',
    'interactive_final':'934b3dcd1024275cbee8dc1e87ec35124fb617a917a0684a1e7bc1463ee59068',
}
MENU_FRAMES = {
    'main':'ed5b9d247f48525c5f67b4ca71914bff1802af857d212e4dd5cb0e7e8c9a8b33',
    'fieldtest':'15970bd84bf9c79a6628ee62eafb7fc92baeb0a28b05857bfbb44322dab695d7',
    'last':'28ef1d58dc05c4c81dea1c524172e142721e2475d8691dd57a2683f239c00722',
    'open':'0fe4d32169242d885c7f23d55885bfbf7364b51dec89cce272d1ad00acdfb175',
}
MENU_NEXT='fe5c61cde59b429b92c687c307415e22f9c4c4ae5e94c97f56835d4099096eeb'
ITEM_FRAMES = {
    'prompt':'5098d80a497e9798fde3c918df959f7c7c7edd6e6ed8f6f4c13776358fa2291c',
    'selected':'40644b3eb21317140cf86cd7b90949ad5f69b9128e800c763e047cc40a86adb2',
    'result':MENU_FRAMES['open'],
}
REFRESH_EEPROM='0a79d52b219fad8e29dc3ccbddfaa31b7e2d0da2330850231763f9d28e8884d8'
REFRESH_88='6e0371efd02777b7a8908af941180a03614130cc5016cb514941b393f28b5d9d'


def wire(text, direction):
    return display_wire(text, direction, 'nsb1')


def profile_for(control=False,interactive=False,menu_item=None,select_item=False,refresh=False):
    if refresh and (select_item or menu_item not in (None,1)):
        raise ValueError('REFRESH requires distinct Continue profile')
    if menu_item not in (None,1,2):
        raise ValueError('unknown installed-menu item')
    if select_item and menu_item not in (None,1):
        raise ValueError('follow-up item requires Continue')
    if control and (interactive or menu_item is not None or select_item or refresh):
        raise ValueError('interactive input and Phase-2 control are distinct')
    return 0 if control else 9 if refresh else 7 if select_item else 4 if menu_item is not None else 3 if interactive else 1


def check_trace(text, control=False, interactive=False,menu_item=None,select_item=False,refresh=False):
    profile_for(control,interactive,menu_item,select_item,refresh)
    if select_item or refresh:
        menu_item=1
    if interactive or menu_item is not None:
        check_interactive(text,prefix='nsb1',terminal_profile=TERMINAL_PROFILE,
                          reset_markers=('nsb1_reset_reason:',),menu_item=menu_item,select_item=select_item,refresh=refresh)
        if menu_item is not None:
            actions=('main','fieldtest','last','open')+(('next',) if menu_item==2 else ())+('select',)
            if (re.findall(r'nsb1_toolkit_menu: action=(\w+) down=([01])',text) !=
                    [(action,down) for action in actions for down in ('1','0')]
                    or text.count('nsb1_toolkit_menu:') != len(actions)*2):
                raise ValueError('own installed-menu physical route differs')
    else:
        check_display(text, prefix='nsb1', terminal_profile=TERMINAL_PROFILE,
                      control=control, reset_markers=('nsb1_reset_reason:',))


def check_configuration(source, run, profile):
    path = run / 'cfg/nsb1r4t.cfg'
    tree = ET.parse(path)
    inputs = tree.find('./system/input')
    if inputs is None:
        raise ValueError('missing input configuration')
    ports = inputs.findall("port[@tag=':SATCFG']")
    # MAME omits a default-valued configuration field when saving its CFG.
    if ports or profile != 0:
        if len(ports) != 1 or ports[0].attrib != dict(
                tag=':SATCFG', type='CONFIG', mask='15', defvalue='0', value=str(profile)):
            raise ValueError('external Toolkit input differs')
        inputs.remove(ports[0])
    original = ET.parse(source / 'cfg/nsb1r4t.cfg')
    if ET.tostring(tree.getroot()) != ET.tostring(original.getroot()):
        raise ValueError('unrelated handset configuration changed')
    manifest = json.loads((run / 'input_manifest.json').read_text())
    if manifest.get('external_toolkit_card') != profile_metadata(profile):
        raise ValueError('external card provenance differs')


def check_inputs(source,run,profile,seconds):
    """Own linked cold inputs, full card and physical authentication."""
    check_cold_link(run, source)
    check_configuration(source, run, profile)
    code = json.loads((run / 'exit_status.json').read_text())['returncode']
    text, card = check_card_run(run, code, seconds)
    if card != make_current_profile(True):
        raise ValueError('complete persistent laboratory card changed')
    check_observer_console((run/'mame_output.log').read_text())
    check_correct(text, card, run / 'snap')
    return text,card


def check(source, run, control=False, interactive=False,menu_item=None,select_item=False,refresh=False):
    profile=profile_for(control,interactive,menu_item,select_item,refresh)
    if select_item or refresh:
        menu_item=1
    interactive=interactive or menu_item is not None
    text,card=check_inputs(source,run,profile,92 if select_item or refresh else 90 if menu_item is not None else 84 if interactive else 74)
    check_trace(text, control, interactive,menu_item,select_item,refresh)
    if interactive:
        check_storage(source)
        if refresh:
            check_storage(run,eeprom_sha256=REFRESH_EEPROM)
        else:
            check_storage(run)
        check_frame(run/'snap/nsb1_toolkit_display.png',DISPLAY_FRAME)
        check_frame(run/'snap/nsb1_toolkit_dismissed.png',INTERACTIVE_FRAMES['inkey'])
        for name,expected in INTERACTIVE_FRAMES.items():
            check_frame(run/f'snap/nsb1_toolkit_{name}.png',expected)
        if menu_item is not None:
            for name,expected in MENU_FRAMES.items():
                check_frame(run/f'snap/nsb1_toolkit_menu_{name}.png',expected)
            if menu_item==1 and not select_item and not refresh:
                check_frame(run/'snap/nsb1_toolkit_menu_select.png',MENU_FRAMES['open'])
            elif menu_item==2:
                check_frame(run/'snap/nsb1_toolkit_menu_next.png',MENU_NEXT)
                check_frame(run/'snap/nsb1_toolkit_menu_select.png',MENU_NEXT)
        if select_item:
            for name,expected in ITEM_FRAMES.items():
                check_frame(run/f'snap/nsb1_toolkit_item_{name}.png',expected)
        if refresh:
            check_frame(run/'snap/nsb1_toolkit_refresh_88.png',REFRESH_88)
            check_frame(run/'snap/nsb1_toolkit_refresh_91.png',INTERACTIVE_FRAMES['interactive_final'])
        return
    if not control:
        check_frame(run / 'snap/nsb1_toolkit_display.png', DISPLAY_FRAME)
        # This is the pre-existing corrupted idle content, not a clean-idle claim.
        check_frame(run / 'snap/nsb1_toolkit_dismissed.png', DISMISSED_FRAME)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('run', type=Path)
    mode=parser.add_mutually_exclusive_group()
    mode.add_argument('--control', action='store_true')
    mode.add_argument('--interactive', action='store_true')
    mode.add_argument('--menu-item',type=int,choices=(1,2))
    mode.add_argument('--select-item',action='store_true')
    mode.add_argument('--refresh',action='store_true')
    parser.add_argument('--mame', type=Path, help='prepare and execute a fresh isolated run')
    args = parser.parse_args()
    source, run = args.source.resolve(), args.run.resolve()
    try:
        if args.mame:
            if args.interactive or args.menu_item is not None or args.select_item or args.refresh:
                check_storage(source)
            profile=profile_for(args.control,args.interactive,args.menu_item,args.select_item,args.refresh)
            seconds=92 if args.select_item or args.refresh else 90 if args.menu_item is not None else 84 if args.interactive else 74
            prepare(source, run, profile)
            script = Path(__file__).with_name('nsb1_toolkit_refresh.lua' if args.refresh else 'nsb1_toolkit_select_item.lua' if args.select_item else
                'nsb1_toolkit_menu_exit.lua' if args.menu_item==2 else
                'nsb1_toolkit_menu.lua' if args.menu_item==1 else
                'nsb1_toolkit_interactive.lua' if args.interactive else 'nsb1_toolkit_input.lua').resolve()
            binary = args.mame.resolve()
            (run / 'toolkit_invocation.json').write_text(json.dumps({
                'mame': str(binary), 'mame_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                'script_sha256': hashlib.sha256(script.read_bytes()).hexdigest(),
                'seconds': seconds, 'profile': profile,
            }, indent=2) + '\n')
            run_mame(binary, run / 'inputs', run, script, seconds, True)
        check(source, run, args.control, args.interactive,args.menu_item,args.select_item,args.refresh)
    except (OSError, ValueError, KeyError, ET.ParseError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSB-1 Toolkit FAIL: {error}\n')
    print('NSB-1 Toolkit PASS: ' + ('Phase-2 negative control' if args.control else
          'REFRESH re-profile, success and idle return; next application cycle unproved' if args.refresh else
          'physical follow-up SELECT ITEM and clean list return' if args.select_item else
          f'physical installed-menu item{args.menu_item} envelope and clean list return' if args.menu_item is not None else
          'physical GET INKEY5/GET INPUT42 and unchanged whole storage' if args.interactive else
          'organic DISPLAY TEXT and physical response; native RF/speech unproved'))


if __name__ == '__main__':
    main()
