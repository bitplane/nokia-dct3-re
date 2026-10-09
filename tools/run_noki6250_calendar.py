#!/usr/bin/env python3
"""Physical NHM-3 Calendar entry and own-NVRAM cold presentation (research HLE)."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.run_noki6250_acceptance import prepare_run

FRAME_SHA256 = "fcf0327cc0cc4edb952d0dc37621e467c0810b867738de6c91f39354a9c3a3b3"


def check_phase(text, cold):
    if "[LUA ERROR]" in text:
        raise ValueError("NHM-3 Calendar physical script failed")
    actions = re.findall(r"6250_calendar_physical: action=(\w+)\b", text)
    expected = ["menu", *[f"down_{index}" for index in range(1, 8)], "calendar"]
    if not cold:
        expected.extend([*[f"time_{index}" for index in range(1, 5)], "time_confirm",
                         *[f"date_{index}" for index in range(1, 9)], "date_confirm"])
    if actions != expected:
        raise ValueError("NHM-3 Calendar physical sequence differs")
    event = "cold_presented" if cold else "entered_presented"
    if text.count("6250_calendar_physical: event=" + event) != 1:
        raise ValueError("NHM-3 Calendar presentation marker missing or duplicated")
    if cold:
        for reg, value in (("09", "0d"), ("08", "2f")):
            if f"ccont_rtc: event=read reg={reg} data={value} " not in text:
                raise ValueError("cold NHM-3 clock did not retain 13:47")
    else:
        after = text.split("6250_calendar_physical: action=time_confirm", 1)[1]
        before_date = after.split("6250_calendar_physical: action=date_1", 1)[0]
        minute = "ccont_rtc: event=alarm_write reg=0b data=2f "
        hour = "ccont_rtc: event=alarm_write reg=0c data=8d "
        if minute not in before_date or hour not in before_date or before_date.index(minute) >= before_date.index(hour):
            raise ValueError("physical time entry did not program the observed RTC sequence")

def check_frame(path, expected):
    from PIL import Image
    with Image.open(path) as frame:
        if frame.size != (96, 60):
            raise ValueError("unexpected NHM-3 Calendar geometry")
        digest = hashlib.sha256(frame.convert("L").tobytes()).hexdigest()
    if digest != expected:
        raise ValueError(f"unreviewed NHM-3 Calendar frame: {digest}")


def run_phase(root, phase, cold, mame):
    command = [str(mame), "nhm3hle", "-rompath",
               f"{phase / 'roms'};{root / 'roms'}", "-nvram_directory", "nvram",
               "-cfg_directory", "cfg", "-noreadconfig", "-autoboot_script",
               str(root / "tools/noki6250_calendar_input.lua"), "-autoboot_delay", "0",
               "-seconds_to_run", "45" if cold else "70", "-video", "none",
               "-sound", "none", "-nothrottle", "-log", "-verbose"]
    env = os.environ.copy()
    env.pop("NOKIA_DCT3_6250_CALENDAR_COLD", None)
    if cold:
        env["NOKIA_DCT3_6250_CALENDAR_COLD"] = "1"
    with (phase / "console.log").open("w") as console:
        subprocess.run(command, cwd=phase, env=env, stdout=console,
                       stderr=subprocess.STDOUT, check=True)
    text = (phase / "error.log").read_text(errors="replace")
    check_phase(text, cold)
    checker = [sys.executable, str(root / "tools/radio_registration_trace_check.py"),
               str(phase / "error.log"), "--profile", "nhm3"]
    if cold:
        checker.append("--preserved")
    subprocess.run(checker, check=True)
    name = "cold" if cold else "entered"
    check_frame(phase / "snap" / f"6250_calendar_{name}.png", FRAME_SHA256)
    return text


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_directory", type=Path)
    parser.add_argument("--mame", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    mame = (args.mame or root / "mame/mame").resolve()
    try:
        run.mkdir(parents=True, exist_ok=False)
        seed, cold = run / "seed", run / "cold"
        prepare_run(seed, root)
        text = run_phase(root, seed, False, mame)
        if "6250_calendar_physical: event=entered_presented" not in text:
            raise ValueError("physical Calendar entry did not finish")
        # A separate process receives only this handset's persisted state.
        prepare_run(cold, root)
        shutil.copytree(seed / "nvram", cold / "nvram", dirs_exist_ok=True)
        shutil.copytree(seed / "cfg", cold / "cfg", dirs_exist_ok=True)
        replay = run_phase(root, cold, True, mame)
        if "6250_calendar_physical: event=cold_presented" not in replay:
            raise ValueError("cold Calendar presentation did not finish")
        if "action=time_" in replay or "action=date_" in replay:
            raise ValueError("cold Calendar fixture replaced stored input")
        print("6250 research-HLE physical Calendar, retained clock/date and cold registration PASS; native/audio not tested")
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"6250 Calendar failed: {error}\n")


if __name__ == "__main__":
    main()
