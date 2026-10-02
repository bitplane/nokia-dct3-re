"""Run the explicitly unproved NSE-5/NSE-1 ROM4 compatibility composition."""

import argparse
import hashlib
import json
import os
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


def task2_queue_observation(trace):
    """Describe captured queue state, without making a stalled boot a gate."""
    posts = []
    for match in re.finditer(
            r"nse5_compat_task2_queue: primitive=([0-9a-f]{2}) detail=([0-9a-f]{2}) "
            r"producer=([0-9a-f]{2}) consumer=([0-9a-f]{2}) capacity=([0-9a-f]{2}) t=([\d.]+)", trace):
        primitive, detail, producer, consumer, capacity = (
            int(value, 16) for value in match.groups()[:5])
        if capacity < 2 or producer >= capacity or consumer >= capacity:
            raise ValueError("invalid captured task-2 queue geometry")
        posts.append({"primitive": primitive, "detail": detail,
                      "producer": producer, "consumer": consumer,
                      "capacity": capacity, "time": float(match[6]),
                      "full": (producer + 1) % capacity == consumer})
    failures = [{"primitive": int(match[1], 16), "detail": int(match[2], 16),
                 "time": float(match[3])} for match in re.finditer(
        r"nse5_compat_task2_queue_failure: primitive=([0-9a-f]{2}) "
        r"detail=([0-9a-f]{2}) t=([\d.]+)", trace)]
    return {"capture_limit_per_tap": 64, "posts": posts, "failures": failures}


def check_trace(trace, returncode):
    if returncode or "[LUA ERROR]" in trace:
        raise ValueError(f"compatibility execution failed (returncode={returncode})")
    samples = re.findall(r"nse5_compat_sample: t=([\d.]+) pc=([0-9a-f]+) result0=([0-9a-f]+) result1=([0-9a-f]+)", trace)
    if len(samples) != 6 or float(samples[-1][0]) < 8:
        raise ValueError("missing complete observation window")
    summary = re.search(r"rom4_interface_summary: completion_strobes=(\d+) mailbox_writes=(\d+)", trace)
    if not summary or int(summary[1]) == 0 or int(summary[2]) < 228:
        raise ValueError("DSP execution did not complete the upload boundary")
    if int(samples[-1][3], 16) != 4:
        raise ValueError("executed mask did not publish its ROM4 version")
    # A silent Lua tap callback failure must not masquerade as absence.
    for name in ("verifier", "service_init"):
        count = re.search(rf"nse5_compat_entries: name={name} count=(\d+)", trace)
        details = re.findall(rf"nse5_compat_entry: name={name} pc=[0-9a-f]{{8}} "
                             r"r0=[0-9a-f]{8} r1=[0-9a-f]{8} lr=[0-9a-f]{8} t=[\d.]+", trace)
        if not count or int(count[1]) < 1 or len(details) != min(int(count[1]), 12):
            raise ValueError(f"missing validated positive-control entry trace: {name}")
    return samples


def program_upload_observation(trace):
    """Compare captured upload writes with later reads, without inferring silicon."""
    writes, comparisons = {}, []
    for line in trace.splitlines():
        write = re.search(r"nse5_compat_dsp_upper_program_upload: address=([0-9a-f]{4}) value=([0-9a-f]{4})", line)
        read = re.search(r"nse5_compat_dsp_live_word: address=([0-9a-f]{4}) word=([0-9a-f]{4})", line)
        if write:
            writes[int(write[1], 16)] = int(write[2], 16)
        elif read and int(read[1], 16) in writes:
            address, value = int(read[1], 16), int(read[2], 16)
            comparisons.append({"address": address, "written": writes[address],
                                "read": value, "matches": writes[address] == value})
    return {"captured_write_addresses": len(writes),
            "compared_reads": comparisons,
            "mismatches": [row for row in comparisons if not row["matches"]]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("roms", type=Path)
    parser.add_argument("run_dir", type=Path)
    parser.add_argument("--menu", action="store_true", help="press/release the physical Menu switch after startup")
    parser.add_argument("--verbose", action="store_true", help="include device-boundary transport traces")
    parser.add_argument("--dsp-tail", action="store_true", help="capture DSP helper/low-program entry tails and return-stack writes")
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
        env = os.environ.copy()
        env["NSE5_COMPAT_MENU"] = "1" if args.menu else "0"
        env["NSE5_COMPAT_DSP_TAIL"] = "1" if args.dsp_tail else "0"
        result = subprocess.run([
            str(args.binary.resolve()), "nse5r4t", "-rompath", str(romdir.parent),
            "-nvram_directory", str(run / "nvram"), "-cfg_directory", str(run / "cfg"),
            "-snapshot_directory", str(run), "-noreadconfig", "-video", "none",
            "-sound", "none", "-nothrottle", "-seconds_to_run", "9", "-skip_gameinfo",
            "-log", "-autoboot_delay", "0", "-autoboot_script",
            str(Path(__file__).with_name("nse5_rom4_compat.lua").resolve()),
            *(["-verbose"] if args.verbose else [])],
            cwd=run, env=env, capture_output=True, text=True, timeout=120)
        output = result.stdout + result.stderr
        (run / "compat_output.log").write_text(output)
        trace = log.read_text()
        samples = check_trace(trace + output, result.returncode)
        (run / "startup_queue.json").write_text(
            json.dumps(task2_queue_observation(trace), indent=2) + "\n")
        if args.dsp_tail:
            (run / "program_upload.json").write_text(
                json.dumps(program_upload_observation(trace), indent=2) + "\n")
        if args.menu:
            trace = log.read_text()
            if "nse5_compat_menu: pressed=1" not in trace or "nse5_compat_menu: pressed=0" not in trace:
                raise ValueError("physical Menu press/release did not execute")
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
