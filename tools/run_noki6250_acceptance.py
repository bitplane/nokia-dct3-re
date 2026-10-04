#!/usr/bin/env python3
"""Fresh, isolated NHM-3 research-profile physical acceptance runs.

Uses the explicitly derived initial-record PMM comparison, not factory data.
The normal noki6250 machine and the acquired PMM are left unchanged.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

try:
    from tools.noki6250_pmm_check import initial_record_fixture
except ModuleNotFoundError:
    from noki6250_pmm_check import initial_record_fixture


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_directory", type=Path,
                        help="new directory; existing directories are refused")
    parser.add_argument("--mame", type=Path)
    parser.add_argument("--scenario", choices=("calculator", "incoming-call", "outgoing-call",
                                              "sms-read", "sms-delete", "sms-reply",
                                              "phonebook", "registration"),
                        default="calculator")
    parser.add_argument("--rompath", type=Path,
                        help="directory containing acquired noki6250 ROM members")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    mame = (args.mame or root / "mame/mame").resolve()
    rompath = (args.rompath or root / "roms").resolve()
    source = root / "roms/noki6250/6250 virgin eeprom 005fa000.fls"
    run = args.run_directory.resolve()
    try:
        fixture = initial_record_fixture(source.read_bytes())
        if not mame.is_file():
            raise ValueError(f"missing MAME executable: {mame}")
        run.mkdir(parents=True, exist_ok=False)
        local_roms = run / "roms/noki6250"
        local_roms.mkdir(parents=True)
        (local_roms / source.name).write_bytes(fixture)
        (run / "cfg").mkdir()
        (run / "nvram").mkdir()
        call = args.scenario in ("incoming-call", "outgoing-call")
        sms = args.scenario.startswith("sms-")
        if args.scenario == "incoming-call":
            shutil.copyfile(root / "fixtures/radio_incoming_call_answered/nhm3hle.cfg",
                            run / "cfg/nhm3hle.cfg")
        if sms:
            shutil.copyfile(root / "fixtures/radio_incoming_sms/nhm3hle.cfg",
                            run / "cfg/nhm3hle.cfg")
        script = "noki6250_call_observe.lua" if call else "noki6250_app_observe.lua"
        if sms:
            script = "noki6250_sms_observe.lua"
        if args.scenario == "phonebook":
            script = "noki6250_phonebook_observe.lua"
        if args.scenario == "registration":
            script = "noki6250_runtime_observe.lua"
        seconds = "50" if args.scenario == "sms-reply" else "35" if call or sms else "45"
        command = [str(mame), "nhm3hle", "-rompath",
                   f"{run / 'roms'};{rompath}",
                   "-nvram_directory", "nvram", "-cfg_directory", "cfg",
                   "-noreadconfig", "-autoboot_script",
                   str(root / "tools" / script),
                   "-autoboot_delay", "0", "-seconds_to_run", seconds,
                   "-video", "none", "-sound", "none", "-nothrottle",
                   "-log", "-verbose"]
        env = os.environ.copy()
        flags = {"calculator": "NOKIA_DCT3_6250_CALCULATOR",
                 "outgoing-call": "NOKIA_DCT3_6250_OUTGOING",
                 "sms-delete": "NOKIA_DCT3_6250_SMS_DELETE",
                 "sms-reply": "NOKIA_DCT3_6250_SMS_REPLY"}
        for flag in flags.values():
            env.pop(flag, None)
        if args.scenario in flags:
            env[flags[args.scenario]] = "1"
        (run / "acceptance.json").write_text(json.dumps({
            "machine": "nhm3hle", "scenario": args.scenario, "command": command,
            "provisioning": "derived acquired initial-record PMM comparison",
            "audio": "not tested", "normal_machine_boot": "not tested",
        }, indent=2) + "\n")
        with (run / "console.log").open("w") as console:
            subprocess.run(command, cwd=run, env=env, stdout=console,
                           stderr=subprocess.STDOUT, check=True)
        if call:
            checker = [sys.executable, str(root / "tools/noki6250_call_check.py"),
                       str(run / "error.log")]
            if args.scenario == "outgoing-call":
                checker.extend(["--outgoing", "--number", "123"])
        elif sms:
            frame_index = {"sms-read": 2, "sms-delete": 5, "sms-reply": 8}[args.scenario]
            frames = list((run / "snap").rglob(f"6250_sms_{frame_index}.png"))
            if len(frames) != 1:
                raise ValueError(f"expected one SMS frame, found {len(frames)}")
            checker = [sys.executable, str(root / "tools/noki6250_sms_check.py"),
                       str(run / "error.log"), str(run / "nvram/nhm3hle/sim_card"),
                       str(frames[0])]
            if args.scenario != "sms-read":
                checker.append("--deleted" if args.scenario == "sms-delete" else "--sent")
        elif args.scenario == "phonebook":
            frames = list((run / "snap").rglob("6250_phonebook_7.png"))
            if len(frames) != 1:
                raise ValueError(f"expected one save frame, found {len(frames)}")
            checker = [sys.executable, str(root / "tools/noki6250_phonebook_check.py"),
                       "save", str(run / "nvram/nhm3hle/sim_card"), str(frames[0])]
        elif args.scenario == "registration":
            checker = [sys.executable, str(root / "tools/radio_registration_trace_check.py"),
                       str(run / "error.log"), "--profile", "nhm3"]
        else:
            frames = list((run / "snap").rglob("6250_app_14.png"))
            if len(frames) != 1:
                raise ValueError(f"expected one result frame, found {len(frames)}")
            checker = [sys.executable, str(root / "tools/noki6250_app_check.py"),
                       str(run / "error.log"), str(frames[0])]
        subprocess.run(checker, check=True)
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
