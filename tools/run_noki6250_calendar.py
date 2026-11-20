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
ROLLOVER_SHA256 = "a04551d523cb8e0adf4efe4da7e6bdcc6ec683dc264a8e791aed57ae798d4d01"


def check_phase(text, cold, midnight=False, entered=None):
    if "[LUA ERROR]" in text:
        raise ValueError("NHM-3 Calendar physical script failed")
    actions = re.findall(r"6250_calendar_physical: action=(\w+)\b", text)
    expected = ["menu", *[f"down_{index}" for index in range(1, 8)], "calendar"]
    if not cold:
        expected.extend([*[f"time_{index}" for index in range(1, 5)], "time_confirm",
                         *[f"date_{index}" for index in range(1, 9)], "date_confirm"])
        if midnight:
            expected.extend(['midnight_back', 'midnight_reopen'])
    if actions != expected:
        raise ValueError("NHM-3 Calendar physical sequence differs")
    event = "cold_presented" if cold else "entered_presented"
    if text.count("6250_calendar_physical: event=" + event) != 1:
        raise ValueError("NHM-3 Calendar presentation marker missing or duplicated")
    hour, minute = entered or (13, 47)
    if cold:
        for reg, value in (("09", "00"), ("08", "00")) if midnight else (("09", f"{hour:02x}"), ("08", f"{minute:02x}")):
            if f"ccont_rtc: event=read reg={reg} data={value} " not in text:
                raise ValueError("cold NHM-3 clock did not retain the entered/rolled-over time")
    else:
        after = text.split("6250_calendar_physical: action=time_confirm", 1)[1]
        before_date = after.split("6250_calendar_physical: action=date_1", 1)[0]
        minute = "ccont_rtc: event=alarm_write reg=0b data=" + ('3b ' if midnight else f'{minute:02x} ')
        hour = "ccont_rtc: event=alarm_write reg=0c data=" + ('97 ' if midnight else f'{0x80 | hour:02x} ')
        if minute not in before_date or hour not in before_date or before_date.index(minute) >= before_date.index(hour):
            raise ValueError("physical time entry did not program the observed RTC sequence")
        if midnight:
            cursor = text.index('6250_calendar_physical: event=entered_presented')
            for token in ('event=second time=00:00:00 day=1 ',
                          '6250_calendar_physical: action=midnight_back',
                          'event=read reg=0a data=01 ',
                          'event=read reg=0a data=00 ',
                          '6250_calendar_physical: event=midnight_presented'):
                position = text.find(token, cursor)
                if position < 0:
                    raise ValueError('missing ordered NHM-3 rollover evidence: ' + token)
                cursor = position + len(token)

def check_frame(path, expected):
    from PIL import Image
    with Image.open(path) as frame:
        if frame.size != (96, 60):
            raise ValueError("unexpected NHM-3 Calendar geometry")
        digest = hashlib.sha256(frame.convert("L").tobytes()).hexdigest()
    if digest != expected:
        raise ValueError(f"unreviewed NHM-3 Calendar frame: {digest}")


def run_phase(root, phase, cold, mame, midnight=False, entered=None, seconds=None, cold_time=None):
    command = [str(mame), "nhm3hle", "-rompath",
               f"{phase / 'roms'};{root / 'roms'}", "-nvram_directory", "nvram",
               "-cfg_directory", "cfg", "-noreadconfig", "-autoboot_script",
               str(root / ('tools/noki6250_calendar_midnight.lua' if midnight and not cold
                           else 'tools/noki6250_calendar_input.lua')), "-autoboot_delay", "0",
               "-seconds_to_run", "45" if cold else "140" if midnight else str(seconds or 70), "-video", "none",
               "-sound", "none", "-nothrottle", "-log", "-verbose"]
    env = os.environ.copy()
    env.pop("NOKIA_DCT3_6250_CALENDAR_COLD", None)
    env.pop("NOKIA_DCT3_6250_CALENDAR_TIME", None)
    if cold:
        env["NOKIA_DCT3_6250_CALENDAR_COLD"] = "1"
    elif entered:
        env["NOKIA_DCT3_6250_CALENDAR_TIME"] = f"{entered[0]:02d}{entered[1]:02d}"
    with (phase / "console.log").open("w") as console:
        subprocess.run(command, cwd=phase, env=env, stdout=console,
                       stderr=subprocess.STDOUT, check=True)
    text = (phase / "error.log").read_text(errors="replace")
    check_phase(text, cold, midnight, cold_time if cold else entered)
    checker = [sys.executable, str(root / "tools/radio_registration_trace_check.py"),
               str(phase / "error.log"), "--profile", "nhm3"]
    if cold:
        checker.append("--preserved")
    subprocess.run(checker, check=True)
    name = "cold" if cold else "entered"
    check_frame(phase / "snap" / f"6250_calendar_{name}.png",
                ROLLOVER_SHA256 if midnight and cold else FRAME_SHA256)
    if midnight:
        if cold:
            check_frame(phase / 'snap/6250_calendar_cold.png', ROLLOVER_SHA256)
        else:
            check_frame(phase / 'snap/6250_calendar_midnight_open.png', FRAME_SHA256)
            check_frame(phase / 'snap/6250_calendar_midnight_result.png', ROLLOVER_SHA256)
    return text


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_directory", type=Path)
    parser.add_argument("--mame", type=Path)
    parser.add_argument('--midnight', action='store_true')
    parser.add_argument('--time', default='1347',
                        help='HHMM physically entered (alarm seeds use 1346)')
    parser.add_argument('--seed-seconds', type=int, default=70,
                        help='seed process length; time confirmation lands at about second 42')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    mame = (args.mame or root / "mame/mame").resolve()
    try:
        run.mkdir(parents=True, exist_ok=False)
        seed, cold = run / "seed", run / "cold"
        prepare_run(seed, root)
        if not re.fullmatch(r"([01]\d|2[0-3])[0-5]\d", args.time) or (args.midnight and args.time != '1347'):
            parser.error('--time must be HHMM and cannot combine with --midnight')
        entered = (int(args.time[:2]), int(args.time[2:]))
        # The RTC ticks once per emulated second after the confirmation near second 42;
        # the cold process starts from whatever minute the seed process saved.
        elapsed = entered[0] * 60 + entered[1] + max(0, args.seed_seconds - 42) // 60
        cold_time = (elapsed // 60 % 24, elapsed % 60)
        text = run_phase(root, seed, False, mame, args.midnight, entered, args.seed_seconds)
        if "6250_calendar_physical: event=entered_presented" not in text:
            raise ValueError("physical Calendar entry did not finish")
        # A separate process receives only this handset's persisted state.
        prepare_run(cold, root)
        shutil.copytree(seed / "nvram", cold / "nvram", dirs_exist_ok=True)
        shutil.copytree(seed / "cfg", cold / "cfg", dirs_exist_ok=True)
        replay = run_phase(root, cold, True, mame, args.midnight, cold_time=cold_time)
        if "6250_calendar_physical: event=cold_presented" not in replay:
            raise ValueError("cold Calendar presentation did not finish")
        if "action=time_" in replay or "action=date_" in replay:
            raise ValueError("cold Calendar fixture replaced stored input")
        print("6250 research-HLE physical Calendar, retained clock/date and cold registration PASS; native/audio not tested")
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"6250 Calendar failed: {error}\n")


if __name__ == "__main__":
    main()
