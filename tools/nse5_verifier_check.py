"""Execute the 7110 verifier with recovered ROM4 code and explicit COBBA fixtures."""

import argparse
import hashlib
from pathlib import Path
import re
import shutil
import subprocess

try:
    from tools.extract_nsm3_verifier import extract
except ModuleNotFoundError:
    from extract_nsm3_verifier import extract


MASK_SHA1 = "a05a1e96a8c36ec5a47e1ea059d15afa54ca5739"


def check_blocks(trace):
    blocks = [int(value) for value in re.findall(r"nsm3_verifier: block=(\d+) flag=", trace)]
    if blocks != list(range(228)):
        raise ValueError("verifier did not consume all 228 ordered blocks")


def check_boundary(output, trace, returncode):
    check_blocks(trace)
    if returncode != 1 or not re.search(
            r"requires peripheral read: port=002d pc=[0-9a-f]{4} blocks=228", output):
        raise ValueError("missing fail-closed COBBA read boundary")


def check_publication(output, trace, returncode, expected):
    check_blocks(trace)
    match = re.search(
        r"publication: blocks=228 word0=([0-9a-f]{4}) word1=0004 word2=0004 "
        r"word3=0004 pc=0f6[ab] fingerprint=([0-9a-f]{8}) pmst=ffa8", output)
    if returncode != 3 or not match or int(match[1], 16) != expected:
        raise ValueError("publication does not follow supplied ROM4/COBBA inputs")
    if match[2] != "a98692ad":
        raise ValueError("unexpected stock-input ROM4 CRC fixture result")
    return match[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("flash", type=Path)
    parser.add_argument("mask", type=Path)
    parser.add_argument("run_dir", type=Path)
    args = parser.parse_args()
    try:
        binary, run = args.binary.resolve(), args.run_dir.resolve()
        mask = args.mask.read_bytes()
        if hashlib.sha1(mask).hexdigest() != MASK_SHA1:
            raise ValueError("not the pinned recovered NSE-1 ROM4 program")
        rom = run / "nse5verify"
        rom.mkdir(parents=True, exist_ok=True)
        (rom / "nse5_verifier.bin").write_bytes(extract(args.flash.read_bytes(), "7110"))
        shutil.copyfile(args.flash, rom / "7110f501_ppmc.fls")
        (rom / "dsp_full.bin").write_bytes(mask)
        fingerprints = []
        for bios, expected in (("boundary", None), ("cobba", 0), ("cobba_alt", 0x16)):
            work = run / bios
            work.mkdir(exist_ok=True)
            log = work / "error.log"
            log.unlink(missing_ok=True)
            result = subprocess.run([
                str(binary), "nse5verify", "-bios", bios, "-rompath", str(run),
                "-noreadconfig", "-video", "none", "-sound", "none", "-nothrottle",
                "-seconds_to_run", "3", "-skip_gameinfo", "-window", "-log"],
                cwd=work, capture_output=True, text=True, timeout=45)
            output = result.stdout + result.stderr
            (work / "verifier_output.log").write_text(output)
            if not log.is_file():
                raise ValueError(f"{bios} run produced no trace (exit {result.returncode}): {output.strip()}")
            trace = log.read_text()
            if expected is None:
                check_boundary(output, trace, result.returncode)
            else:
                fingerprints.append(check_publication(output, trace, result.returncode, expected))
        if fingerprints[0] != fingerprints[1]:
            raise ValueError("COBBA input unexpectedly changed the flash fingerprint")
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"NSE-5 verifier gate failed: {error}\n")
    print(f"NSE-5 verifier PASS: 228 blocks, ROM4 CRC={fingerprints[0]}; "
          "final publication remains conditional on the peripheral/memory-map fixture")


if __name__ == "__main__":
    main()
