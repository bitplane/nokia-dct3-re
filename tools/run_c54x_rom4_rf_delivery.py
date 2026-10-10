#!/usr/bin/env python3
"""Run fresh native RF controls and task-delivery checks; no acquisition claim."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

try:
    from tools.c54x_rom4_rf_delivery_check import check as check_delivery, check_request, check_tuning_upload, check_tuning_blob
    from tools.c54x_rom4_rf_delivery_check import nse1_gsm900_tuning_consistency
    from tools.c54x_rom4_rf_score_check import check as check_score
    from tools.c54x_rom4_rf_consumption_check import check as check_consumption
except ModuleNotFoundError:
    from c54x_rom4_rf_delivery_check import check as check_delivery, check_request, check_tuning_upload, check_tuning_blob
    from c54x_rom4_rf_delivery_check import nse1_gsm900_tuning_consistency
    from c54x_rom4_rf_score_check import check as check_score
    from c54x_rom4_rf_consumption_check import check as check_consumption


def check_repeats(observations):
    baselines = {}
    for case in observations:
        fingerprint = tuple(case[key] for key in ("results", "accepted", "object", "native_body"))
        baseline = baselines.setdefault(case["mode"], fingerprint)
        if fingerprint != baseline:
            raise ValueError(f"fresh {case['mode']} controls differ: {baseline} versus {fingerprint}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--rompath", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, default=Path("/tmp"))
    parser.add_argument("--repeats", type=int, default=2)
    args = parser.parse_args()
    if args.repeats < 1:
        parser.error("--repeats must be positive")
    root = Path(__file__).resolve().parent.parent
    run = Path(tempfile.mkdtemp(prefix="run_rom4_rf_delivery_", dir=args.output_root))
    print(f"Native RF delivery evidence: {run}", flush=True)
    observations = []
    for mode in ("zero", "tone"):
        for repeat in range(args.repeats):
            case = run / f"{mode}_{repeat}"
            nvram = case / "nvram" / "noki5110"
            nvram.mkdir(parents=True)
            subprocess.run([sys.executable, str(root / "tools/make_5110_eeprom_profile.py"),
                            "--eeprom", str(root / "roms/noki5110/nse-1.bin"),
                            "--flash", str(root / "roms/noki5110/5110f530.fls"),
                            "--output", str(nvram / "eeprom")],
                           check=True, stdout=subprocess.DEVNULL)
            command = [str(args.mame.resolve()), "noki5110", "-rompath", str(args.rompath.resolve()),
                       "-nvram_directory", str(case / "nvram"), "-video", "none", "-sound", "none",
                       "-log", "-skip_gameinfo", "-nothrottle", "-seconds_to_run", "60",
                       "-autoboot_script", str(root / "tools/c54x_rom4_rf_score.lua")]
            with (case / "console.log").open("w") as console:
                subprocess.run(command, cwd=case, env=dict(os.environ, ROM4_RF_SCORE_MODE=mode),
                               stdout=console, stderr=subprocess.STDOUT, check=True)
            text = (case / "error.log").read_text()
            actual_mode, results, accepted = check_score(text)
            consumption = check_consumption(text)
            scan_consumption = check_consumption(text, scan=True)
            object_address, body = check_delivery(text)
            tuning = nse1_gsm900_tuning_consistency(check_tuning_upload(text))
            check_tuning_blob(text)
            if check_request(text) != int(body, 16):
                raise ValueError("response body differs from selected request entry")
            if tuning["channel"] != int(body, 16):
                raise ValueError("tuning inference differs from requested entry")
            if actual_mode != mode:
                raise ValueError("RF stimulus mode differs from requested case")
            observations.append(dict(mode=mode, repeat=repeat, results=results,
                                     accepted=accepted, object=object_address, native_body=body,
                                     consumption=consumption,
                                     scan_consumption=scan_consumption,
                                     tuning_consistency=tuning))
            print(f"Native RF delivery: PASS mode={mode} repeat={repeat} "
                  f"results={results} accepted={accepted} body={body}", flush=True)
    (run / "summary.json").write_text(json.dumps(dict(cases=observations,
        fresh_storage=True, acquisition_claim=False), indent=2) + "\n")
    check_repeats(observations)
    print(f"ROM4 RF cold controls: PASS cases={len(observations)} acquisition_claim=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
