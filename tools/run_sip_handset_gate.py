#!/usr/bin/env python3
"""Run an isolated handset command with the real SIP host backend."""

import argparse
import asyncio
import json
from pathlib import Path
import re
import sys

try:
    from tools.run_sip_stack_probe import write_tone
except ModuleNotFoundError:
    from run_sip_stack_probe import write_tone


async def run(args):
    root = args.run_dir.resolve()
    root.mkdir(parents=True, exist_ok=True)
    write_tone(root / 'sip-source.wav', 660)
    processes = []
    try:
        with (root / 'sip-remote.log').open('w') as remote_log, (root / 'sip-bridge.log').open('w') as bridge_log:
            remote_command = [str(args.pjsua.resolve()),
                '--null-audio', '--no-tcp', '--no-vad', '--clock-rate=8000',
                '--bound-addr=127.0.0.1', '--ip-addr=127.0.0.1', f'--local-port={args.sip_port}',
                f'--auto-answer={args.sip_response}', '--duration=8', f'--play-file={root / "sip-source.wav"}',
                '--auto-play']
            if not args.incoming:
                remote = await asyncio.create_subprocess_exec(*remote_command,
                    stdin=asyncio.subprocess.PIPE, stdout=remote_log, stderr=asyncio.subprocess.STDOUT)
                processes.append(remote)
                for _ in range(100):
                    if 'pjsua version' in (root / 'sip-remote.log').read_text(errors='replace'):
                        break
                    if remote.returncode is not None:
                        raise RuntimeError('remote SIP endpoint failed')
                    await asyncio.sleep(0.05)
                else:
                    raise RuntimeError('SIP endpoint did not become ready')
            handset = await asyncio.create_subprocess_exec(*args.command)
            processes.append(handset)
            for _ in range(300):
                if handset.returncode is not None:
                    raise RuntimeError('handset exited before HTTP was ready')
                try:
                    _, writer = await asyncio.open_connection('127.0.0.1', args.http_port)
                    writer.close()
                    await writer.wait_closed()
                    break
                except OSError:
                    await asyncio.sleep(0.1)
            else:
                raise RuntimeError('handset HTTP endpoint did not become ready')
            bridge = await asyncio.create_subprocess_exec(sys.executable,
                str(Path(__file__).with_name('dct3_sip_bridge.py')),
                '--url', f'ws://127.0.0.1:{args.http_port}/nokia/dct3/calls',
                '--destination', f'sip:probe@127.0.0.1:{args.sip_port}',
                '--sip-port', str(args.sip_port + 1), '--once', '--require-frames',
                '100' if args.sip_response == 200 else '0',
                stdout=bridge_log, stderr=asyncio.subprocess.STDOUT)
            processes.append(bridge)
            if args.incoming:
                for _ in range(900):
                    if 'SIP registration ready' in (root / 'sip-bridge.log').read_text(errors='replace'):
                        break
                    if bridge.returncode is not None:
                        raise RuntimeError('bridge exited before registration')
                    await asyncio.sleep(0.05)
                else:
                    raise RuntimeError('incoming SIP lacks registered handset')
                remote = await asyncio.create_subprocess_exec(*remote_command,
                    '--id', f'sip:5551234@127.0.0.1:{args.sip_port}',
                    f'sip:dct3@127.0.0.1:{args.sip_port + 1}',
                    stdin=asyncio.subprocess.PIPE, stdout=remote_log, stderr=asyncio.subprocess.STDOUT)
                processes.append(remote)
            if await asyncio.wait_for(bridge.wait(), 90):
                raise RuntimeError('handset SIP bridge failed; inspect sip-bridge.log')
            if await asyncio.wait_for(handset.wait(), 90):
                raise RuntimeError('handset exited unsuccessfully')
    finally:
        for process in reversed(processes):
            if process.returncode is None:
                try:
                    if process.stdin:
                        process.stdin.write(b'q\n')
                        await process.stdin.drain()
                    else:
                        process.terminate()
                    await asyncio.wait_for(process.wait(), 5)
                except (asyncio.TimeoutError, BrokenPipeError, ConnectionResetError):
                    process.kill()
                    await process.wait()
    remote_text = (root / 'sip-remote.log').read_text(errors='replace')
    if args.sip_response != 200:
        verify_failure(root, remote_text, args.sip_response)
        return
    if 'state changed to CONFIRMED' not in remote_text or 'DISCONNECTED [reason=200 (OK)]' not in remote_text:
        raise RuntimeError('remote SIP call did not confirm and release normally')
    bridge_text = (root / 'sip-bridge.log').read_text(errors='replace')
    identity_marker = 'caller=5551234' if args.incoming else 'digits=5551234'
    if identity_marker not in bridge_text or 'SIP confirmed status=200' not in bridge_text:
        raise RuntimeError('missing reviewed physical dial/SIP acceptance')
    if args.incoming and 'SIP physical answer identity=' not in bridge_text:
        raise RuntimeError('incoming SIP lacked handset-owned Answer')
    match = re.search(r'SIP bridge ended (\{[^\n]+\})', bridge_text)
    if not match:
        raise RuntimeError('missing bridge media/release summary')
    counts = json.loads(match[1])
    if min(counts.get(name, 0) for name in (
            'uplink', 'downlink', 'pcm_transmitted', 'pcm_received')) < 100:
        raise RuntimeError('insufficient executed bidirectional media')
    log = (root / 'error.log').read_text(errors='replace')
    cursor = 0
    patterns = (
            r'gsm_call_adapter: incoming state id=1 epoch=1 phase=paging',
            r'GSM service downlink kind=9 sapi=0 pd=03 message=05',
            r'input-press: t=[0-9.]+ name=enter',
            r'GSM service uplink sapi=0 pd=03 message=07 length=2 data=8347',
            r'gsm_call_adapter: incoming state id=1 epoch=1 phase=connected',
            r'gsm_call_adapter: termination id=1 cause=16 result=accepted',
            r'GSM service uplink sapi=0 pd=03 message=2a .*data=032a0802e0d1',
            r'gsm_call_adapter: incoming state id=1 epoch=1 phase=ended',
    ) if args.incoming else (
            r'GSM service uplink sapi=0 pd=03 message=05 length=15 data=03450401a05e0581551532f4150101',
            r'GSM service downlink kind=12 sapi=0 pd=03 message=07',
            r'GSM service uplink sapi=0 pd=03 message=0f .*data=030f',
            r'GSM service uplink sapi=0 pd=03 message=2d .*data=03(?:2d|6d)',
            r'LAPDm service Channel Release acknowledged')
    for pattern in patterns:
        match = re.search(pattern, log[cursor:])
        if not match:
            raise RuntimeError(f'missing ordered firmware call checkpoint: {pattern}')
        cursor += match.end()
    (root / 'sip-result.json').write_text(json.dumps({
        'scope': f'3210 research-HLE physical {"incoming" if args.incoming else "outgoing"} SIP signaling and media transport; not native DSP speech',
        ('caller' if args.incoming else 'dialed_digits'): '5551234',
        'media': counts, 'passed': True}, indent=2) + '\n')
    print('OK - physical handset call connected to SIP with bidirectional host media and release')


def verify_failure(root, remote_text, status):
    bridge_text = (root / 'sip-bridge.log').read_text(errors='replace')
    log = (root / 'error.log').read_text(errors='replace')
    if (f'SIP/2.0 {status} ' not in remote_text or
            f'SIP disconnected status={status} identity=(1, 1)' not in bridge_text):
        raise RuntimeError('missing actual SIP failure response and correlated bridge result')
    if ('state changed to CONFIRMED' in remote_text or 'SIP confirmed' in bridge_text or
            re.search(r'GSM service downlink kind=12 sapi=0 pd=03 message=07', log)):
        raise RuntimeError('failed SIP call falsely connected')
    match = re.search(r'SIP bridge ended (\{[^\n]+\})', bridge_text)
    if not match:
        raise RuntimeError('failed SIP call never completed handset release')
    counts = json.loads(match[1])
    if any(counts.get(name) != 0 for name in ('uplink', 'downlink', 'pcm_transmitted', 'pcm_received')):
        raise RuntimeError('failed SIP call falsely claimed media')
    cursor = 0
    for pattern in (
            r'GSM service uplink sapi=0 pd=03 message=05 length=15 data=03450401a05e0581551532f4150101',
            r'GSM service downlink kind=13 sapi=0 pd=03 message=25 length=5',
            # Bit 6 is the CC send-sequence flag; it is not another primitive.
            r'GSM service uplink sapi=0 pd=03 message=2d .*data=03(?:2d|6d)',
            r'GSM service downlink kind=26 sapi=0 pd=03 message=2a',
            r'LAPDm service Channel Release acknowledged'):
        match = re.search(pattern, log[cursor:])
        if not match:
            raise RuntimeError(f'missing SIP-failure firmware checkpoint: {pattern}')
        cursor += match.end()
    if status == 480 and 'outgoing termination consumed id=1 cause=18' not in log:
        raise RuntimeError('SIP 480 did not deliver cause 18 through the GSM session')
    if status == 486 and 'outgoing decision consumed id=1 outcome=1' not in log:
        raise RuntimeError('SIP 486 did not deliver the GSM busy decision')
    (root / 'sip-result.json').write_text(json.dumps({
        'scope': '3210 HLE physical outgoing SIP failure and firmware release; no connection/media',
        'sip_status': status, 'media': counts, 'passed': True}, indent=2) + '\n')
    print(f'OK - SIP {status} became a correlated handset failure and clean release without CONNECT/media')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pjsua', type=Path, required=True)
    parser.add_argument('--run-dir', type=Path, required=True)
    parser.add_argument('--sip-port', type=int, default=25100)
    parser.add_argument('--http-port', type=int, default=18100)
    parser.add_argument('--incoming', action='store_true')
    parser.add_argument('--sip-response', type=int, choices=(200, 480, 486), default=200)
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.incoming and args.sip_response != 200:
        parser.error('--sip-response failure fixtures are outgoing only')
    if args.command[:1] == ['--']:
        args.command = args.command[1:]
    if not args.command:
        parser.error('a handset command is required')
    try:
        asyncio.run(run(args))
    except (OSError, RuntimeError, asyncio.TimeoutError) as error:
        parser.exit(1, f'FAIL - {error}\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
