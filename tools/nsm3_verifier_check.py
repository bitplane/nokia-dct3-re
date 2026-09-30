"""Execute the stock NSM-3 staged verifier and check its peripheral frontier."""

import argparse
from pathlib import Path
import re
import shutil
import subprocess

try:
    from tools.extract_nsm3_verifier import extract
except ModuleNotFoundError:
    from extract_nsm3_verifier import extract


def check(output, trace, returncode):
    if returncode != 1 or "port=002d pc=0f9f blocks=116" not in output:
        raise ValueError("missing fail-closed NSM-3 peripheral-read frontier")
    blocks = [int(value) for value in re.findall(r"nsm3_verifier: block=(\d+) flag=", trace)]
    if blocks != list(range(116)):
        raise ValueError("staged verifier did not consume all 116 ordered blocks")
    writes = re.findall(r"port_write=([0-9a-f]+) data=([0-9a-f]+) blocks=(\d+)", trace)
    if writes != [("000e", "1387", "116"), ("0000", "000d", "116"),
                  ("000c", "0010", "116")]:
        raise ValueError("unexpected verifier peripheral-write sequence")


def check_cobba(output, trace, returncode, expected, version=6):
    publication = re.search(
        rf"publication: blocks=116 word0=([0-9a-f]{{4}}) word1={version:04x} word2={version:04x} word3=0006 pc=0f64", output)
    if returncode != 3 or not publication or int(publication[1], 16) != expected:
        raise ValueError("staged verifier did not publish the supplied COBBA register-F value")
    blocks = [int(value) for value in re.findall(r"nsm3_verifier: block=(\d+) flag=", trace)]
    if blocks != list(range(116)):
        raise ValueError("COBBA comparison did not consume all ordered blocks")
    selects = re.findall(r"port_write=002c data=([0-9a-f]+) blocks=116", trace)
    if selects != ["001f", "001d", "001f"]:
        raise ValueError("unexpected COBBA register-F/status handshake")
    values = re.findall(r"port_read=002d data=([0-9a-f]+) blocks=116", trace)
    if values != [f"{expected:04x}"] * 3 + ["000c", "000c", f"{expected:04x}"]:
        raise ValueError("unexpected COBBA read sequence")
    if re.findall(r"immutable_version_write=([0-9a-f]+)", trace) != ["0006"]:
        raise ValueError("missing immutable-PROM write probe")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("flash", type=Path)
    parser.add_argument("run_dir", type=Path)
    args = parser.parse_args()
    try:
        binary, flash, run = args.binary.resolve(), args.flash.resolve(), args.run_dir.resolve()
        rom_dir = run / "nsm3verify"
        rom_dir.mkdir(parents=True, exist_ok=True)
        (rom_dir / "nsm3_verifier.bin").write_bytes(extract(flash.read_bytes()))
        shutil.copyfile(flash, rom_dir / "8210_5.31ppm_c.fls")
        log = run / "error.log"
        log.unlink(missing_ok=True)
        result = subprocess.run([
            str(binary), "nsm3verify", "-rompath", str(run), "-noreadconfig",
            "-video", "none", "-sound", "none", "-nothrottle", "-seconds_to_run", "3",
            "-skip_gameinfo", "-window", "-log"], cwd=run, capture_output=True,
            text=True, timeout=30)
        output = result.stdout + result.stderr
        (run / "verifier_output.log").write_text(output)
        check(output, log.read_text(), result.returncode)
        for bios, expected, version in [("cobba", 0, 6), ("cobba_alt", 0x16, 6), ("rom4", 0, 4)]:
            comparison = run / bios
            comparison.mkdir(exist_ok=True)
            comparison_log = comparison / "error.log"
            comparison_log.unlink(missing_ok=True)
            result = subprocess.run([
                str(binary), "nsm3verify", "-bios", bios, "-rompath", str(run), "-noreadconfig",
                "-video", "none", "-sound", "none", "-nothrottle", "-seconds_to_run", "3",
                "-skip_gameinfo", "-window", "-log"], cwd=comparison, capture_output=True,
                text=True, timeout=30)
            output = result.stdout + result.stderr
            (comparison / "verifier_output.log").write_text(output)
            check_cobba(output, comparison_log.read_text(), result.returncode, expected, version)
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"NSM-3 verifier gate failed: {error}\n")
    print("NSM-3 verifier PASS: completion follows immutable PROM version; word0 follows COBBA register F")


if __name__ == "__main__":
    main()
