"""Check pinned NSE-5 key tables and executable physical matrix wiring."""

import argparse
import hashlib
from pathlib import Path
import subprocess


FLASH_SHA1 = "53af8324919f455ba8199d2c05f7a921cfb811d5"
NORMAL = bytes.fromhex("5a 0e 19 1a 0c 5a 0f 0a 0b 06 5a 01 12 04 07 5a 02 05 08 09 5a 03 5a 5a 5a")
SPECIAL = bytes.fromhex("5a 0d 5a 5a 5a")


def check_tables(image):
    if hashlib.sha1(image).hexdigest() != FLASH_SHA1:
        raise ValueError("not the pinned NSE-5 v5.01 PPM C flash")
    if image[0x8dcd8:0x8dcd8 + 25] != NORMAL:
        raise ValueError("unexpected NSE-5 normal key table")
    if image[0x8dcf4:0x8dcf4 + 5] != SPECIAL:
        raise ValueError("unexpected NSE-5 special key table")


def check_trace(trace, returncode):
    if returncode or "nse5_keypad: PASS matrix_keys=17 scans=85 power_mask=02" not in trace:
        raise ValueError("missing complete NSE-5 keypad controller conformance")
    if "[LUA ERROR]" in trace or "assertion failed" in trace:
        raise ValueError("Lua conformance error")
    if "nse5_roller: PASS positions=3 probes=18 restored_pairs=9 irq_delivery=unvalidated" not in trace:
        raise ValueError("missing NSE-5 physical roller contact probes")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("rompath", type=Path)
    parser.add_argument("run_dir", type=Path)
    args = parser.parse_args()
    try:
        binary, rompath, run = args.binary.resolve(), args.rompath.resolve(), args.run_dir.resolve()
        check_tables((rompath / "noki7110/7110f501_ppmc.fls").read_bytes())
        run.mkdir(parents=True, exist_ok=True)
        log = run / "error.log"
        log.unlink(missing_ok=True)
        result = subprocess.run([
            str(binary), "noki7110", "-bios", "501", "-rompath", str(rompath),
            "-nvram_directory", str(run / "nvram"), "-cfg_directory", str(run / "cfg"),
            "-noreadconfig", "-video", "none", "-sound", "none", "-nothrottle",
            "-seconds_to_run", "1", "-skip_gameinfo", "-log", "-autoboot_delay", "0",
            "-autoboot_script", str(Path(__file__).with_name("nse5_keypad_conformance.lua").resolve())],
            cwd=run, capture_output=True, text=True, timeout=30)
        output = result.stdout + result.stderr
        (run / "keypad_output.log").write_text(output)
        check_trace(log.read_text() + output, result.returncode)
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"NSE-5 keypad gate failed: {error}\n")
    print("NSE-5 inputs PASS: pinned tables, 17 matrix keys/85 scans, Power press/release, 18 roller probes; UI/roller IRQ handling not validated")


if __name__ == "__main__":
    main()
