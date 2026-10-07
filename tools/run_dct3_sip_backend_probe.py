#!/usr/bin/env python3
"""Exercise the real SIP bridge against a synthetic host interface, not a phone."""

import argparse
import asyncio
import json
from pathlib import Path
import sys
import wave

import websockets

try:
    from tools.gsm_fr_codec import GsmFrCodec
    from tools.run_sip_stack_probe import write_tone, tone_energy
except ModuleNotFoundError:
    from gsm_fr_codec import GsmFrCodec
    from run_sip_stack_probe import write_tone, tone_energy


async def run(args):
    root = args.run_dir.resolve()
    root.mkdir(parents=True, exist_ok=False)
    source = root / 'remote.wav'
    write_tone(source, 660)
    write_tone(root / 'uplink.wav', 440)
    codec = GsmFrCodec()
    downlink = bytearray()
    complete = asyncio.get_running_loop().create_future()

    async def host(websocket):
        try:
            await websocket.send(json.dumps({'type': 'call_adapter_ready', 'protocol_version': 1, 'epoch': 1}))
            await websocket.send(json.dumps({'type': 'outgoing_call', 'epoch': 1, 'request_id': 1, 'digits': '123'}))
            decision = json.loads(await asyncio.wait_for(websocket.recv(), 10))
            if decision != {'epoch': 1, 'request_id': 1, 'type': 'outgoing_call_decision', 'decision': 'connect'}:
                raise RuntimeError(f'unexpected SIP decision {decision!r}')
            await websocket.send(json.dumps({'type': 'outgoing_call_state', 'epoch': 1,
                'request_id': 1, 'phase': 'connected', 'media_downlink_sequence': 0}))

            async def uplink():
                with wave.open(str(root / 'uplink.wav'), 'rb') as audio:
                    for sequence in range(150):
                        frame = codec.encode(audio.readframes(160))
                        await websocket.send(json.dumps({'type': 'outgoing_call_media_uplink',
                            'epoch': 1, 'request_id': 1, 'sequence': sequence,
                            'emulation_time_us': sequence * 20000, 'good': True, 'frame': frame.hex()}))
                        await asyncio.sleep(0.02)

            sender = asyncio.create_task(uplink())
            sequence = 0
            try:
                while True:
                    event = json.loads(await asyncio.wait_for(websocket.recv(), 10))
                    if (event.get('epoch'), event.get('request_id')) != (1, 1):
                        raise RuntimeError('SIP bridge emitted wrong host identity')
                    if event['type'] == 'outgoing_call_terminate':
                        await websocket.send(json.dumps({'type': 'outgoing_call_state',
                            'epoch': 1, 'request_id': 1, 'phase': 'ended'}))
                        break
                    if event['type'] != 'outgoing_call_media_downlink' or event['sequence'] != sequence:
                        raise RuntimeError('unexpected media ordering')
                    downlink.extend(codec.decode(bytes.fromhex(event['frame'])))
                    sequence += 1
                await sender
                complete.set_result(sequence)
            finally:
                sender.cancel()
                await asyncio.gather(sender, return_exceptions=True)
        except Exception as error:
            complete.set_exception(error)

    processes = []
    try:
        with (root / 'remote.log').open('w') as remote_log, (root / 'bridge.log').open('w') as bridge_log:
            remote = await asyncio.create_subprocess_exec(str(args.pjsua.resolve()),
                '--null-audio', '--no-tcp', '--no-vad', '--clock-rate=8000',
                '--bound-addr=127.0.0.1', '--ip-addr=127.0.0.1', f'--local-port={args.sip_port}',
                '--auto-answer=200', '--duration=4', f'--play-file={source}', '--auto-play',
                f'--rec-file={root / "remote-received.wav"}', '--auto-rec',
                stdin=asyncio.subprocess.PIPE, stdout=remote_log, stderr=asyncio.subprocess.STDOUT)
            processes.append(remote)
            for _ in range(100):
                if 'pjsua version' in (root / 'remote.log').read_text(errors='replace'):
                    break
                if remote.returncode is not None:
                    raise RuntimeError('remote SIP endpoint failed')
                await asyncio.sleep(0.05)
            else:
                raise RuntimeError('remote SIP endpoint did not become ready')
            async with websockets.serve(host, '127.0.0.1', args.ws_port):
                bridge = await asyncio.create_subprocess_exec(sys.executable,
                    str(Path(__file__).with_name('dct3_sip_bridge.py')),
                    '--url', f'ws://127.0.0.1:{args.ws_port}', '--destination',
                    f'sip:probe@127.0.0.1:{args.sip_port}', '--sip-port', str(args.sip_port + 1),
                    '--once', '--require-frames', '100', stdout=bridge_log, stderr=asyncio.subprocess.STDOUT)
                processes.append(bridge)
                count = await asyncio.wait_for(complete, 20)
                if await asyncio.wait_for(bridge.wait(), 10):
                    raise RuntimeError('SIP bridge failed; inspect bridge.log')
    finally:
        codec.close()
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
    with wave.open(str(root / 'host-received.wav'), 'wb') as output:
        output.setparams((1, 2, 8000, 0, 'NONE', 'not compressed'))
        output.writeframes(downlink)
    result = tone_energy(root / 'host-received.wav', 660)
    uplink_result = tone_energy(root / 'remote-received.wav', 440)
    if count < 100:
        raise RuntimeError('insufficient SIP downlink media')
    (root / 'result.json').write_text(json.dumps({
        'scope': 'real SIP bridge, synthetic host interface; no handset/native DSP',
        'downlink_frames': count, 'downlink_tone': result,
        'uplink_tone': uplink_result}, indent=2) + '\n')
    print('OK - real SIP call and bidirectional GSM-FR/PCM tones cross the host bridge')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pjsua', type=Path, required=True)
    parser.add_argument('--run-dir', type=Path, required=True)
    parser.add_argument('--sip-port', type=int, default=25080)
    parser.add_argument('--ws-port', type=int, default=25090)
    args = parser.parse_args()
    try:
        asyncio.run(run(args))
    except (OSError, RuntimeError, ValueError, asyncio.TimeoutError) as error:
        parser.exit(1, f'FAIL - {error}\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
