"""Run the ROM-independent, byte-level SIM parameter-negotiation fixture."""
import argparse
import json
from pathlib import Path
import subprocess

PASS = "SIM PTS conformance: PASS cases=10 default_speed=1 unsupported_speed_fallback=2 checksum=1 reserved_fields=1 partial_restore=1 following_apdu=10 late_pts_class_error=1 deactivation_abort=1 enhanced_clock_claim=0 handset_claim=0"


def check(text, returncode):
    if returncode or text.splitlines().count(PASS) != 1 or "Fatal error:" in text:
        raise ValueError("missing unique completed SIM PTS result")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_directory", type=Path)
    parser.add_argument("--mame", required=True, type=Path)
    args = parser.parse_args()
    run = args.run_directory.resolve()
    try:
        run.mkdir(parents=True, exist_ok=False)
        command = [str(args.mame.resolve()), "simptst", "-noreadconfig",
                   "-video", "none", "-sound", "none", "-nothrottle",
                   "-nvram_directory", "nvram", "-cfg_directory", "cfg",
                   "-seconds_to_run", "1"]
        with (run / "console.log").open("w") as output:
            result = subprocess.run(command, cwd=run, stdout=output,
                                    stderr=subprocess.STDOUT, timeout=30)
        check((run / "console.log").read_text(), result.returncode)
        (run / "acceptance.json").write_text(json.dumps({
            "result": "pass", "command": command,
            "fixture_class": "card protocol conformance",
            "enhanced_clock_claim": False, "handset_claim": False,
        }, indent=2) + "\n")
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f"SIM PTS FAIL: {error}; inspect {run}\n")
    print(PASS)


if __name__ == "__main__":
    main()
