"""Run executable SED1565 controller conformance, independent of phone firmware."""

import argparse
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("run_dir", type=Path)
    args = parser.parse_args()
    try:
        binary, run = args.binary.resolve(), args.run_dir.resolve()
        run.mkdir(parents=True, exist_ok=True)
        log = run / "error.log"
        log.unlink(missing_ok=True)
        result = subprocess.run(
            [str(binary), "sed1565t", "-video", "none", "-sound", "none",
             "-noreadconfig", "-skip_gameinfo", "-nothrottle", "-seconds_to_run", "1",
             "-log", "-cfg_directory", str(run / "cfg"),
             "-nvram_directory", str(run / "nvram")],
            cwd=run, capture_output=True, text=True, timeout=30)
        output = result.stdout + result.stderr
        (run / "output.log").write_text(output)
        if result.returncode or "sed1565_conformance: PASS checks=17" not in log.read_text():
            raise ValueError("missing complete controller conformance; inspect error.log/output.log")
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"SED1565 gate failed: {error}\n")
    print("SED1565 PASS: 17 executable serial/address/display/reset checks")


if __name__ == "__main__":
    main()
