"""Run isolated SIMI/card save-state checkpoints without handset firmware."""
import argparse
import json
from pathlib import Path
import subprocess

PASS = "SIMI restoration: PASS checkpoints=2 partial_apdu=1 pending_rx=1 exact_irq_order=1 exact_deadlines=1 handset_claim=0"


def check(text, returncode):
    if returncode != 0 or text.splitlines().count(PASS) != 1:
        raise ValueError("missing unique completed controller/card restoration result")
    if "Fatal error:" in text or "Error reading NVRAM" in text:
        raise ValueError("runtime failure or invalid initial storage")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_directory", type=Path)
    parser.add_argument("--mame", type=Path, required=True)
    args = parser.parse_args()
    run = args.run_directory.resolve()
    try:
        run.mkdir(parents=True, exist_ok=False)
        command = [str(args.mame.resolve()), "simitest", "-noreadconfig",
                   "-nvram_directory", "nvram", "-cfg_directory", "cfg",
                   "-video", "none", "-sound", "none", "-nothrottle",
                   "-seconds_to_run", "1"]
        with (run / "console.log").open("w") as output:
            result = subprocess.run(command, cwd=run, stdout=output,
                                    stderr=subprocess.STDOUT, timeout=90)
        check((run / "console.log").read_text(), result.returncode)
        (run / "acceptance.json").write_text(json.dumps({
            "result": "pass", "command": command, "checkpoints": 2,
            "fixture_class": "controller/card conformance", "handset_claim": False,
        }, indent=2) + "\n")
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f"SIMI restoration FAIL: {error}; inspect {run}\n")
    print(PASS)


if __name__ == "__main__":
    main()
