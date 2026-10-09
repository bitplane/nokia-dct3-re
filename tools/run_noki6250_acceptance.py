#!/usr/bin/env python3
"""Fresh, isolated NHM-3 research-profile physical acceptance runs.

Uses the explicitly derived initial-record PMM comparison, not factory data.
The normal noki6250 machine and the acquired PMM are left unchanged.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

try:
    from tools.noki6250_pmm_check import initial_record_fixture
except ModuleNotFoundError:
    from noki6250_pmm_check import initial_record_fixture


def check_divert_lifecycle(text, frames):
    from PIL import Image
    from tools.radio_call_divert_lifecycle_trace_check import EVENTS
    from tools.radio_call_lifecycle_common import require_ordered
    import re
    if '[LUA ERROR]' in text:
        raise ValueError('NHM-3 forwarding physical fixture failed')
    ordered = []
    for index in range(4):
        ordered.append((f'physical transaction {index + 1}', re.compile(
            rf'6250_divert_lifecycle_physical: transaction={index + 1}\b')))
        ordered.extend((f'forwarding protocol {index * 2 + offset + 1}', pattern)
                       for offset, pattern in enumerate(EVENTS[index * 2:index * 2 + 2]))
    require_ordered(text, tuple(ordered), '6250 forwarding lifecycle')
    expected = (
        'e9e4c057d769f66a48893d561b8edc1de9213a44c3756f047db4bf3d14653295',
        '466a5a0241eb09e162c00227e8737eaddc5717251ec2663bf9a97b3c8c8c58d9',
        '45ca2e2d94aa2af50a1f008baef60fffecbb49ba304c3ff8e68818a91b88bf3e',
        '0218f879565b696f4768f55215c45166b725b1897d0d78715da54278c74244b4',
        '7c541cfc93c2da8e854421941df0ac81755b73f47c3af98f2f6a40efac181b0b',
    )
    for phase, digest in zip(('1', '2', '3', '4', 'idle'), expected):
        with Image.open(frames / f'6250_divert_lifecycle_{phase}.png') as frame:
            if frame.size != (96, 60) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != digest:
                raise ValueError('missing reviewed NHM-3 forwarding frame: ' + phase)


def check_toolkit_protocol(text):
    from tools.dct3_toolkit_check import verify_display_text
    verify_display_text(text, '6250')


def check_toolkit(text, frames):
    from PIL import Image
    check_toolkit_protocol(text)
    expected = {
        'display': '1c27b5e561a2183e01fffc11e71a78f5df35e342fde2c01d6df3fffc26c4199b',
        'after_dismiss': '7c541cfc93c2da8e854421941df0ac81755b73f47c3af98f2f6a40efac181b0b',
    }
    for phase, digest in expected.items():
        with Image.open(frames / f'6250_toolkit_{phase}.png') as frame:
            if frame.size != (96, 60) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != digest:
                raise ValueError('missing reviewed NHM-3 Toolkit frame: ' + phase)


def check_interactive_toolkit(text, frames, *, menu=False, sms=False, call=False, decline=False):
    import re
    from tools.dct3_toolkit_check import verify_interactive, verify_menu
    from tools.run_noki6250_calendar import check_frame
    if (sms and call) or (decline and not call):
        raise ValueError('inconsistent Toolkit consent scenario')
    if (sms or call) and not menu:
        raise ValueError('Toolkit network requests require the physical card-menu sequence')
    if menu:
        verify_menu(text, '6250', selection_status='911c' if call else '9124' if sms else '9000')
    else:
        verify_interactive(text, '6250')
    if sms:
        from tools.sim_toolkit_sms_trace_check import verify as verify_sms
        verify_sms(text, cp=0x39, message_reference=1)
    if call:
        from tools.sim_toolkit_call_trace_check import verify as verify_call, verify_decline
        if decline:
            verify_decline(text, '6250')
        else:
            verify_call(text)
            from tools.sim_toolkit_trace_check import require_in_order
            require_in_order(text, [
                '6250_toolkit_interactive: action=call_accept',
                'GSM outgoing request id=1 digits=5551234',
                '6250_toolkit_interactive: action=menu_exit',
                'GSM service uplink sapi=0 pd=03 message=25',
            ])
    actions = ['dismiss', 'inkey_5', 'input_4', 'input_2', 'confirm']
    if menu:
        actions.extend(['menu', 'menu_last', 'menu_open', 'menu_select', 'menu_exit'])
        if call:
            actions.insert(-1, 'call_decline' if decline else 'call_accept')
    if re.findall(r'6250_toolkit_interactive: action=(\w+)\b', text) != actions:
        raise ValueError('NHM-3 interactive Toolkit physical sequence differs')
    expected = {
        'display': '1c27b5e561a2183e01fffc11e71a78f5df35e342fde2c01d6df3fffc26c4199b',
        'inkey': '60ece9b44b016ae749bc6b3498ded166628a81f5b983a2c7dd06768e93782fbf',
        'input': '91bdd069d33a7cd535d1f472ed04f006a8615b8de3158b1c0d574ca5844063c6',
        'entered': 'c0fe70b64f1b25657e22bc7efb786b17e4831a48230bf3eced61dc0a21616b2a',
        'idle': '7c541cfc93c2da8e854421941df0ac81755b73f47c3af98f2f6a40efac181b0b',
    }
    for phase, digest in expected.items():
        check_frame(frames / f'6250_toolkit_interactive_{phase}.png', digest)
    if menu:
        menu_frames = {
            'entry': '7dbd340f978d32f74d91fbd54ffbb1b3f07e5c5a12375aea0b74ca78e2a192be',
            'items': 'a9d5d7e7caa5f074af70d12c5b25ec7dfcad057e7bebc49a91f181cf0a1cfcea',
            'result': 'a9d5d7e7caa5f074af70d12c5b25ec7dfcad057e7bebc49a91f181cf0a1cfcea',
            'idle': expected['idle'],
        }
        if call:
            menu_frames['result'] = '66063a4df7efdacdfce3e8d90995423c48d856dfc50e1a649e008d42572cc35a'
        for phase, digest in menu_frames.items():
            check_frame(frames / f'6250_toolkit_menu_{phase}.png', digest)
        if sms:
            check_frame(frames / '6250_toolkit_network_result.png', menu_frames['items'])
        if call:
            check_frame(frames / '6250_toolkit_network_result.png',
                        menu_frames['items'] if decline else
                        'ee9fb86506b4999c0902c9e8f29a9737e63bd3ffeacf47de133bfc41a54c7001')


def check_ussd(text, frames):
    check_supplementary(text, frames, 'ussd')


def check_supplementary(text, frames, service,
                        idle='7c541cfc93c2da8e854421941df0ac81755b73f47c3af98f2f6a40efac181b0b'):
    from tools import radio_ussd_trace_check
    from tools import radio_call_divert_trace_check
    from tools.noki8210_supplementary_check import verify_transaction
    keys, protocol, result = {
        'ussd': (('Keypad *', 'Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad #', 'Send'),
                 radio_ussd_trace_check,
                 '7282a48b0f972545caac4db1a02189d25775dd13e02b11857cba98cfd58b3112'),
        'divert': (('Keypad *', 'Keypad #', 'Keypad 2', 'Keypad 1', 'Keypad #', 'Send'),
                   radio_call_divert_trace_check,
                   '0218f879565b696f4768f55215c45166b725b1897d0d78715da54278c74244b4'),
    }[service]
    verify_transaction(text, frames, service, keys, protocol, result,
                       idle,
                       product='6250', geometry=(96, 60))


def prerequisite_trace(text, scenario):
    """A restored event cannot establish the prerequisites of the saved state."""
    if scenario not in ('idle-state', 'call-state', 'sms-state', 'divert-state'):
        return text
    before, marker, _ = text.partition('6250_state: event=saved')
    if not marker:
        raise ValueError('restoration prerequisites lack a save boundary')
    return before


def apply_coherent_config(path, fixture):
    """Overlay lab carrier/host inputs without losing a physical Reply target."""
    source = ET.parse(fixture).getroot().find('system')
    incoming = source.find('input') if source is not None else None
    if source is None or source.get('name') != 'nhm3hle' or incoming is None:
        raise ValueError('coherent NHM-3 fixture lacks inputs')
    if path.exists():
        config = ET.parse(path).getroot()
        system = config.find('system')
        inputs = system.find('input') if system is not None else None
        if system is None or system.get('name') != 'nhm3hle' or inputs is None:
            raise ValueError('existing NHM-3 configuration lacks inputs')
    else:
        config = ET.Element('mameconfig', version='10')
        inputs = ET.SubElement(ET.SubElement(config, 'system', name='nhm3hle'), 'input')
    for port in incoming:
        for previous in list(inputs):
            if previous.get('tag') == port.get('tag') and previous.get('mask') == port.get('mask'):
                inputs.remove(previous)
        inputs.append(ET.Element(port.tag, port.attrib))
    ET.ElementTree(config).write(path, encoding='utf-8', xml_declaration=True)


def prepare_run(run, root):
    """Own acquired PMM comparison; audit-only shared ROMs are not NHM-3 masks."""
    from tools.noki6250_accessory_contract import verify as verify_accessory
    accessory = verify_accessory((root / 'roms/noki6250/6250-503mcuppmc.fls').read_bytes())
    source = root / 'roms/noki6250/6250 virgin eeprom 005fa000.fls'
    fixture = initial_record_fixture(source.read_bytes())
    run.mkdir(parents=True, exist_ok=False)
    local_roms = run / 'roms/noki6250'
    local_roms.mkdir(parents=True)
    (local_roms / source.name).write_bytes(fixture)
    audit_members = []
    for name in ('dsp_prom', 'dsp_drom', 'dsp_pdrom'):
        audit_source = root / 'roms/noki3210' / name
        payload = audit_source.read_bytes()
        (local_roms / name).write_bytes(payload)
        audit_members.append({'name': name, 'source': str(audit_source),
                              'sha256': hashlib.sha256(payload).hexdigest(),
                              'native_6250_evidence': False})
    (run / 'cfg').mkdir()
    (run / 'nvram').mkdir()
    return accessory, audit_members


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_directory", type=Path,
                        help="new directory; existing directories are refused")
    parser.add_argument("--mame", type=Path)
    parser.add_argument("--scenario", choices=("calculator", "ussd", "divert", "divert-lifecycle", "toolkit", "toolkit-interactive", "toolkit-menu", "toolkit-sms", "toolkit-call", "toolkit-call-decline", "incoming-call", "outgoing-call",
                                              "sms-read", "sms-delete", "sms-reply",
                                              "phonebook", "registration", "coherent-registration", "slow-pin-registration", "power-cycle", "accessory", "idle-state", "call-state", "sms-state", "divert-state",
                                              "host-incoming-call", "host-incoming-sms", "host-incoming-sms-text", "host-outgoing-sms",
                                              "host-rejected-sms", "host-silent-sms", "host-outgoing-call"),
                        default="calculator")
    parser.add_argument("--rompath", type=Path,
                        help="directory containing acquired noki6250 ROM members")
    parser.add_argument("--port", type=int, default=16250)
    parser.add_argument('--coherent-cell', action='store_true',
                        help='use explicit ARFCN19/20 network and require carrier coherence')
    parser.add_argument('--pin-enabled', action='store_true',
                        help='require slow physical PIN entry before coherent services or phonebook')
    args = parser.parse_args()
    if args.coherent_cell and not (args.scenario.startswith('host-') or args.scenario in ('idle-state', 'call-state', 'sms-state', 'phonebook')):
        parser.error('coherent-cell downstream coverage requires a host, state or phonebook scenario')
    if args.pin_enabled and (not args.coherent_cell or args.scenario not in
                             ('host-incoming-call', 'host-incoming-sms', 'host-outgoing-call', 'host-outgoing-sms', 'host-rejected-sms', 'host-silent-sms', 'phonebook', 'idle-state', 'call-state', 'sms-state')):
        parser.error('pin-enabled requires coherent host services, restoration or phonebook')
    root = Path(__file__).resolve().parents[1]
    mame = (args.mame or root / "mame/mame").resolve()
    rompath = (args.rompath or root / "roms").resolve()
    run = args.run_directory.resolve()
    try:
        if not mame.is_file():
            raise ValueError(f"missing MAME executable: {mame}")
        accessory_contract, audit_members = prepare_run(run, root)
        if args.scenario == 'slow-pin-registration' or args.pin_enabled:
            from tools.make_sim_card_profile import make_profile
            card = run / 'nvram/nhm3hle/sim_card'
            card.parent.mkdir(parents=True)
            card.write_bytes(make_profile(pin_enabled=True))
        host_call = args.scenario == "host-incoming-call"
        host = args.scenario.startswith("host-")
        host_incoming_sms = args.scenario in ("host-incoming-sms", "host-incoming-sms-text")
        host_sms = host_incoming_sms or args.scenario in ("host-outgoing-sms", "host-rejected-sms", "host-silent-sms")
        call = args.scenario in ("incoming-call", "outgoing-call", "host-incoming-call", "host-outgoing-call")
        sms = args.scenario.startswith("sms-") or host_sms
        if args.scenario in ('toolkit', 'toolkit-interactive', 'toolkit-menu', 'toolkit-sms', 'toolkit-call', 'toolkit-call-decline'):
            config = ET.Element('mameconfig', version='10')
            inputs = ET.SubElement(ET.SubElement(config, 'system', name='nhm3hle'), 'input')
            ET.SubElement(inputs, 'port', tag=':SATCFG', type='CONFIG',
                          mask='15', defvalue='0', value={'toolkit': '1', 'toolkit-interactive': '3', 'toolkit-menu': '4', 'toolkit-sms': '5', 'toolkit-call': '6', 'toolkit-call-decline': '6'}[args.scenario])
            ET.ElementTree(config).write(run / 'cfg/nhm3hle.cfg', encoding='utf-8', xml_declaration=True)
        if args.scenario == "incoming-call":
            shutil.copyfile(root / "fixtures/radio_incoming_call_answered/nhm3hle.cfg",
                            run / "cfg/nhm3hle.cfg")
        if sms and not host_incoming_sms:
            shutil.copyfile(root / "fixtures/radio_incoming_sms/nhm3hle.cfg",
                            run / "cfg/nhm3hle.cfg")
        if host:
            config_path = run / "cfg/nhm3hle.cfg"
            if config_path.exists():
                config = ET.parse(config_path).getroot()
                inputs = config.find("system/input")
            else:
                config = ET.Element("mameconfig", version="10")
                inputs = ET.SubElement(ET.SubElement(config, "system", name="nhm3hle"), "input")
            ET.SubElement(inputs, "port", tag=":CALLHOST", type="CONFIG",
                          mask="1", defvalue="0", value="1")
            ET.ElementTree(config).write(config_path, encoding="utf-8", xml_declaration=True)
        if args.scenario in ('coherent-registration', 'slow-pin-registration', 'power-cycle') or args.coherent_cell:
            apply_coherent_config(run / 'cfg/nhm3hle.cfg', root / 'fixtures/noki6250_host_gsm900/nhm3hle.cfg')
        if args.coherent_cell and args.scenario in ('idle-state', 'call-state', 'sms-state'):
            config_path = run / 'cfg/nhm3hle.cfg'
            configuration = ET.parse(config_path)
            configuration.find("./system/input/port[@tag=':CALLHOST']").set('value', '0')
            configuration.write(config_path, encoding='utf-8', xml_declaration=True)
        script = "noki6250_call_observe.lua" if call else "noki6250_app_observe.lua"
        if sms:
            script = "noki6250_sms_observe.lua"
        if args.scenario == "phonebook":
            script = "noki6250_phonebook_observe.lua"
        if args.scenario in ("registration", "coherent-registration", "accessory"):
            script = "noki6250_runtime_observe.lua"
        if args.scenario == 'slow-pin-registration':
            script = 'noki6250_slow_pin_observe.lua'
        if args.scenario == "idle-state":
            script = "noki6250_state_idle.lua"
        if args.scenario == "call-state":
            script = "noki6250_state_call.lua"
        if args.scenario == "sms-state":
            script = "noki6250_state_sms.lua"
        if args.scenario == 'divert-state':
            script = 'noki6250_state_divert.lua'
        if args.scenario == 'power-cycle':
            script = 'noki6250_power_input.lua'
        if args.scenario in ('ussd', 'divert', 'toolkit'):
            script = f'noki6250_{args.scenario}_input.lua'
        if args.scenario == 'toolkit-interactive':
            script = 'noki6250_toolkit_interactive.lua'
        if args.scenario == 'toolkit-menu':
            script = 'noki6250_toolkit_menu.lua'
        if args.scenario == 'toolkit-sms':
            script = 'noki6250_toolkit_sms.lua'
        if args.scenario in ('toolkit-call', 'toolkit-call-decline'):
            script = 'noki6250_' + args.scenario.replace('-', '_') + '.lua'
        if args.scenario == 'divert-lifecycle':
            script = 'noki6250_divert_lifecycle_input.lua'
        if host_call:
            script = "noki6250_host_incoming_input.lua"
        if args.scenario == "host-rejected-sms":
            script = "noki6250_sms_reject_input.lua"
        if args.scenario == "host-silent-sms":
            script = "noki6250_sms_silence_input.lua"
        seconds = "50" if args.scenario in ("sms-reply", "host-outgoing-sms") else "35" if call or sms else "45"
        if args.scenario == 'toolkit-menu':
            seconds = '75'
        if args.scenario == 'toolkit-sms':
            seconds = '100'
        if args.scenario in ('toolkit-call', 'toolkit-call-decline'):
            seconds = '105'
        if args.scenario == 'toolkit-interactive':
            seconds = '65'
        if args.scenario == 'power-cycle':
            seconds = '80'
        if args.scenario in ('divert-lifecycle', 'divert-state'):
            seconds = '80'
        if args.pin_enabled and args.scenario in ('host-outgoing-call', 'host-outgoing-sms'):
            seconds = '60'
        command = [str(mame), "nhm3hle", "-rompath",
                   f"{run / 'roms'};{rompath}",
                   "-nvram_directory", "nvram", "-cfg_directory", "cfg",
                   "-noreadconfig", "-autoboot_script",
                   str(root / "tools" / script),
                   "-autoboot_delay", "0", "-seconds_to_run", seconds,
                   "-video", "none", "-sound", "none", "-nothrottle",
                   "-log", "-verbose"]
        if host:
            if args.scenario == "host-silent-sms":
                command[command.index("-seconds_to_run") + 1] = "135"
            if args.scenario == "host-rejected-sms":
                command[command.index("-seconds_to_run") + 1] = "60"
            if host_call:
                command[command.index("-seconds_to_run") + 1] = "60"
            command.extend(["-http", "-http_port", str(args.port)])
        host_command = ([sys.executable, str(root / "tools/run_host_incoming_signaling_gate.py"),
                         "--port", str(args.port), "--cwd", str(run), "--caller", "5551234",
                         "--ready-file", str(run / "snap/6250_host_registered_idle.png"),
                         "--"] + command) if host_call else None
        if args.scenario == "host-outgoing-call":
            host_command = [sys.executable, str(root / "tools/run_host_call_adapter_gate.py"),
                            "--port", str(args.port), "--cwd", str(run),
                            "--number", "123", "--decision", "connect", "--"] + command
        if host_sms:
            runner = "run_host_incoming_sms_gate.py" if host_incoming_sms else "run_host_sms_gate.py"
            options = [] if host_incoming_sms else ["--user-data", "c834", "--user-data-length", "2"]
            if args.scenario == "host-incoming-sms-text":
                options.extend(["--text", "@_{}"])
            if args.scenario == "host-rejected-sms":
                options.extend(["--decision", "rp_error"])
            if args.scenario == "host-silent-sms":
                options.extend(["--decision", "rp_silence"])
            host_command = [sys.executable, str(root / "tools" / runner),
                            "--port", str(args.port), "--cwd", str(run)] + options + ["--"] + command
        env = os.environ.copy()
        env.pop('NOKIA_DCT3_6250_PIN_ENTRY', None)
        if args.pin_enabled:
            env['NOKIA_DCT3_6250_PIN_ENTRY'] = '1'
        flags = {"calculator": "NOKIA_DCT3_6250_CALCULATOR",
                 "outgoing-call": "NOKIA_DCT3_6250_OUTGOING",
                 "call-state": "NOKIA_DCT3_6250_OUTGOING",
                 "sms-delete": "NOKIA_DCT3_6250_SMS_DELETE",
                 "sms-reply": "NOKIA_DCT3_6250_SMS_REPLY"}
        flags["host-outgoing-sms"] = "NOKIA_DCT3_6250_SMS_REPLY"
        flags["host-rejected-sms"] = "NOKIA_DCT3_6250_SMS_REPLY"
        flags["host-silent-sms"] = "NOKIA_DCT3_6250_SMS_REPLY"
        flags["host-outgoing-call"] = "NOKIA_DCT3_6250_OUTGOING"
        for flag in flags.values():
            env.pop(flag, None)
        if args.scenario in flags:
            env[flags[args.scenario]] = "1"
        (run / "acceptance.json").write_text(json.dumps({
            "machine": "nhm3hle", "scenario": args.scenario, "command": command,
            "provisioning": "derived acquired initial-record PMM comparison",
            "audio": "not tested", "normal_machine_boot": "not tested",
            "laboratory_carrier": 19 if args.scenario in ('coherent-registration', 'slow-pin-registration', 'power-cycle') or args.coherent_cell else None,
            "slow_physical_pin": args.pin_enabled or args.scenario == 'slow-pin-registration',
            "shared_rom_audit_members": audit_members,
            "accessory_contract": accessory_contract,
            "host_command": host_command,
        }, indent=2) + "\n")
        with (run / "console.log").open("w") as console:
            subprocess.run(host_command or command, cwd=run, env=env, stdout=console,
                           stderr=subprocess.STDOUT, check=True)
        if args.scenario == 'divert-lifecycle':
            check_divert_lifecycle((run / 'error.log').read_text(errors='replace'), run / 'snap')
            checker = [sys.executable, str(root / 'tools/radio_registration_trace_check.py'),
                       str(run / 'error.log'), '--profile', 'nhm3']
        elif args.scenario == 'toolkit':
            check_toolkit((run / 'error.log').read_text(errors='replace'), run / 'snap')
        elif args.scenario in ('toolkit-interactive', 'toolkit-menu', 'toolkit-sms', 'toolkit-call', 'toolkit-call-decline'):
            check_interactive_toolkit((run / 'error.log').read_text(errors='replace'), run / 'snap',
                                      menu=args.scenario != 'toolkit-interactive',
                                      sms=args.scenario == 'toolkit-sms',
                                      call=args.scenario in ('toolkit-call', 'toolkit-call-decline'),
                                      decline=args.scenario == 'toolkit-call-decline')
            checker = [sys.executable, str(root / 'tools/radio_registration_trace_check.py'),
                       str(run / 'error.log'), '--profile', 'nhm3']
        elif args.scenario in ('ussd', 'divert'):
            check_supplementary((run / 'error.log').read_text(errors='replace'), run / 'snap', args.scenario)
            checker = [sys.executable, str(root / 'tools/radio_registration_trace_check.py'),
                       str(run / 'error.log'), '--profile', 'nhm3']
        elif call:
            checker = [sys.executable, str(root / "tools/noki6250_call_check.py"),
                       str(run / "error.log")]
            if args.scenario in ("outgoing-call", "host-outgoing-call"):
                checker.extend(["--outgoing", "--number", "123"])
            if args.coherent_cell:
                checker.extend(['--configured-carrier', '--frames', str(run / 'snap')])
        elif args.scenario in ("idle-state", "call-state", "sms-state", "divert-state"):
            checker = [sys.executable, str(root / "tools/noki6250_state_check.py"),
                       str(run / "error.log"), str(run / "snap")]
            if args.scenario == "call-state":
                checker.append("--call")
            elif args.scenario == "sms-state":
                checker.extend(["--sms", "--storage", str(run / "nvram/nhm3hle/sim_card")])
            elif args.scenario == 'divert-state':
                checker.append('--divert')
            if args.coherent_cell:
                checker.append('--configured-carrier')
        elif args.scenario in ("host-rejected-sms", "host-silent-sms"):
            checker = [sys.executable, str(root / "tools/noki6250_sms_failure_check.py"),
                       str(run / "error.log"), str(run / "snap")]
            if args.scenario == "host-silent-sms":
                checker.append("--rp-silence")
        elif sms:
            frame_index = {"sms-read": 2, "sms-delete": 5, "sms-reply": 8,
                           "host-incoming-sms": 2, "host-incoming-sms-text": 2, "host-outgoing-sms": 8}[args.scenario]
            if args.pin_enabled and host_incoming_sms:
                # Late SIM initialization reaches the same reviewed body
                # after the 22-second Read input, not the 18-second input.
                frame_index = 3
            frames = list((run / "snap").rglob(f"6250_sms_{frame_index}.png"))
            if len(frames) != 1:
                raise ValueError(f"expected one SMS frame, found {len(frames)}")
            checker = [sys.executable, str(root / "tools/noki6250_sms_check.py"),
                       str(run / "error.log"), str(run / "nvram/nhm3hle/sim_card"),
                       str(frames[0])]
            if args.scenario == "host-incoming-sms-text":
                checker.append("--text-fixture")
            elif args.scenario != "sms-read" and not host_incoming_sms:
                checker.append("--deleted" if args.scenario == "sms-delete" else "--sent")
        elif args.scenario == "phonebook":
            frames = list((run / "snap").rglob("6250_phonebook_7.png"))
            if len(frames) != 1:
                raise ValueError(f"expected one save frame, found {len(frames)}")
            checker = [sys.executable, str(root / "tools/noki6250_phonebook_check.py"),
                       "save", str(run / "nvram/nhm3hle/sim_card"), str(frames[0])]
        elif args.scenario == 'power-cycle':
            checker = [sys.executable, str(root / 'tools/noki6250_power_check.py'),
                       str(run / 'error.log'), str(run / 'nvram/nhm3hle/sim_card'), str(run / 'snap')]
        elif args.scenario == 'coherent-registration':
            checker = [sys.executable, str(root / 'tools/noki6250_coherent_registration_check.py'),
                       str(run / 'error.log'), str(run / 'nvram/nhm3hle/sim_card')]
        elif args.scenario == 'slow-pin-registration':
            checker = [sys.executable, str(root / 'tools/noki6250_slow_pin_check.py'),
                       str(run / 'error.log'), str(run / 'nvram/nhm3hle/sim_card')]
        elif args.scenario in ("registration", "accessory"):
            checker = [sys.executable, str(root / "tools/radio_registration_trace_check.py"),
                       str(run / "error.log"), "--profile", "nhm3"]
        else:
            frames = list((run / "snap").rglob("6250_app_14.png"))
            if len(frames) != 1:
                raise ValueError(f"expected one result frame, found {len(frames)}")
            checker = [sys.executable, str(root / "tools/noki6250_app_check.py"),
                       str(run / "error.log"), str(frames[0])]
        subprocess.run(checker, check=True)
        prerequisites = prerequisite_trace(
            (run / 'error.log').read_text(errors='replace'), args.scenario)
        if args.scenario == 'divert-state':
            from tools.radio_registration_trace_check import verify as check_saved_registration
            check_saved_registration(prerequisites, 'nhm3')
        if args.coherent_cell and args.scenario in ('idle-state', 'call-state', 'sms-state'):
            from tools.radio_registration_trace_check import verify as check_saved_registration
            check_saved_registration(prerequisites, 'nhm3', configured_carrier=True)
            if args.pin_enabled:
                from tools.sim_security_trace_check import validate as check_saved_security
                check_saved_security(prerequisites,
                    (run / 'nvram/nhm3hle/sim_card').read_bytes(), 'verify', '1234')
        if args.pin_enabled:
            from tools.noki6250_slow_pin_check import verify as check_slow_pin
            check_slow_pin((run / 'error.log').read_text(errors='replace'),
                           (run / 'nvram/nhm3hle/sim_card').read_bytes(),
                           require_host=args.scenario not in ('idle-state', 'call-state', 'sms-state'))
        if args.coherent_cell:
            from tools.noki6250_coherent_registration_check import verify as check_coherent
            check_coherent((run / 'error.log').read_text(errors='replace'),
                           (run / 'nvram/nhm3hle/sim_card').read_bytes(),
                           require_host=args.scenario not in ('idle-state', 'call-state', 'sms-state'))
        if args.scenario == "accessory":
            subprocess.run([sys.executable, str(root / 'tools/noki6250_accessory_check.py'),
                            str(run / 'error.log'), str(run / 'snap/6250_runtime20.png')], check=True)
        if args.scenario == "host-outgoing-call":
            from tools.radio_host_outgoing_connect_check import verify as check_host
            check_host((run / "error.log").read_text(errors="replace"), "123")
        if host_sms:
            name = "radio_incoming_host_sms_trace_check.py" if host_incoming_sms else "radio_outgoing_host_sms_trace_check.py"
            options = ['--arfcn', '19'] if host_incoming_sms and args.coherent_cell else []
            if not host_incoming_sms:
                outcome = {'host-rejected-sms': 'rp_error', 'host-silent-sms': 'rp_silence'}.get(args.scenario, 'rp_ack')
                options.extend(['--outcome', outcome])
            subprocess.run([sys.executable, str(root / "tools" / name), *options, str(run / "error.log")], check=True)
        if args.scenario == "phonebook":
            shutil.copyfile(run / "error.log", run / "phonebook-save.log")
            saved_sim = (run / "nvram/nhm3hle/sim_card").read_bytes()
            (run / "phonebook-save.sim").write_bytes(saved_sim)
            # A new MAME process reloads persisted flash/SIM, without a save state.
            command[command.index("-autoboot_script") + 1] = str(
                root / "tools/noki6250_phonebook_readback.lua")
            command[command.index("-seconds_to_run") + 1] = "30"
            if args.pin_enabled:
                command[command.index("-seconds_to_run") + 1] = "40"
            manifest = json.loads((run / "acceptance.json").read_text())
            manifest["cold_restart_command"] = command
            (run / "acceptance.json").write_text(json.dumps(manifest, indent=2) + "\n")
            with (run / "readback-console.log").open("w") as console:
                subprocess.run(command, cwd=run, env=env, stdout=console,
                               stderr=subprocess.STDOUT, check=True)
            if (run / "nvram/nhm3hle/sim_card").read_bytes() != saved_sim:
                raise ValueError("cold-start phonebook readback changed persistent SIM data")
            frames = list((run / "snap").rglob("6250_phonebook_readback_5.png"))
            if len(frames) != 1:
                raise ValueError(f"expected one cold-start frame, found {len(frames)}")
            checker[2] = "readback"
            checker[-1] = str(frames[0])
            subprocess.run(checker, check=True)
            if args.pin_enabled:
                from tools.sim_security_trace_check import validate as check_security
                from tools.noki6250_coherent_registration_check import verify as check_coherent
                cold_text = (run / 'error.log').read_text(errors='replace')
                cold_sim = (run / 'nvram/nhm3hle/sim_card').read_bytes()
                check_security(cold_text, cold_sim, 'verify', '1234')
                check_coherent(cold_text, cold_sim, preserved=True)
        if args.scenario == "registration":
            shutil.copyfile(run / "error.log", run / "registration-fresh.log")
            shutil.copyfile(run / "nvram/nhm3hle/sim_card", run / "registration-fresh.sim")
            with (run / "preserved-console.log").open("w") as console:
                subprocess.run(command, cwd=run, env=env, stdout=console,
                               stderr=subprocess.STDOUT, check=True)
            subprocess.run(checker + ["--preserved"], check=True)
            manifest = json.loads((run / "acceptance.json").read_text())
            manifest["cold_restart_command"] = command
            (run / "acceptance.json").write_text(json.dumps(manifest, indent=2) + "\n")
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"FAIL: {error}\n")
    print(f"Evidence: {run}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
