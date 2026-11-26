#!/usr/bin/env python3
"""Many SIP calls, both directions, in one session of the native 6210.

The lean lab composition (run_6210_sip.py's: no observers, no debugger, MAME
throttled) runs headless with the lab cell accepting both call directions.
Calls alternate: a PJSUA remote calls the handset, which answers with Send
once it rings and the remote hangs up; then the handset dials, a PJSUA remote
answers and the handset hangs up with End. The handset idles between calls.

Each call must carry speech both ways (the bridge requires 100 frames each
way, and the remote must hear the handset's 500 Hz microphone tone). The
handset must not reset, and its idle screen must return after every call.
"""
import argparse
import asyncio
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.bench_6210_call import MODEL, prepare  # noqa: E402
from tools.lab_host_call_client import MICROPHONE_HZ, TONE_HZ  # noqa: E402
from tools.run_npe3_native import configure_cell_power  # noqa: E402
from tools.run_rom4_lab_sip_call import recorded_fraction, wait_for_http  # noqa: E402
from tools.run_sip_stack_probe import write_tone  # noqa: E402

DIALLED = '1234567'
BOOT_RESET_LINES = 3  # the MCU releases, holds and re-releases the DSP while loading it


class Soak:
    def __init__(self, args, base):
        self.args, self.base = args, base
        self.directory = base / 'handset'
        self.commands = self.directory / 'commands'
        self.bridge_log = base / 'sip-bridge.log'
        self.processes = []
        self.calls = []

    def log(self, message):
        print(f'[{time.monotonic() - self.started:7.1f}s] {message}', flush=True)

    def send(self, *lines):
        with self.commands.open('a') as file:
            file.write(''.join(line + '\n' for line in lines))

    def bridge_count(self, marker):
        return self.bridge_log.read_text(errors='replace').count(marker)

    async def wait_bridge(self, marker, count, timeout, what):
        deadline = time.monotonic() + timeout
        while self.bridge_count(marker) < count:
            for process in self.processes:
                if process.returncode is not None:
                    raise RuntimeError(f'a process exited while waiting for {what}')
            if time.monotonic() > deadline:
                raise RuntimeError(f'timed out waiting for {what}')
            await asyncio.sleep(0.1)

    async def remote(self, call, *arguments):
        """A PJSUA remote for one call, recording what it hears."""
        port = self.args.sip_port
        log = (self.base / f'remote-{call}.log').open('w')
        process = await asyncio.create_subprocess_exec(
            str(self.args.pjsua), '--null-audio', '--no-tcp', '--no-vad', '--clock-rate=8000',
            '--bound-addr=127.0.0.1', '--ip-addr=127.0.0.1', f'--local-port={port}',
            *(f'--dis-codec={codec}' for codec in ('speex', 'iLBC', 'GSM', 'G722', 'PCMA')),
            f'--play-file={self.base / "remote.wav"}', '--auto-play',
            f'--rec-file={self.base / f"remote-{call}.wav"}', '--auto-rec',
            '--id', f'sip:4420@127.0.0.1:{port}', *arguments,
            stdin=asyncio.subprocess.PIPE, stdout=log, stderr=asyncio.subprocess.STDOUT)
        return process

    async def quit(self, process):
        try:
            process.stdin.write(b'q\n')
            await process.stdin.drain()
            await asyncio.wait_for(process.wait(), 10)
        except (asyncio.TimeoutError, BrokenPipeError, ConnectionResetError):
            process.kill()
            await process.wait()

    async def incoming(self, call, ended):
        # Dialled calls alert too, so count from this call's start.
        alerting = self.bridge_count('SIP handset alerting')
        answered = self.bridge_count('SIP physical answer')
        remote = await self.remote(call, f'sip:dct3@127.0.0.1:{self.args.sip_port + 1}')
        try:
            await self.wait_bridge('SIP handset alerting', alerting + 1, 60, 'the handset to ring')
            self.send('key Send 0.15')
            await self.wait_bridge('SIP physical answer', answered + 1, 30, 'the answer')
            self.log(f'call {call} incoming: answered')
            await asyncio.sleep(self.args.talk)
            remote.stdin.write(b'h\n')
            await remote.stdin.drain()
            await self.wait_bridge('SIP bridge ended', ended + 1, 60, 'the call to clear')
        finally:
            await self.quit(remote)

    async def outgoing(self, call, ended):
        confirmed = self.bridge_count('SIP confirmed status=200')
        remote = await self.remote(call, '--auto-answer=200')
        try:
            await asyncio.sleep(1)
            self.send(*(f'key Keypad {digit} 0.1' for digit in DIALLED), 'key Send 0.15')
            await self.wait_bridge('SIP confirmed status=200', confirmed + 1, 60, 'the remote to answer')
            self.log(f'call {call} outgoing: answered')
            await asyncio.sleep(self.args.talk)
            self.send('key End 0.15')
            await self.wait_bridge('SIP bridge ended', ended + 1, 60, 'the call to clear')
        finally:
            await self.quit(remote)

    async def run(self):
        self.started = time.monotonic()
        write_tone(self.base / 'remote.wav', TONE_HZ)
        self.commands.write_text('')
        environment = {k: v for k, v in os.environ.items() if not k.startswith('NOKIA_DCT3_')}
        environment['NPE3_SOAK_COMMANDS'] = str(self.commands)
        bridge_environment = dict(os.environ, PYTHONPATH=os.pathsep.join(
            filter(None, (self.args.pjsua2, os.environ.get('PYTHONPATH')))))
        command = [str(self.args.mame), MODEL, '-rompath', f'{self.base / "roms"};{ROOT / "roms"}',
                   '-nvram_directory', str(self.base / 'nvram'), '-cfg_directory', 'cfg',
                   '-snapshot_directory', 'snap', '-noreadconfig', '-skip_gameinfo', '-nonvram_save',
                   '-video', 'none', '-sound', 'none', '-log',
                   '-http', '-http_port', str(self.args.http_port),
                   '-autoboot_delay', '0', '-autoboot_script', str(ROOT / 'tools/npe3_sip_soak_input.lua')]
        try:
            with (self.directory / 'console.log').open('wb') as console, self.bridge_log.open('w') as bridge_log:
                handset = await asyncio.create_subprocess_exec(
                    *command, cwd=self.directory, env=environment, stdout=console, stderr=asyncio.subprocess.STDOUT)
                self.processes.append(handset)
                await wait_for_http(self.args.http_port, handset)
                bridge = await asyncio.create_subprocess_exec(
                    sys.executable, str(ROOT / 'tools/dct3_sip_bridge.py'),
                    '--url', f'ws://127.0.0.1:{self.args.http_port}/nokia/dct3/calls',
                    '--destination', f'sip:remote@127.0.0.1:{self.args.sip_port}',
                    '--sip-port', str(self.args.sip_port + 1), '--calls', str(self.args.calls),
                    '--require-frames', '100', env=bridge_environment,
                    stdout=bridge_log, stderr=asyncio.subprocess.STDOUT)
                self.processes.append(bridge)
                await self.wait_bridge('SIP registration ready', 1, 120, 'registration')
                self.log('registered')
                await asyncio.sleep(self.args.idle)
                self.send('snap idle-0')
                for call in range(1, self.args.calls + 1):
                    ended = self.bridge_count('SIP bridge ended')
                    direction = 'incoming' if call % 2 else 'outgoing'
                    await getattr(self, direction)(call, ended)
                    counts = json.loads(re.findall(r'SIP bridge ended (\{[^\n]+\})',
                                                   self.bridge_log.read_text(errors='replace'))[-1])
                    self.calls.append(dict(call=call, direction=direction, **counts))
                    self.log(f'call {call} {direction}: cleared {json.dumps(counts)}')
                    await asyncio.sleep(self.args.idle)
                    self.send(f'snap idle-{call}')
                await asyncio.sleep(1)
                if handset.returncode is not None:
                    raise RuntimeError('the handset exited')
        finally:
            for process in reversed(self.processes):
                if process.returncode is None:
                    process.terminate()
                    try:
                        await asyncio.wait_for(process.wait(), 15)
                    except asyncio.TimeoutError:
                        process.kill()
                        await process.wait()

    def verify(self):
        for entry in self.calls:
            fraction = recorded_fraction(self.base / f'remote-{entry["call"]}.wav', MICROPHONE_HZ)
            entry['remote_microphone'] = round(fraction, 3)
            if fraction < 0.5:
                raise ValueError(f'call {entry["call"]}: the remote did not hear the microphone tone ({fraction:.3f})')
        text = (self.directory / 'error.log').read_text(errors='replace')
        resets = text.count('native_dsp_reset_line:')
        if resets != BOOT_RESET_LINES:
            raise ValueError(f'the DSP reset line moved after boot ({resets} transitions)')
        snaps = sorted((self.directory / 'snap').glob('idle-*.png'),
                       key=lambda path: int(path.stem.split('-')[1]))
        if len(snaps) != self.args.calls + 1:
            raise ValueError(f'{len(snaps)} idle snapshots, expected {self.args.calls + 1}')
        digests = {hashlib.sha256(path.read_bytes()).hexdigest() for path in snaps}
        if len(digests) != 1:
            raise ValueError('the idle screen differs between calls')
        return dict(calls=len(self.calls), idle_screen=digests.pop()[:16])


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--calls', type=int, default=6)
    parser.add_argument('--talk', type=float, default=8, help='wall-clock seconds of conversation per call')
    parser.add_argument('--idle', type=float, default=15, help='wall-clock seconds idle before and between calls')
    parser.add_argument('--mame', type=Path, default=ROOT / 'mame/mame')
    parser.add_argument('--pjsua', type=Path, required=True)
    parser.add_argument('--pjsua2', default=os.environ.get('SIP_PYTHON_PATH', ''))
    parser.add_argument('--sip-port', type=int, default=25200, help="the remote's SIP port; the bridge takes the next one")
    parser.add_argument('--http-port', type=int, default=18121)
    parser.add_argument('--keep', action='store_true')
    args = parser.parse_args()
    args.mame, args.pjsua = args.mame.resolve(), args.pjsua.resolve()
    scratch = ROOT / 'tmp/native-gates'
    scratch.mkdir(parents=True, exist_ok=True)
    base = Path(tempfile.mkdtemp(prefix='npe3-sip-soak.', dir=scratch))
    soak = Soak(args, base)
    try:
        prepare(base)
        soak.directory.mkdir()
        configure_cell_power(soak.directory, MODEL, 'handset33', incoming_call=True, traffic_assignment=True,
                             call_ack=True, clear_call=True, sacch=True, accept_outgoing_service=True,
                             proceed_outgoing_call=True, speech=True)
        asyncio.run(soak.run())
        result = soak.verify()
    except Exception as error:
        for entry in soak.calls:
            print(json.dumps(entry))
        raise SystemExit(f'FAIL - {error}; evidence retained: {base}')
    for entry in soak.calls:
        print(json.dumps(entry))
    print(' '.join(f'{key}={value}' for key, value in result.items()))
    print('NPE-3 native lab SIP soak: PASS')
    if args.keep:
        print(f'evidence retained: {base}')
    else:
        shutil.rmtree(base)


if __name__ == '__main__':
    main()
