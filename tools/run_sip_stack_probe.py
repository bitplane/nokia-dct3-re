#!/usr/bin/env python3
"""Verify an upstream PJSUA's local SIP/GSM-RTP path; no handset is involved."""

import argparse
import array
import json
import math
from pathlib import Path
import subprocess
import sys
import time
import wave


def write_tone(path, frequency):
    samples = array.array('h', (
        round(8000 * math.sin(2 * math.pi * frequency * index / 8000))
        for index in range(8000 * 8)))
    if sys.byteorder != 'little':
        samples.byteswap()
    with wave.open(str(path), 'wb') as output:
        output.setparams((1, 2, 8000, 0, 'NONE', 'not compressed'))
        output.writeframes(samples.tobytes())


def tone_energy(path, frequency):
    with wave.open(str(path), 'rb') as recording:
        if (recording.getnchannels(), recording.getsampwidth(),
                recording.getframerate()) != (1, 2, 8000):
            raise ValueError('expected mono 16-bit 8 kHz recording')
        samples = array.array('h', recording.readframes(recording.getnframes()))
    if sys.byteorder != 'little':
        samples.byteswap()
    # Inspect the loudest second, excluding silent negotiation/release intervals.
    windows = [samples[index:index + 8000]
               for index in range(0, len(samples) - 7999, 8000)]
    if not windows:
        raise ValueError('recording is shorter than one second')
    window = max(windows, key=lambda values: sum(value * value for value in values))
    energy = sum(value * value for value in window)
    rms = math.sqrt(energy / len(window))
    real = sum(value * math.cos(2 * math.pi * frequency * index / 8000)
               for index, value in enumerate(window))
    imag = sum(value * math.sin(2 * math.pi * frequency * index / 8000)
               for index, value in enumerate(window))
    fraction = 2 * (real * real + imag * imag) / (len(window) * max(energy, 1))
    if rms < 500 or fraction < 0.5:
        raise ValueError(f'missing received {frequency} Hz tone: rms={rms:.1f}, fraction={fraction:.3f}')
    return {'rms': rms, 'tone_energy_fraction': fraction}


def wait_log(process, path, token, timeout=20):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        text = path.read_text(errors='replace') if path.exists() else ''
        if token in text:
            return text
        if process.poll() is not None:
            raise RuntimeError(f'PJSUA exited before {token!r}; see {path}')
        time.sleep(0.05)
    raise RuntimeError(f'timed out waiting for {token!r}; see {path}')


def stop(process):
    if process.poll() is None:
        try:
            process.communicate('q\n', timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.communicate()


def run_leg(binary, root, port, frequency):
    root.mkdir(parents=True, exist_ok=False)
    tone = root / 'source.wav'
    recorded = root / 'received.wav'
    write_tone(tone, frequency)
    common = [str(binary), '--null-audio', '--no-tcp', '--no-vad',
              '--clock-rate=8000', '--add-codec=GSM/8000', '--dis-codec=*',
              '--bound-addr=127.0.0.1', '--ip-addr=127.0.0.1', '--log-level=5']
    processes = []
    try:
        with (root / 'callee.log').open('w') as callee_log, (root / 'caller.log').open('w') as caller_log:
            callee = subprocess.Popen(common + [f'--local-port={port}',
                '--auto-answer=200', f'--play-file={tone}', '--auto-play'],
                stdin=subprocess.PIPE, stdout=callee_log, stderr=subprocess.STDOUT, text=True)
            processes.append(callee)
            wait_log(callee, root / 'callee.log', 'pjsua version')
            caller = subprocess.Popen(common + [f'--local-port={port + 1}',
                '--duration=4', f'--rec-file={recorded}', '--auto-rec',
                f'sip:probe@127.0.0.1:{port}'], stdin=subprocess.PIPE,
                stdout=caller_log, stderr=subprocess.STDOUT, text=True)
            processes.append(caller)
            text = wait_log(caller, root / 'caller.log', 'DISCONNECTED', timeout=30)
            if ('state changed to CONFIRMED' not in text or
                    'audio updated, stream #0: GSM (sendrecv)' not in text or
                    'DISCONNECTED [reason=200 (OK)]' not in text):
                raise RuntimeError('call did not confirm with GSM media')
    finally:
        for process in reversed(processes):
            stop(process)
    return tone_energy(recorded, frequency)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pjsua', type=Path, required=True)
    parser.add_argument('--run-dir', type=Path, required=True)
    parser.add_argument('--port', type=int, default=25060)
    args = parser.parse_args()
    root = args.run_dir.resolve()
    root.mkdir(parents=True, exist_ok=True)
    results = []
    try:
        for name, port, frequency in (
                ('a-to-b', args.port, 440), ('b-to-a', args.port + 2, 660)):
            results.append({'leg': name, 'frequency': frequency,
                            **run_leg(args.pjsua.resolve(), root / name, port, frequency)})
    except (OSError, RuntimeError, ValueError) as error:
        print(f'FAIL - {error}', file=sys.stderr)
        return 1
    (root / 'result.json').write_text(json.dumps({
        'scope': 'upstream SIP stack only; no MAME, handset or native DSP',
        'passed': True, 'legs': results}, indent=2) + '\n')
    print('OK - two local SIP calls confirmed, released and carried received GSM-coded tones')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
