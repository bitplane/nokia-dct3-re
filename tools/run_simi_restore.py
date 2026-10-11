"""Run isolated SIMI/card restoration and APDU-header conformance checks."""
import argparse
import json
from pathlib import Path
import subprocess

PASS = "SIMI restoration: PASS checkpoints=3 partial_apdu=1 pending_rx=1 security_attempts=1 exact_irq_order=1 exact_deadlines=1 handset_claim=0"
HEADER_PASS = "SIM card header conformance: PASS unsupported_classes=3 empty_verify=1 retry_preservation=1 following_apdu=1 handset_claim=0"
PARAMETER_PASS = "SIM card parameter conformance: PASS reserved_parameters=6 select_lengths=3 state_preservation=1 following_apdu=1 handset_claim=0"


def check(text, returncode):
    if returncode != 0 or any(text.splitlines().count(marker) != 1
                              for marker in (PASS, HEADER_PASS, PARAMETER_PASS)):
        raise ValueError("missing unique completed restoration or APDU-header result")
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
            "result": "pass", "command": command, "checkpoints": 3,
            "unsupported_classes": 3, "empty_verify": True,
            "retry_preservation": True, "following_apdu": True,
            "reserved_parameters": 6, "select_lengths": 3,
            "fixture_class": "controller/card conformance", "handset_claim": False,
        }, indent=2) + "\n")
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f"SIMI restoration FAIL: {error}; inspect {run}\n")
    print(PASS)
    print(HEADER_PASS)
    print(PARAMETER_PASS)


if __name__ == "__main__":
    main()
