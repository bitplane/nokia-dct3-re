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


def check_alarm(text, power_choice=None, snooze=False):
    if power_choice not in (None, "yes", "no"):
        raise ValueError("unknown NHM-3 alarm activation choice")
    actions = re.findall(r"6250_alarm_physical: action=(\w+)\b", text)
    expected = ["menu", *[f"down_{i}" for i in range(1, 10)], "clock",
                "clock_down_1", "alarm", *[f"time_{i}" for i in range(1, 5)],
                "confirm", "idle", "stop"]
    if power_choice:
        expected[-1:-1] = ["power_off", "power_release"]
        expected.append("activate_" + power_choice)
    if snooze:
        expected.insert(expected.index("stop"), "snooze")
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
    if snooze:
        cursor = text.index("6250_alarm_physical: action=snooze")
        for pattern in (
            r"buzzer: enabled=0 divider=0 frequency=0\b",
            r"ccont_rtc: event=alarm_write reg=0b data=35 armed=1\b",
            r"ccont_rtc: event=alarm_write reg=0c data=0d armed=1\b",
            r"6250_alarm_physical: event=snoozed_presented\b",
            rf"ccont_rtc: event=second time=13:53:00 day=0 status=b1 mask={'50' if power_choice else '10'}\b",
            r"ccont_rtc: event=status_ack data=a1 old=b1\b",
            r"buzzer: enabled=1 divider=\d+ frequency=\d+ volume=[1-9]\d*\b",
            r"6250_alarm_physical: event=recurrence_observed\b",
            r"6250_alarm_physical: action=stop\b",
        ):
            match = re.search(pattern, text[cursor:])
            if not match:
                raise ValueError("missing/out-of-order NHM-3 Snooze boundary: " + pattern)
            cursor += match.end()
    if power_choice:
        from tools.power_domain_contract import require_endpoint_silence

        off = list(re.finditer(r"ccont_power: event=off\b", text))
        wakes = list(re.finditer(r"ccont_power: event=wake cause=([0-9a-f]+)\b", text))
        activation = text.index("6250_alarm_physical: action=activate_" + power_choice)
        wake_count = 2 if snooze else 1
        if (len(off) != wake_count + (power_choice == "no") or
                len(wakes) != wake_count or any(wake[1] != "80" for wake in wakes)):
            raise ValueError("NHM-3 alarm RTC wake count or selected rail-off outcome differs")
        if not off[0].end() < wakes[0].start() < activation:
            raise ValueError("NHM-3 alarm power boundaries out of order")
        require_endpoint_silence(text[off[0].end():wakes[0].start()], "NHM-3 powered-off alarm interval")
        if snooze:
            snoozed = text.index("6250_alarm_physical: action=snooze")
            if not wakes[0].end() < snoozed < off[1].start() < wakes[1].start() < activation:
                raise ValueError("NHM-3 powered-off Snooze boundaries out of order")
            require_endpoint_silence(text[off[1].end():wakes[1].start()],
                                    "NHM-3 powered-off Snooze interval")
        if power_choice == "no":
            if off[-1].start() < activation:
                raise ValueError("NHM-3 declined activation powered off before the choice")
            require_endpoint_silence(text[off[-1].end():], "NHM-3 declined activation interval")


def check_alarm_seed(text):
    expected = ["menu", *[f"down_{i}" for i in range(1, 10)], "clock",
                "clock_down_1", "alarm", *[f"time_{i}" for i in range(1, 5)],
                "confirm", "idle"]
    actions = re.findall(r"6250_alarm_physical: action=(\w+)\b", text)
    if "[LUA ERROR]" in text or actions != expected:
        raise ValueError("cold alarm seed physical sequence differs")
    prefix = text.split("6250_alarm_physical: action=idle", 1)[0]
    programming = re.search(
        r"action=confirm\b.*?alarm_write reg=0b data=30 armed=1\b.*?"
        r"alarm_write reg=0c data=0d armed=1\b", prefix, re.S)
    if programming is None:
        raise ValueError("cold alarm seed did not arm after confirmation")
    if "event=second time=13:48:00" in text or "event=stopped_presented" in text:
        raise ValueError("cold alarm seed expired or was dismissed")


def check_cold_alarm(text):
    if "[LUA ERROR]" in text or "6250_alarm_physical:" in text:
        raise ValueError("cold alarm must not replay arming inputs")
    if re.findall(r"6250_alarm_cold: action=(\w+)", text) != ["stop"]:
        raise ValueError("cold alarm requires exactly one physical Stop")
    cursor = 0
    for pattern in (
        r"ccont_rtc: event=alarm_write reg=0b data=30 armed=1\b",
        r"ccont_rtc: event=alarm_write reg=0c data=0d armed=1\b",
        r"6250_alarm_cold: event=armed_observed\b",
        r"ccont_rtc: event=second time=13:48:00 day=0 status=b1 mask=30\b",
        r"ccont_rtc: event=status_ack data=81 old=b1\b",
        r"buzzer: enabled=1 divider=\d+ frequency=\d+ volume=[1-9]\d*\b",
        r"6250_alarm_cold: action=stop\b",
        r"buzzer: enabled=0 divider=0 frequency=0\b",
        r"6250_alarm_cold: event=stopped_observed\b",
    ):
        match = re.search(pattern, text[cursor:])
        if not match:
            raise ValueError("missing/out-of-order NHM-3 cold alarm boundary: " + pattern)
        cursor += match.end()
    if "buzzer: enabled=1" in text[cursor:]:
        raise ValueError("cold alarm buzzer resumed after Stop")


def check_restore(text):
    from tools.power_domain_contract import require_endpoint_silence
    states = list(re.finditer(
        r"6250_alarm_state: event=(saved|restored) pc=([0-9a-f]{8}) sp=([0-9a-f]{8}) "
        r"ram=([0-9a-f]{8}) cpu=([0-9a-f,]+) t=([0-9.]+)", text))
    if len(states) != 2 or [state[1] for state in states] != ["saved", "restored"]:
        raise ValueError("missing unique NHM-3 alarm save/load observations")
    if any(not re.fullmatch(r"[0-9a-f]{8}(?:,[0-9a-f]{8}){36}", state[5]) for state in states):
        raise ValueError("incomplete NHM-3 ARM/banked snapshot")
    if states[0].groups()[1:] != states[1].groups()[1:] or float(states[0][6]) != 49:
        raise ValueError("NHM-3 restored architecture/checkpoint differs")
    windows = list(re.finditer(
        r"6250_alarm_replay: phase=(reference|restored) event=(begin|end) t=([0-9.]+)", text))
    if [(event[1], event[2]) for event in windows] != [
            ("reference", "begin"), ("reference", "end"),
            ("restored", "begin"), ("restored", "end")]:
        raise ValueError("NHM-3 alarm replay windows absent/unordered")
    if [float(event[3]) for event in windows] != [49, 50.25, 49, 50.25]:
        raise ValueError("NHM-3 alarm replay times differ")
    reference = text[windows[0].end():windows[1].start()]
    restored = text[windows[2].end():windows[3].start()]
    ticks = re.findall(r"ccont_rtc: event=second[^\r\n]+", reference)
    if len(ticks) != 1 or ticks != re.findall(r"ccont_rtc: event=second[^\r\n]+", restored):
        raise ValueError("NHM-3 powered-off RTC replay differs/absent")
    require_endpoint_silence(reference + restored, "NHM-3 off-state replay")
    if "ccont_power: event=wake" in reference + restored:
        raise ValueError("NHM-3 replay woke before deadline")
    return text[:states[0].start()] + text[states[1].start():]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_directory", type=Path)
    parser.add_argument("--mame", type=Path)
    parser.add_argument("--power-choice", choices=("yes", "no"),
                        help="power off before expiry, then physically choose activation")
    parser.add_argument("--snooze", action="store_true")
    parser.add_argument("--restore-off", action="store_true")
    parser.add_argument("--cold", action="store_true")
    args = parser.parse_args()
    if args.restore_off and not args.power_choice:
        parser.error("off-state restoration requires an explicit activation choice")
    if args.cold and (args.snooze or args.restore_off or args.power_choice):
        parser.error("cold alarm is an independent awake lifecycle")
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
                   str(root / "tools" / ("noki6250_alarm_restore.lua" if args.restore_off
                                         else "noki6250_alarm_input.lua")), "-autoboot_delay", "0",
                   "-seconds_to_run", "40" if args.cold else "430" if args.snooze and args.power_choice else "395" if args.snooze else "110" if args.power_choice else "80", "-video", "none", "-sound", "none",
                   "-nothrottle", "-log", "-verbose"]
        environment = os.environ.copy()
        for key in ("NOKIA_DCT3_6250_ALARM_POWER_OFF", "NOKIA_DCT3_6250_ALARM_POWER_CHOICE",
                    "NOKIA_DCT3_6250_ALARM_SNOOZE"):
            environment.pop(key, None)
        if args.power_choice:
            environment.update(NOKIA_DCT3_6250_ALARM_POWER_OFF="1",
                               NOKIA_DCT3_6250_ALARM_POWER_CHOICE=args.power_choice)
        if args.snooze:
            environment["NOKIA_DCT3_6250_ALARM_SNOOZE"] = "1"
        with (phase / "console.log").open("w") as console:
            subprocess.run(command, cwd=phase, env=environment, stdout=console,
                           stderr=subprocess.STDOUT, check=True)
        text = (phase / "error.log").read_text(errors="replace")
        if args.cold:
            from tools.noki6250_staged_check import check as check_uploads
            from tools.radio_registration_trace_check import verify
            check_alarm_seed(text)
            check_uploads(text, runtime=True)
            verify(text, "nhm3", preserved=True)
            check_frame(phase / "snap/6250_alarm_confirm.png", FRAMES["confirm"])
            check_frame(phase / "snap/6250_alarm_idle.png", FRAMES["idle"])
            cold = run / "cold"
            prepare_run(cold, root)
            for name in ("nvram", "cfg"):
                shutil.copytree(phase / name, cold / name, dirs_exist_ok=True)
            command[command.index("-rompath") + 1] = f"{cold / 'roms'};{root / 'roms'}"
            command[command.index("-autoboot_script") + 1] = str(root / "tools/noki6250_alarm_cold.lua")
            command[command.index("-seconds_to_run") + 1] = "80"
            with (cold / "console.log").open("w") as console:
                subprocess.run(command, cwd=cold, env=environment, stdout=console,
                               stderr=subprocess.STDOUT, check=True)
            retained = (cold / "error.log").read_text(errors="replace")
            check_cold_alarm(retained)
            check_uploads(retained, runtime=True)
            verify(retained, "nhm3", preserved=True)
            for name, digest in (("armed", FRAMES["idle"]), ("elapsed", FRAMES["elapsed"]),
                                 ("stopped", FRAMES["stopped"])):
                check_frame(cold / "snap" / f"6250_alarm_cold_{name}.png", digest)
            print("6250 research-HLE retained cold alarm and physical Stop PASS; native/audio not tested")
            return
        if args.restore_off:
            text = check_restore(text)
            for name in ("reference", "restored"):
                check_frame(phase / "snap" / f"6250_alarm_off_{name}.png",
                            "907c2e3cc0dc7d0dc17827521badb7be0f647b6b945f1bac2e68094fd47568a7")
        elif "6250_alarm_state:" in text or "6250_alarm_replay:" in text:
            raise ValueError("unexpected state replay in uninterrupted alarm run")
        check_alarm(text, args.power_choice, args.snooze)
        frames = dict(FRAMES)
        if args.snooze:
            frames.update(
                snoozed=("907c2e3cc0dc7d0dc17827521badb7be0f647b6b945f1bac2e68094fd47568a7"
                         if args.power_choice else
                         "360ff6d56fc11b9b4447678eb0e11f6568d7171ea3c611633137f9f14c785b76"),
                recurred=("1d07bdbc7c561fe1ea613620416b9e1daa7fab88f96961c64499346b20cb3aff"
                          if args.power_choice else
                          "a17ae41b186da6b422d94484b171c0766749f27c4ce0c1747a5a49b24e6d7c18"))
        if args.power_choice:
            from tools.noki6250_staged_check import check as check_uploads
            from tools.radio_registration_trace_check import verify

            off = text.index("ccont_power: event=off")
            wake = text.index("ccont_power: event=wake cause=80")
            activation = text.index("6250_alarm_physical: action=activate_" + args.power_choice)
            check_uploads(text[:off], runtime=True)
            if args.snooze:
                wakes = list(re.finditer(r"ccont_power: event=wake cause=80\b", text))
                snooze_off = text.index("ccont_power: event=off", wakes[0].end())
                check_uploads(text[wakes[0].start():snooze_off], runtime=True)
                check_uploads(text[wakes[1].start():activation], runtime=True)
            else:
                check_uploads(text[wake:activation], runtime=True)
            verify(text[:off], "nhm3", preserved=True)
            if "LAPDm Location Updating Accept acknowledged" in text[wake:activation]:
                raise ValueError("alarm-only wake unexpectedly registered before activation")
            if args.power_choice == "yes":
                verify(text[activation:], "nhm3", preserved=True)
            elif "LAPDm Location Updating Accept acknowledged" in text[activation:]:
                raise ValueError("declined activation unexpectedly registered")
            frames.update(
                off="907c2e3cc0dc7d0dc17827521badb7be0f647b6b945f1bac2e68094fd47568a7",
                elapsed="06480ad9a6db38ed78e22108fd63272d7992765161fe11ab8bea65a18c98ab33",
                stopped="66e40d0bd8e655b0ac6400b6e83c1acd01c58b0247d51ed4f341ccf6f7cab615",
                choice=(FRAMES["stopped"] if args.power_choice == "yes" else
                        "907c2e3cc0dc7d0dc17827521badb7be0f647b6b945f1bac2e68094fd47568a7"))
        for name, digest in frames.items():
            check_frame(phase / "snap" / f"6250_alarm_{name}.png", digest)
        if not args.power_choice:
            subprocess.run([sys.executable, str(root / "tools/radio_registration_trace_check.py"),
                        str(phase / "error.log"), "--profile", "nhm3", "--preserved"], check=True)
        print("6250 research-HLE physical alarm PASS; activation=" +
              (args.power_choice or ("awake Snooze/Stop" if args.snooze else "awake Stop")) +
              "; native speech/audio output not tested")
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"6250 alarm failed: {error}\n")


if __name__ == "__main__":
    main()
