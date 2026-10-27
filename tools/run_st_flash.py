"""Run the independent ST M28W320ECT command-engine conformance fixture."""
import argparse
import json
from pathlib import Path
import subprocess

PASS = "ST flash engine: PASS checks=120 blocks=71 status_anywhere=1 sticky_errors=1 deferred_program=1 deferred_erase=1 suspend_resume=1 nested_program=1 restore=4 lockdown_wp=1 double_word=1 reset_abort=2 physical_cell_claim=0 handset_claim=0"


def check(text, returncode):
    if returncode or text.splitlines().count(PASS) != 1 or "Fatal error:" in text:
        raise ValueError("missing unique completed ST flash result")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_directory", type=Path)
    parser.add_argument("--mame", required=True, type=Path)
    args = parser.parse_args()
    run = args.run_directory.resolve()
    try:
        run.mkdir(parents=True, exist_ok=False)
        command = [str(args.mame.resolve()), "sttest", "-noreadconfig",
                   "-video", "none", "-sound", "none", "-nothrottle",
                   "-midiprovider", "none", "-nvram_directory", "nvram",
                   "-cfg_directory", "cfg", "-seconds_to_run", "8"]
        with (run / "console.log").open("w") as output:
            result = subprocess.run(command, cwd=run, stdout=output,
                                    stderr=subprocess.STDOUT, timeout=30)
        check((run / "console.log").read_text(), result.returncode)
        (run / "acceptance.json").write_text(json.dumps({
            "result": "pass", "command": command, "checks": 120,
            "blocks": 71, "fixture_class": "flash command conformance",
            "physical_cell_claim": False, "handset_claim": False,
        }, indent=2) + "\n")
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f"ST flash FAIL: {error}; inspect {run}\n")
    print(PASS)


if __name__ == "__main__":
    main()
