"""Check pinned NPE-3 key tables and executable keypad-controller wiring."""

import argparse
import hashlib
from pathlib import Path
import subprocess


FLASH_SHA1 = "3d9ea319503e78ec69b60d72cda23e461e118ea9"
NORMAL = bytes.fromhex("5a 5a 5a 5a 5a 11 19 01 02 03 0e 17 04 05 06 0f 18 07 08 09 10 1a 0c 0a 0b")
SPECIAL = bytes.fromhex("5a 5a 5a 5a 0d")


def check_tables(image):
    if hashlib.sha1(image).hexdigest() != FLASH_SHA1:
        raise ValueError("not the pinned NPE-3 v5.56 flash")
    if image[0x869b8:0x869b8 + 25] != NORMAL:
        raise ValueError("unexpected normal key table")
    if image[0x869d4:0x869d4 + 5] != SPECIAL:
        raise ValueError("unexpected special key table")


def check_trace(trace, returncode):
    if returncode or "npe3_keypad: PASS matrix_keys=20 scans=100 power_mask=10" not in trace:
        raise ValueError("missing complete keypad controller conformance")
    if "[LUA ERROR]" in trace or "Error in" in trace or "assertion failed" in trace:
        raise ValueError("Lua conformance error")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("rompath", type=Path)
    parser.add_argument("run_dir", type=Path)
    args = parser.parse_args()
    try:
        binary, rompath, run = args.binary.resolve(), args.rompath.resolve(), args.run_dir.resolve()
        check_tables((rompath / "noki6210/6210_556c.fls").read_bytes())
        run.mkdir(parents=True, exist_ok=True)
        log = run / "error.log"
        log.unlink(missing_ok=True)
        fixture = Path(__file__).with_name("npe3_keypad_conformance.lua").resolve()
        result = subprocess.run([
            str(binary), "noki6210", "-bios", "556", "-rompath", str(rompath),
            "-nvram_directory", str(run / "nvram"), "-noreadconfig", "-video", "none",
            "-sound", "none", "-nothrottle", "-seconds_to_run", "1", "-skip_gameinfo",
            "-autoboot_script", str(fixture), "-autoboot_delay", "0", "-log"],
            cwd=run, capture_output=True, text=True, timeout=30)
        output = result.stdout + result.stderr
        (run / "keypad_output.log").write_text(output)
        check_trace(log.read_text() + output, result.returncode)
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"NPE-3 keypad gate failed: {error}\n")
    print("NPE-3 keypad PASS: pinned tables, 20 physical matrix inputs/100 scans, Power press/release; firmware handling not yet validated")


if __name__ == "__main__":
    main()
