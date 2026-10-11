#!/usr/bin/env python3
"""Run isolated native RF-port impulse cases; no acquisition claim."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile


COEFFICIENTS = (0x0387, 0x11a3, 0x2ad6, 0x4000,
                0x4000, 0x2ad6, 0x11a3, 0x0387)
INPUT = re.compile(r"rom4_rf_impulse_input: index=(\d+) pc=([0-9a-f]{4}) value=([0-9a-f]{4})")
OUTPUT = re.compile(r"rom4_rf_impulse_output: index=(\d+) pc=([0-9a-f]{4}) address=([0-9a-f]{4}) value=([0-9a-f]{4})")


def expected(index, amplitude):
    signed = amplitude if amplitude < 0x8000 else amplitude - 0x10000
    words = [0] * 48
    first = 2 * (index // 4) + 1 - index % 2
    phase = 1 - (index % 4) // 2
    for tap in range(4):
        product = signed * COEFFICIENTS[phase + tap * 2]
        words[first + tap * 2] = ((product + 0x8000) >> 16) & 0xffff
    return words


def check(text, index, amplitude):
    inputs = INPUT.findall(text)
    outputs = OUTPUT.findall(text)
    if len(inputs) != 32 or len(outputs) != 48:
        raise ValueError("missing complete input/output capture")
    for number, (position, pc, value) in enumerate(inputs):
        if int(position) != number or int(pc, 16) != (0x330c, 0x3312, 0x3318, 0x331e)[number % 4]:
            raise ValueError(f"input alignment changed at {number}")
        if int(value, 16) != (amplitude if number == index else 0):
            raise ValueError(f"input value changed at {number}")
    for number, ((position, pc, address, value), wanted) in enumerate(zip(outputs, expected(index, amplitude))):
        if int(position) != number or int(pc, 16) != 0x3322 + number % 2 or int(address, 16) != 0x1a48 + number % 16:
            raise ValueError(f"output alignment changed at {number}")
        if int(value, 16) != wanted:
            raise ValueError(f"output {number}: actual={value} expected={wanted:04x}")
    marker = f"rom4_rf_impulse_summary: impulse={index} amplitude={amplitude:04x}"
    if text.count(marker) != 1:
        raise ValueError("missing or duplicate case summary")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--rompath", type=Path, required=True)
    parser.add_argument("--output-root", type=Path,
                        default=Path(__file__).resolve().parents[1] / 'tmp')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    args.output_root.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix="run_rom4_rf_impulse_", dir=args.output_root))
    print(f"RF impulse evidence: {run}", flush=True)
    cases = 0
    for amplitude in (0x4000, 0xc000):
        for index in range(32):
            case = run / f"{amplitude:04x}_{index:02d}"
            nvram = case / "nvram" / "noki5110"
            nvram.mkdir(parents=True)
            subprocess.run([str(root / ".venv/bin/python"), str(root / "tools/make_5110_eeprom_profile.py"),
                            "--eeprom", str(root / "roms/noki5110/nse-1.bin"),
                            "--flash", str(root / "roms/noki5110/5110f530.fls"),
                            "--output", str(nvram / "eeprom")], check=True, stdout=subprocess.DEVNULL)
            env = dict(os.environ, ROM4_RF_IMPULSE_INDEX=str(index), ROM4_RF_IMPULSE_VALUE=str(amplitude),
                       ROM4_RF_IMPULSE_TIME="1.6402")
            command = [str(args.mame.resolve()), "noki5110", "-rompath", str(args.rompath.resolve()),
                       "-nvram_directory", str(case / "nvram"), "-video", "none", "-sound", "none",
                       "-log", "-skip_gameinfo", "-nothrottle", "-seconds_to_run", "8",
                       "-autoboot_script", str(root / "tools/c54x_rom4_rf_impulse.lua")]
            with (case / "console.log").open("w") as console:
                subprocess.run(command, cwd=case, env=env, stdout=console, stderr=subprocess.STDOUT, check=True)
            check((case / "error.log").read_text(), index, amplitude)
            cases += 1
            print(f"RF impulse case: PASS index={index} amplitude={amplitude:04x}", flush=True)
    print(f"ROM4 RF impulse: PASS cases={cases} physical_iq_claim=0 acquisition_claim=0")


if __name__ == "__main__":
    main()
