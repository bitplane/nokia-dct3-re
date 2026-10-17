#!/usr/bin/env python3
"""Run fresh native RF controls and task-delivery checks; no acquisition claim."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

try:
    from tools.c54x_rom4_rf_delivery_check import check as check_delivery, check_request, check_tuning_upload, check_tuning_blob
    from tools.c54x_rom4_rf_delivery_check import nse1_gsm900_tuning_consistency, check_observer_health
    from tools.c54x_rom4_rf_score_check import check as check_score, observe as observe_score, check_window
    from tools.c54x_rom4_rf_consumption_check import check as check_consumption
except ModuleNotFoundError:
    from c54x_rom4_rf_delivery_check import check as check_delivery, check_request, check_tuning_upload, check_tuning_blob
    from c54x_rom4_rf_delivery_check import nse1_gsm900_tuning_consistency, check_observer_health
    from c54x_rom4_rf_score_check import check as check_score, observe as observe_score, check_window
    from c54x_rom4_rf_consumption_check import check as check_consumption


def check_repeats(observations):
    baselines = {}
    for case in observations:
        fingerprint = tuple(case[key] for key in ("results", "accepted", "object", "native_body"))
        baseline = baselines.setdefault(case["mode"], fingerprint)
        if fingerprint != baseline:
            raise ValueError(f"fresh {case['mode']} controls differ: {baseline} versus {fingerprint}")


def input_provenance(paths):
    """Hash actual inputs without claiming source HEAD identifies a built binary."""
    result = {}
    for name, path in paths.items():
        path = Path(path).resolve()
        entry = dict(path=str(path), present=path.is_file())
        if entry['present']:
            with path.open('rb') as stream:
                entry['sha256'] = hashlib.file_digest(stream, 'sha256').hexdigest()
            entry['bytes'] = path.stat().st_size
        result[name] = entry
    return result


def inspect_case(text, console, mode):
    """Keep independent evidence even when an acceptance boundary fails."""
    report = dict(mode=mode, failures={}, checks={})

    def capture(name, operation):
        try:
            value = operation()
            report['checks'][name] = dict(status='pass', value=value)
            return value
        except ValueError as error:
            report['checks'][name] = dict(status='failed', error=str(error))
            report['failures'][name] = str(error)
            return None

    capture('observer_health', lambda: check_observer_health(console))
    score = capture('score_observation', lambda: observe_score(text))
    capture('window_observation', lambda: check_window(text))
    capture('score_acceptance', lambda: check_score(text))
    capture('consumption', lambda: check_consumption(text))
    capture('scan_consumption', lambda: check_consumption(text, scan=True))
    delivery = capture('delivery', lambda: check_delivery(text))
    request = capture('request', lambda: check_request(text))
    tuning = capture('tuning', lambda: nse1_gsm900_tuning_consistency(check_tuning_upload(text)))
    capture('tuning_blob', lambda: check_tuning_blob(text))
    if score is not None and score['mode'] != mode:
        report['failures']['mode'] = 'RF stimulus mode differs from requested case'
    if delivery is not None:
        object_address, body = delivery
        report.update(object=object_address, native_body=body)
        if request is not None and request != int(body, 16):
            report['failures']['request_correlation'] = 'response body differs from selected request entry'
        if tuning is not None and tuning['channel'] != int(body, 16):
            report['failures']['tuning_correlation'] = 'tuning inference differs from requested entry'
    if score is not None:
        report.update(score=score, results=score['results'], accepted=score['accepted'])
    report['status'] = 'failed' if report['failures'] else 'pass'
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--rompath", type=Path, required=True)
    parser.add_argument("--output-root", type=Path,
                        default=Path(__file__).resolve().parents[1] / 'tmp')
    parser.add_argument("--repeats", type=int, default=2)
    args = parser.parse_args()
    if args.repeats < 1:
        parser.error("--repeats must be positive")
    root = Path(__file__).resolve().parent.parent
    args.output_root.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix="run_rom4_rf_delivery_", dir=args.output_root))
    print(f"Native RF delivery evidence: {run}", flush=True)
    paths = dict(binary=args.mame, observer=root / 'tools/c54x_rom4_rf_score.lua',
                 generator=root / 'tools/make_5110_eeprom_profile.py')
    for name in ('nse1_rom4_dsp_program.bin', 'nse1_rom4_dsp_data.bin',
                 '5110f530.fls', 'nse-1.bin', 'nse1_rom4_boot.bin'):
        paths['rom:' + name] = args.rompath / 'noki5110' / name
    # The generator uses repo-local provisioning sources even with another ROM path.
    paths['provisioning_flash'] = root / 'roms/noki5110/5110f530.fls'
    paths['provisioning_eeprom'] = root / 'roms/noki5110/nse-1.bin'
    (run / 'inputs.json').write_text(json.dumps(dict(schema=1,
        inputs=input_provenance(paths), physical_iq_claim=False,
        acquisition_claim=False), indent=2) + '\n')
    observations = []
    summary = dict(schema=2, status='running', cases=observations,
                   fresh_storage=True, acquisition_claim=False)
    (run / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
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
            (case / 'invocation.json').write_text(json.dumps(dict(
                command=command, cwd=str(case.resolve()), mode=mode,
                generated_storage=input_provenance({'eeprom': nvram / 'eeprom'})),
                indent=2) + '\n')
            execution_errors = {}
            returncode = None
            with (case / "console.log").open("w") as console:
                try:
                    result = subprocess.run(command, cwd=case, env=dict(os.environ, ROM4_RF_SCORE_MODE=mode),
                                            stdout=console, stderr=subprocess.STDOUT)
                    returncode = result.returncode
                except OSError as error:
                    execution_errors['mame_launch'] = str(error)
            try:
                text = (case / "error.log").read_text()
            except OSError as error:
                text = ''
                execution_errors['trace_read'] = str(error)
            report = inspect_case(text, (case / 'console.log').read_text(), mode)
            report.update(repeat=repeat, returncode=returncode)
            report['failures'].update(execution_errors)
            if returncode:
                report['failures']['mame_exit'] = f'MAME exited with status {returncode}'
            if report['failures']:
                report['status'] = 'failed'
            observations.append(report)
            (case / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
            (run / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
            print(f"Native RF delivery: {report['status'].upper()} mode={mode} repeat={repeat} "
                  f"results={report.get('results')} accepted={report.get('accepted')} "
                  f"body={report.get('native_body')} failures={','.join(report['failures']) or 'none'}", flush=True)
    summary['status'] = 'failed' if any(case['failures'] for case in observations) else 'pass'
    try:
        check_repeats(observations)
    except (ValueError, KeyError) as error:
        summary.update(status='failed', repeat_error=str(error))
    (run / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    if summary['status'] != 'pass':
        print('ROM4 RF cold controls: FAIL; per-boundary evidence retained in summary.json', file=sys.stderr)
        return 1
    print(f"ROM4 RF cold controls: PASS cases={len(observations)} acquisition_claim=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
