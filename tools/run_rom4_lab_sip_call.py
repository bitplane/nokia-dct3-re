#!/usr/bin/env python3
"""A real SIP call to the native NSE-1 handset on the laboratory BTS.

A PJSUA remote calls the DCT3 SIP bridge (dct3_sip_bridge.py), which places the
call on the lab cell through MAME's host call endpoint. The handset rings, is
answered with its own key, and its native DSP carries speech both ways: the
remote's 1 kHz tone to the earpiece and the lab microphone's 500 Hz tone to
the remote. The remote hangs up and the lab clears the call from the network
side. MAME runs in real time, as SIP media does.
"""
import argparse
import asyncio
import json
import math
import os
from pathlib import Path
import re
import sys
import tempfile
import wave

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.lab_host_call_client import MICROPHONE_HZ, TONE_HZ, tone_fraction
from tools.rom4_lab_registration_check import NSE1_LCD_SHA256, check, check_host_lab, check_lcd
from tools.run_rom4_lab_registration import command
from tools.run_sip_stack_probe import write_tone


def recorded_fraction(path, hz):
    with wave.open(str(path), 'rb') as recording:
        return tone_fraction(recording.readframes(recording.getnframes()), hz)


async def run(args, root, run_dir):
    cmd = [part for part in command(root, run_dir, 'nse1ap5', args.mame.resolve(), True, True,
                                    http_port=args.http_port) if part != '-nothrottle']
    write_tone(run_dir / 'remote.wav', TONE_HZ)
    environment = dict(os.environ, PYTHONPATH=os.pathsep.join(filter(None, (args.pjsua2, os.environ.get('PYTHONPATH')))))
    processes = []
    try:
        with (run_dir / 'console.log').open('wb') as console, (run_dir / 'sip-bridge.log').open('w') as bridge_log, \
                (run_dir / 'sip-remote.log').open('w') as remote_log:
            handset = await asyncio.create_subprocess_exec(*cmd, cwd=run_dir, stdout=console, stderr=asyncio.subprocess.STDOUT)
            processes.append(handset)
            bridge = await asyncio.create_subprocess_exec(
                sys.executable, str(root / 'tools/dct3_sip_bridge.py'),
                '--url', f'ws://127.0.0.1:{args.http_port}/nokia/dct3/calls',
                '--destination', f'sip:remote@127.0.0.1:{args.sip_port}', '--sip-port', str(args.sip_port + 1),
                '--once', '--require-frames', '100', '--record-pcm', str(run_dir),
                env=environment, stdout=bridge_log, stderr=asyncio.subprocess.STDOUT)
            processes.append(bridge)
            for _ in range(1200):
                if 'SIP registration ready' in (run_dir / 'sip-bridge.log').read_text(errors='replace'):
                    break
                if bridge.returncode is not None or handset.returncode is not None:
                    raise RuntimeError('bridge or handset exited before registration')
                await asyncio.sleep(0.05)
            else:
                raise RuntimeError('the handset never registered with the bridge')
            remote = await asyncio.create_subprocess_exec(
                str(args.pjsua.resolve()), '--null-audio', '--no-tcp', '--no-vad', '--clock-rate=8000',
                '--bound-addr=127.0.0.1', '--ip-addr=127.0.0.1', f'--local-port={args.sip_port}',
                f'--play-file={run_dir / "remote.wav"}', '--auto-play', '--auto-loop',
                f'--rec-file={run_dir / "remote-received.wav"}', '--auto-rec',
                '--id', f'sip:4420@127.0.0.1:{args.sip_port}', f'sip:dct3@127.0.0.1:{args.sip_port + 1}',
                stdin=asyncio.subprocess.PIPE, stdout=remote_log, stderr=asyncio.subprocess.STDOUT)
            processes.append(remote)
            # pjsua's --duration runs from the INVITE, so ringing would eat the
            # talk time: hang up a fixed time after the handset answers.
            for _ in range(1200):
                if 'SIP physical answer' in (run_dir / 'sip-bridge.log').read_text(errors='replace'):
                    break
                if bridge.returncode is not None or remote.returncode is not None:
                    raise RuntimeError('call ended before the handset answered')
                await asyncio.sleep(0.05)
            else:
                raise RuntimeError('the handset never answered')
            await asyncio.sleep(args.talk)
            remote.stdin.write(b'h\n')
            await remote.stdin.drain()
            if await asyncio.wait_for(bridge.wait(), 120):
                raise RuntimeError('SIP bridge failed; inspect sip-bridge.log')
            if await asyncio.wait_for(handset.wait(), 120):
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
                except (asyncio.TimeoutError, BrokenPipeError, ConnectionResetError, ProcessLookupError):
                    process.kill()
                    await process.wait()


def verify(run_dir):
    text = (run_dir / 'error.log').read_text(errors='replace')
    page = text.find('npe3_bts_page_request:')
    result = check(text[:page] if page >= 0 else text)
    result.update(check_host_lab(text))
    check_lcd(run_dir / 'snap', NSE1_LCD_SHA256)
    bridge = (run_dir / 'sip-bridge.log').read_text(errors='replace')
    remote = (run_dir / 'sip-remote.log').read_text(errors='replace')
    for marker in ('SIP incoming identity=', 'caller=4420', 'SIP physical answer identity=', 'SIP confirmed status=200'):
        if marker not in bridge:
            raise ValueError(f'bridge log lacks {marker!r}')
    if 'state changed to CONFIRMED' not in remote or 'DISCONNECTED' not in remote:
        raise ValueError('the remote SIP call did not confirm and end')
    counts = json.loads(re.search(r'SIP bridge ended (\{[^\n]+\})', bridge)[1])
    # The remote must hear the handset's microphone through native encoding,
    # the bridge's decoding and RTP.
    remote_microphone = recorded_fraction(run_dir / 'remote-received.wav', MICROPHONE_HZ)
    if remote_microphone < 0.5:
        raise ValueError(f'the remote did not hear the handset microphone tone ({remote_microphone:.3f})')
    result.update(sip_uplink_frames=counts['uplink'], sip_downlink_frames=counts['downlink'],
                  rtp_sent=counts['pcm_transmitted'], rtp_received=counts['pcm_received'],
                  dropped=counts['dropped'], remote_microphone_fraction=round(remote_microphone, 3))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mame', type=Path, default=Path('mame/mame'))
    parser.add_argument('--pjsua', type=Path, required=True)
    parser.add_argument('--pjsua2', required=True, help='PYTHONPATH entries for the pjsua2 binding')
    parser.add_argument('--http-port', type=int, default=18111)
    parser.add_argument('--sip-port', type=int, default=25180)
    # The handset's DSP runs at about 40% of real time during a call, so 12 s
    # of wall-clock talk carries roughly 240 uplink frames (the bridge needs 100).
    parser.add_argument('--talk', type=int, default=12, help='wall-clock seconds the remote talks after answer')
    parser.add_argument('--keep', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    scratch = root / 'tmp/native-gates'
    scratch.mkdir(parents=True, exist_ok=True)
    run_dir = Path(tempfile.mkdtemp(prefix='nse1ap5-sip.', dir=scratch))
    os.rmdir(run_dir)
    try:
        os.environ['NOKIA_DCT3_SNAPSHOT_DIR'] = str(run_dir / 'snap')
        asyncio.run(run(args, root, run_dir))
        result = verify(run_dir)
    except Exception as error:
        raise SystemExit(f'FAIL - {error}; evidence retained: {run_dir}')
    print(' '.join(f'{key}={value}' for key, value in result.items()))
    print('NSE ROM4 nse1ap5 native laboratory SIP call: PASS')
    if args.keep:
        print(f'evidence retained: {run_dir}')
    else:
        import shutil
        shutil.rmtree(run_dir)


if __name__ == '__main__':
    main()
