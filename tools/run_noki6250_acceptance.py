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
    parser.add_argument("--scenario", choices=("calculator", "incoming-call", "outgoing-call",
                                              "sms-read", "sms-delete", "sms-reply",
                                              "phonebook", "registration", "coherent-registration", "slow-pin-registration", "power-cycle", "accessory", "idle-state", "call-state", "sms-state",
                                              "host-incoming-call", "host-incoming-sms", "host-incoming-sms-text", "host-outgoing-sms",
                                              "host-rejected-sms", "host-silent-sms", "host-outgoing-call"),
                        default="calculator")
    parser.add_argument("--rompath", type=Path,
                        help="directory containing acquired noki6250 ROM members")
    parser.add_argument("--port", type=int, default=16250)
    parser.add_argument('--coherent-cell', action='store_true',
                        help='use explicit ARFCN19/20 network and require carrier coherence')
    parser.add_argument('--pin-enabled', action='store_true',
                        help='require slow physical PIN entry before coherent host call/SMS')
    args = parser.parse_args()
    if args.coherent_cell and not (args.scenario.startswith('host-') or args.scenario == 'idle-state'):
        parser.error('coherent-cell downstream coverage requires a host or idle-state scenario')
    if args.pin_enabled and (not args.coherent_cell or args.scenario not in
                             ('host-incoming-call', 'host-incoming-sms', 'host-outgoing-call', 'host-outgoing-sms')):
        parser.error('pin-enabled requires coherent host call or SMS')
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
        if args.scenario == 'power-cycle':
            script = 'noki6250_power_input.lua'
        if host_call:
            script = "noki6250_host_incoming_input.lua"
        if args.scenario == "host-rejected-sms":
            script = "noki6250_sms_reject_input.lua"
        if args.scenario == "host-silent-sms":
            script = "noki6250_sms_silence_input.lua"
        seconds = "50" if args.scenario in ("sms-reply", "host-outgoing-sms") else "35" if call or sms else "45"
        if args.scenario == 'power-cycle':
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
        if call:
            checker = [sys.executable, str(root / "tools/noki6250_call_check.py"),
                       str(run / "error.log")]
            if args.scenario in ("outgoing-call", "host-outgoing-call"):
                checker.extend(["--outgoing", "--number", "123"])
            if args.coherent_cell:
                checker.extend(['--configured-carrier', '--frames', str(run / 'snap')])
        elif args.scenario in ("idle-state", "call-state", "sms-state"):
            checker = [sys.executable, str(root / "tools/noki6250_state_check.py"),
                       str(run / "error.log"), str(run / "snap")]
            if args.scenario == "call-state":
                checker.append("--call")
            elif args.scenario == "sms-state":
                checker.extend(["--sms", "--storage", str(run / "nvram/nhm3hle/sim_card")])
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
        if args.pin_enabled:
            from tools.noki6250_slow_pin_check import verify as check_slow_pin
            check_slow_pin((run / 'error.log').read_text(errors='replace'),
                           (run / 'nvram/nhm3hle/sim_card').read_bytes())
        if args.coherent_cell:
            from tools.noki6250_coherent_registration_check import verify as check_coherent
            check_coherent((run / 'error.log').read_text(errors='replace'),
                           (run / 'nvram/nhm3hle/sim_card').read_bytes())
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
