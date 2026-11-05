#!/usr/bin/env python3
"""Time ROM4 EFR validation and, when available, first-execution frame spans.

Frame spans exclude reset, input preparation, save/replay and output comparison;
they include normal emulator scheduling. The process time retains all that work.
"""
import argparse
import json
import math
import os
from pathlib import Path
import re
import statistics
import subprocess
import sys
import time

try:
    from tools.run_c54x_rom4_crc import check_result, file_identity
except ModuleNotFoundError:
    from run_c54x_rom4_crc import check_result, file_identity


def parse_frames(text, count=163):
    matches = re.findall(r'^ROM4 codec frame timing: index=(\d+) host_seconds=([0-9.]+) replay_included=0$', text, re.M)
    if [int(index) for index, _ in matches] != list(range(count)):
        raise ValueError('missing, duplicate or unordered frame timings')
    values = [float(value) for _, value in matches]
    if not all(math.isfinite(value) and value > 0 for value in values):
        raise ValueError('invalid frame duration')
    return values


def frame_stats(values):
    ordered = sorted(values)
    if not ordered:
        raise ValueError('empty frame timings')
    return dict(frames=len(values), mean_ms=1000 * statistics.mean(values),
                p95_ms=1000 * ordered[math.ceil(len(values) * .95) - 1],
                p99_ms=1000 * ordered[math.ceil(len(values) * .99) - 1],
                worst_ms=1000 * ordered[-1], over_20ms=sum(value > .020 for value in values))


def paired_budget(encoder, decoder, handset_seconds_per_second=0):
    if len(encoder) != len(decoder) or any(len(e) != len(d) for e, d in zip(encoder, decoder)):
        raise ValueError('unpaired frame timings')
    pairs = [[e + d + .020 * handset_seconds_per_second for e, d in zip(enc, dec)]
             for enc, dec in zip(encoder, decoder)]
    return dict(scope='sum of separately measured frames; not concurrent/native handset duplex',
                handset_idle_seconds_per_second=handset_seconds_per_second,
                runs=[frame_stats(pair) for pair in pairs],
                estimated_realtime_percent=2 / statistics.median(statistics.mean(pair) for pair in pairs))


def bench_handset(root, binary, output, env, repeats):
    speeds = []
    runs = []
    for repeat in range(repeats):
        directory = output / f'handset-{repeat}'
        directory.mkdir()
        nvram = directory / 'nvram/noki5110/eeprom'
        nvram.parent.mkdir(parents=True)
        subprocess.run([sys.executable, str(root / 'tools/make_5110_eeprom_profile.py'),
                        '--eeprom', str(root / 'roms/noki5110/nse-1.bin'),
                        '--flash', str(root / 'roms/noki5110/5110f530.fls'), '--output', str(nvram)], check=True,
                       capture_output=True, text=True)
        command = [str(binary), 'noki5110', '-rompath', str(root / 'mame/roms'),
                   '-nvram_directory', str(directory / 'nvram'), '-video', 'none', '-sound', 'none',
                   '-nothrottle', '-noreadconfig', '-joystickprovider', 'none', '-midiprovider', 'none',
                   '-log', '-seconds_to_run', '30']
        start = time.monotonic()
        run = subprocess.run(command, cwd=directory, env=env, capture_output=True, text=True, timeout=180)
        elapsed = time.monotonic() - start
        text = run.stdout + run.stderr
        (directory / 'console.log').write_text(text)
        errors = (directory / 'error.log').read_text()
        speeds_found = re.findall(r'Average speed: ([0-9.]+)%', text)
        if run.returncode or len(speeds_found) != 1 or re.search(r'rom4_reset_request|TMS320C54x.*illegal|unsupported.*staged|staged.*unsupported|unimplemented.*C54x', errors):
            raise ValueError('handset benchmark failed or encountered a DSP fault')
        speeds.append(float(speeds_found[0]))
        if not math.isfinite(speeds[-1]) or speeds[-1] <= 0:
            raise ValueError('invalid handset speed')
        runs.append(dict(command=command, process_seconds=elapsed, speed_percent=speeds[-1]))
    return dict(scope='5110 boot/idle including idle DSP, not active-call overhead',
                runs=runs, median_realtime_percent=statistics.median(speeds))


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=root / 'mame/mame')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--frame-timing', action='store_true', help='require per-frame timing from a current fixture')
    parser.add_argument('--handset', action='store_true', help='measure 5110 staged boot/idle and estimate paired budget')
    parser.add_argument('--cpu', type=int, help='pin this benchmark and its children to one available logical CPU')
    parser.add_argument('--bios', nargs='+', default=['efr-decoder', 'efr-encoder'],
                        choices=['efr-decoder', 'efr-encoder'])
    args = parser.parse_args()
    if args.repeats < 1:
        parser.error('--repeats must be positive')
    if args.handset and not args.frame_timing:
        parser.error('--handset requires --frame-timing')
    if args.cpu is not None:
        if args.cpu not in os.sched_getaffinity(0):
            parser.error('--cpu is outside the available affinity set')
        os.sched_setaffinity(0, {args.cpu})
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    binary = args.binary.resolve()
    env = {key: value for key, value in os.environ.items() if not key.startswith('NOKIA_DCT3_')}
    report = dict(binary=file_identity(binary), workspace_core=file_identity(root / 'cpu/tms320c54x/tms320c54x.cpp'),
                  includes_full_frame_replay=True, audio_seconds=3.26, complete=False,
                  cpu_affinity=sorted(os.sched_getaffinity(0)), results=[])
    (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    for bios in args.bios:
        times = []
        execution_times = []
        frame_times = []
        command = [str(binary), 'tms54rom4', '-bios', bios, '-rompath', str(root / 'mame/roms'),
                   '-video', 'none', '-sound', 'none', '-nothrottle', '-noreadconfig',
                   '-joystickprovider', 'none', '-midiprovider', 'none', '-seconds_to_run', '10']
        for repeat in range(args.repeats):
            start = time.monotonic()
            run = subprocess.run(command, cwd=output, env=env, capture_output=True, text=True, timeout=180)
            elapsed = time.monotonic() - start
            text = run.stdout + run.stderr
            (output / f'{bios}-{repeat}.log').write_text(text)
            check_result(text, run.returncode, efr_encoder='encoder' in bios,
                         efr_decoder='decoder' in bios, require_frontend_marker=True)
            if args.frame_timing:
                frame_times.append(parse_frames(text))
            execution = re.findall(r'^ROM4 EFR (?:encoder|decoder) execution: frames=163 audio_seconds=3\.26 host_seconds=([0-9.]+) replay_included=0$', text, re.M)
            if len(execution) > 1:
                raise ValueError('duplicate first-execution timing')
            if args.frame_timing and (len(execution) != 1 or not math.isclose(
                    sum(frame_times[-1]), float(execution[0]), rel_tol=0, abs_tol=.000001)):
                raise ValueError('frame timings do not match aggregate execution time')
            if len(execution) == 1:
                execution_times.append(float(execution[0]))
            times.append(elapsed)
        median = statistics.median(times)
        result = dict(bios=bios, command=command, seconds=times, median_seconds=median,
                      effective_realtime_percent=326 / median)
        if execution_times:
            if len(execution_times) != args.repeats or min(execution_times) <= 0:
                raise ValueError('incomplete/invalid first-execution timing')
            result['first_execution_seconds'] = execution_times
            result['first_execution_realtime_percent'] = 326 / statistics.median(execution_times)
        if frame_times:
            result['frame_seconds'] = frame_times
            result['frame_stats'] = [frame_stats(values) for values in frame_times]
        report['results'].append(result)
        (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
        print(f'{bios}: {median:.3f}s, {326 / median:.1f}% effective realtime (replay included)', flush=True)
        if execution_times:
            print(f"  first execution only: {result['first_execution_realtime_percent']:.1f}% realtime", flush=True)
    if args.handset:
        report['handset'] = bench_handset(root, binary, output, env, args.repeats)
    by_bios = {result['bios']: result for result in report['results']}
    enc, dec = by_bios.get('efr-encoder'), by_bios.get('efr-decoder')
    if enc and dec and args.frame_timing:
        report['paired_budget'] = paired_budget(enc['frame_seconds'], dec['frame_seconds'])
        if args.handset:
            report['paired_with_handset_idle_budget'] = paired_budget(
                enc['frame_seconds'], dec['frame_seconds'], 100 / report['handset']['median_realtime_percent'])
    report['complete'] = True
    (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
