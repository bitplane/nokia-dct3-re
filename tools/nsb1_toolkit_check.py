"""Qualify own NSB-1 DISPLAY TEXT or its matched Phase-2 card control."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_current_profile
from tools.nsb1_pin_roundtrip import check_correct
from tools.nsb1_rom4_compare import check_card_run, check_cold_link, check_frame, run_mame
from tools.nsb1_toolkit_prepare import prepare, profile_metadata
from tools.sim_toolkit_display_check import DISPLAY, RESPONSE, check_display, wire as display_wire
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


def wire(text, direction):
    return display_wire(text, direction, 'nsb1')


def check_trace(text, control=False, interactive=False):
    if control and interactive:
        raise ValueError('interactive input and Phase-2 control are distinct')
    if interactive:
        check_interactive(text,prefix='nsb1',terminal_profile=TERMINAL_PROFILE,
                          reset_markers=('nsb1_reset_reason:',))
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


def check(source, run, control=False, interactive=False):
    if control and interactive:
        raise ValueError('interactive input and Phase-2 control are distinct')
    check_cold_link(run, source)
    check_configuration(source, run, 0 if control else 3 if interactive else 1)
    code = json.loads((run / 'exit_status.json').read_text())['returncode']
    text, card = check_card_run(run, code, 84 if interactive else 74)
    if card != make_current_profile(True):
        raise ValueError('complete persistent laboratory card changed')
    check_correct(text, card, run / 'snap')
    check_trace(text, control, interactive)
    if interactive:
        check_storage(source)
        check_storage(run)
        check_frame(run/'snap/nsb1_toolkit_display.png',DISPLAY_FRAME)
        check_frame(run/'snap/nsb1_toolkit_dismissed.png',INTERACTIVE_FRAMES['inkey'])
        for name,expected in INTERACTIVE_FRAMES.items():
            check_frame(run/f'snap/nsb1_toolkit_{name}.png',expected)
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
    parser.add_argument('--mame', type=Path, help='prepare and execute a fresh isolated run')
    args = parser.parse_args()
    source, run = args.source.resolve(), args.run.resolve()
    try:
        if args.mame:
            if args.interactive:
                check_storage(source)
            profile=0 if args.control else 3 if args.interactive else 1
            seconds=84 if args.interactive else 74
            prepare(source, run, profile)
            script = Path(__file__).with_name('nsb1_toolkit_interactive.lua' if args.interactive else 'nsb1_toolkit_input.lua').resolve()
            binary = args.mame.resolve()
            (run / 'toolkit_invocation.json').write_text(json.dumps({
                'mame': str(binary), 'mame_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                'script_sha256': hashlib.sha256(script.read_bytes()).hexdigest(),
                'seconds': seconds, 'profile': profile,
            }, indent=2) + '\n')
            run_mame(binary, run / 'inputs', run, script, seconds, True)
        check(source, run, args.control, args.interactive)
    except (OSError, ValueError, KeyError, ET.ParseError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSB-1 Toolkit FAIL: {error}\n')
    print('NSB-1 Toolkit PASS: ' + ('Phase-2 negative control' if args.control else
          'physical GET INKEY5/GET INPUT42 and unchanged whole storage' if args.interactive else
          'organic DISPLAY TEXT and physical response; native RF/speech unproved'))


if __name__ == '__main__':
    main()
