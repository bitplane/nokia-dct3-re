"""Run the explicitly unproved NSE-5/NSE-1 ROM4 compatibility composition."""

import argparse
import hashlib
from pathlib import Path
import re
import shutil
import subprocess

from PIL import Image


INPUTS = {
    "noki7110/7110f501_ppmc.fls": "53af8324919f455ba8199d2c05f7a921cfb811d5",
    "noki7110/7110 virgin eeprom 005fa000.fls": "8b4dd782fc9d1306268ba63124ee463ac646912b",
    "noki5110/nse1_rom4_dsp_program.bin": "a05a1e96a8c36ec5a47e1ea059d15afa54ca5739",
    "noki5110/nse1_rom4_dsp_data.bin": "024c7f970f4ef754d3e90471de48a167515f930d",
}


def check_trace(trace, returncode):
    if returncode or "[LUA ERROR]" in trace:
        raise ValueError("compatibility execution failed")
    samples = re.findall(r"nse5_compat_sample: t=([\d.]+) pc=([0-9a-f]+) result0=([0-9a-f]+) result1=([0-9a-f]+)", trace)
    if len(samples) != 6 or float(samples[-1][0]) < 8:
        raise ValueError("missing complete observation window")
    summary = re.search(r"rom4_interface_summary: completion_strobes=(\d+) mailbox_writes=(\d+)", trace)
    if not summary or int(summary[1]) == 0 or int(summary[2]) < 228:
        raise ValueError("DSP execution did not complete the upload boundary")
    if int(samples[-1][3], 16) != 4:
        raise ValueError("executed mask did not publish its ROM4 version")
    return samples


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("roms", type=Path)
    parser.add_argument("run_dir", type=Path)
    args = parser.parse_args()
    try:
        run = args.run_dir.resolve()
        romdir = run / "roms" / "nse5r4t"
        romdir.mkdir(parents=True, exist_ok=True)
        for name, sha1 in INPUTS.items():
            source = args.roms / name
            if hashlib.sha1(source.read_bytes()).hexdigest() != sha1:
                raise ValueError(f"unrecognized research input: {name}")
            shutil.copyfile(source, romdir / source.name)
        log = run / "error.log"
        log.unlink(missing_ok=True)
        # This fixture always starts from the acquired product-local PMM,
        # never state retained from an earlier compatibility experiment.
        nvram = run / "nvram" / "nse5r4t"
        if nvram.exists():
            shutil.rmtree(nvram)
        for frame in run.rglob("native_*.png"):
            frame.unlink()
        result = subprocess.run([
            str(args.binary.resolve()), "nse5r4t", "-rompath", str(romdir.parent),
            "-nvram_directory", str(run / "nvram"), "-cfg_directory", str(run / "cfg"),
            "-snapshot_directory", str(run), "-noreadconfig", "-video", "none",
            "-sound", "none", "-nothrottle", "-seconds_to_run", "9", "-skip_gameinfo",
            "-log", "-autoboot_delay", "0", "-autoboot_script",
            str(Path(__file__).with_name("nse5_rom4_compat.lua").resolve())],
            cwd=run, capture_output=True, text=True, timeout=120)
        output = result.stdout + result.stderr
        (run / "compat_output.log").write_text(output)
        samples = check_trace(log.read_text() + output, result.returncode)
        frames = list(run.rglob("native_*.png"))
        if len(frames) != 6:
            raise ValueError("missing six native LCD captures")
        for frame in frames:
            with Image.open(frame) as image:
                if image.size != (96, 65):
                    raise ValueError("unexpected native panel geometry")
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"NSE-5 compatibility gate failed: {error}\n")
    print(f"NSE-5 ROM4 compatibility PASS: final MCU PC={samples[-1][1]}; "
          "six native captures; fitted mask and graphical UI remain unproved")


if __name__ == "__main__":
    main()
