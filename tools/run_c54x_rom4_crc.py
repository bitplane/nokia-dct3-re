#!/usr/bin/env python3
"""Run acquired ROM4 helper arithmetic, not a handset or speech fixture."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

PASS = "TMS320C54x ROM4 CRC helper: PASS cases=512 gsm_packing_claim=0 speech_claim=0"
COMPLETE = "Fatal error: TMS320C54x ROM4 CRC helper complete"


def check_result(text, returncode):
    lines = text.splitlines()
    if returncode != 3:
        raise ValueError(f"unexpected MAME exit {returncode}; expected fixture completion 3")
    if lines.count(PASS) != 1 or not lines or lines[-1] != COMPLETE:
        raise ValueError("missing unique result or terminal fixture completion")
    if any(line.startswith("Fatal error:") and line != COMPLETE for line in lines):
        raise ValueError("unexpected fatal error")
    if "WRONG CHECKSUMS" in text or "NOT FOUND" in text:
        raise ValueError("acquired ROM input failed validation")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--rompath", type=Path, required=True)
    args = parser.parse_args()
    run = Path(tempfile.mkdtemp(prefix="run_c54x_rom4_crc_", dir=Path.cwd()))
    command = [str(args.mame.resolve()), "tms54rom4", "-bios", "crc-helper",
               "-rompath", str(args.rompath.resolve()), "-video", "none",
               "-sound", "none", "-nothrottle", "-noreadconfig", "-log",
               "-seconds_to_run", "1"]
    print(f"ROM4 helper evidence: {run}", flush=True)
    with (run / "console.log").open("w") as output:
        result = subprocess.run(command, cwd=run, stdout=output, stderr=subprocess.STDOUT)
    text = (run / "console.log").read_text()
    print(text, end="")
    try:
        check_result(text, result.returncode)
    except ValueError as error:
        print(f"ROM4 helper: FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
