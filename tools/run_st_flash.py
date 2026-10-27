"""Run the independent ST M28W320ECT command-engine conformance fixture."""
import argparse
import json
from pathlib import Path
import subprocess

PASS = "ST flash engine: PASS checks=120 blocks=71 status_anywhere=1 sticky_errors=1 deferred_program=1 deferred_erase=1 suspend_resume=1 nested_program=1 restore=4 lockdown_wp=1 double_word=1 reset_abort=2 physical_cell_claim=0 handset_claim=0"
CFI_PASS = "ST flash CFI: PASS bytes=56 x16=1 capacity=4194304 regions=63x65536,8x8192 page_bytes=8 restore=1 array_unchanged=1 full_datasheet=1 handset_claim=0"
PAGE_PASS = "ST flash page: PASS words=4 deferred=1 arbitrary_order=1 opcode_data=1 restore=2 locked_rejection=1 malformed_rejection=2 parameter_erase_ms=400 physical_cell_claim=0 handset_claim=0"
RESULTS = (CFI_PASS, PASS, PAGE_PASS)


def check(text, returncode):
    lines = text.splitlines()
    if (returncode or any(lines.count(marker) != 1 for marker in RESULTS)
            or "Fatal error:" in text):
        raise ValueError("missing unique completed ST flash result")
    if [lines.index(marker) for marker in RESULTS] != sorted(lines.index(marker) for marker in RESULTS):
        raise ValueError("out-of-order ST flash results")


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
            "cfi_bytes": 56, "page_bytes": 8, "parameter_erase_ms": 400,
            "physical_cell_claim": False, "handset_claim": False,
        }, indent=2) + "\n")
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f"ST flash FAIL: {error}; inspect {run}\n")
    print("\n".join(RESULTS))


if __name__ == "__main__":
    main()
