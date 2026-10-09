#!/usr/bin/env python3
"""Own NHM-3 physical alarm, natural RTC expiry and Stop (research HLE)."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.run_noki6250_acceptance import prepare_run
from tools.run_noki6250_calendar import check_frame

FRAMES = {
    "confirm": "753c5b7d172c9e418a4c9689e463c94bbaba0d2522c9972784f6e59716f0e0fd",
    "idle": "380f4d4eaa6a49826c1f95ac205d779ab1a1f22e93322aaabb1654ed39825b05",
    "elapsed": "c3b31aa953ec44ff0db2d9d0f08abbab9e5ae51d09adc7775b804e606f4e981d",
    "stopped": "7c541cfc93c2da8e854421941df0ac81755b73f47c3af98f2f6a40efac181b0b",
}


def check_alarm(text):
    actions = re.findall(r"6250_alarm_physical: action=(\w+)\b", text)
    expected = ["menu", *[f"down_{i}" for i in range(1, 10)], "clock",
                "clock_down_1", "alarm", *[f"time_{i}" for i in range(1, 5)],
                "confirm", "idle", "stop"]
    if "[LUA ERROR]" in text or actions != expected:
        raise ValueError("NHM-3 physical alarm sequence differs")
    cursor = 0
    for pattern in (
        r"6250_alarm_physical: action=confirm\b",
        r"ccont_rtc: event=alarm_write reg=0b data=30 armed=1\b",
        r"ccont_rtc: event=alarm_write reg=0c data=0d armed=1\b",
        r"6250_alarm_physical: action=idle\b",
        r"ccont_rtc: event=second time=13:48:00 day=0 status=b1 mask=30\b",
        r"ccont_rtc: event=status_ack data=81 old=b1\b",
        r"buzzer: enabled=1 divider=\d+ frequency=\d+ volume=[1-9]\d*\b",
        r"6250_alarm_physical: action=stop\b",
        r"buzzer: enabled=0 divider=0 frequency=0\b",
        r"6250_alarm_physical: event=stopped_presented\b",
    ):
        match = re.search(pattern, text[cursor:])
        if not match:
            raise ValueError("missing/out-of-order NHM-3 alarm boundary: " + pattern)
        cursor += match.end()
    after_stop = text.split("6250_alarm_physical: event=stopped_presented", 1)[1]
    if "buzzer: enabled=1" in after_stop:
        raise ValueError("NHM-3 buzzer resumed after Stop settled")


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
        clock = run / "clock"
        subprocess.run([sys.executable, str(root / "tools/run_noki6250_calendar.py"),
                        str(clock), "--mame", str(mame)], check=True)
        phase = run / "alarm"
        prepare_run(phase, root)
        for name in ("nvram", "cfg"):
            shutil.copytree(clock / "seed" / name, phase / name, dirs_exist_ok=True)
        command = [str(mame), "nhm3hle", "-rompath",
                   f"{phase / 'roms'};{root / 'roms'}", "-nvram_directory", "nvram",
                   "-cfg_directory", "cfg", "-noreadconfig", "-autoboot_script",
                   str(root / "tools/noki6250_alarm_input.lua"), "-autoboot_delay", "0",
                   "-seconds_to_run", "80", "-video", "none", "-sound", "none",
                   "-nothrottle", "-log", "-verbose"]
        with (phase / "console.log").open("w") as console:
            subprocess.run(command, cwd=phase, env=os.environ.copy(), stdout=console,
                           stderr=subprocess.STDOUT, check=True)
        text = (phase / "error.log").read_text(errors="replace")
        check_alarm(text)
        for name, digest in FRAMES.items():
            check_frame(phase / "snap" / f"6250_alarm_{name}.png", digest)
        subprocess.run([sys.executable, str(root / "tools/radio_registration_trace_check.py"),
                        str(phase / "error.log"), "--profile", "nhm3", "--preserved"], check=True)
        print("6250 research-HLE physical alarm, natural RTC expiry and Stop PASS; audio/native/off-wake not tested")
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"6250 alarm failed: {error}\n")


if __name__ == "__main__":
    main()
