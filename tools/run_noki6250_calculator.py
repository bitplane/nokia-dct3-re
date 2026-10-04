#!/usr/bin/env python3
"""Fresh, isolated NHM-3 research-profile calculator acceptance run.

Uses the explicitly derived initial-record PMM comparison, not factory data.
The normal noki6250 machine and the acquired PMM are left unchanged.
"""
import argparse
import json
import os
from pathlib import Path
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
        command = [str(mame), "nhm3hle", "-rompath",
                   f"{run / 'roms'};{rompath}",
                   "-nvram_directory", "nvram", "-cfg_directory", "cfg",
                   "-noreadconfig", "-autoboot_script",
                   str(root / "tools/noki6250_app_observe.lua"),
                   "-autoboot_delay", "0", "-seconds_to_run", "45",
                   "-video", "none", "-sound", "none", "-nothrottle",
                   "-log", "-verbose"]
        env = os.environ.copy()
        env["NOKIA_DCT3_6250_CALCULATOR"] = "1"
        (run / "acceptance.json").write_text(json.dumps({
            "machine": "nhm3hle", "command": command,
            "provisioning": "derived acquired initial-record PMM comparison",
            "audio": "not tested", "normal_machine_boot": "not tested",
        }, indent=2) + "\n")
        with (run / "console.log").open("w") as console:
            subprocess.run(command, cwd=run, env=env, stdout=console,
                           stderr=subprocess.STDOUT, check=True)
        frames = list((run / "snap").rglob("6250_app_14.png"))
        if len(frames) != 1:
            raise ValueError(f"expected one result frame, found {len(frames)}")
        subprocess.run([sys.executable, str(root / "tools/noki6250_app_check.py"),
                        str(run / "error.log"), str(frames[0])], check=True)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"FAIL: {error}\n")
    print(f"Evidence: {run}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
