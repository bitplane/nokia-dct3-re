#!/usr/bin/env python3
"""Run an isolated handset command with the real SIP host backend."""

import argparse
import asyncio
from email.parser import HeaderParser
import json
from pathlib import Path
import re
import sys

try:
    from tools.run_sip_stack_probe import write_tone
except ModuleNotFoundError:
    from run_sip_stack_probe import write_tone


def outgoing_setup_pattern(product):
    frame = {
        '3210': '03450401a05e0581551532f4150101',
        '3310': '03450404600200815e0581551532f4a2150101',
    }[product]
    return (rf'GSM service uplink sapi=0 pd=03 message=05 length={len(frame) // 2} '
            rf'data={frame}(?:\s|$)')


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
            if args.record_media:
                remote_command += [f'--rec-file={root / "sip-microphone.wav"}', '--auto-rec']
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
                '--sip-port', str(args.sip_port + 1),
                *(['--once'] if args.calls == 1 else ['--calls', str(args.calls)]), '--require-frames',
                '100' if args.sip_response == 200 and not args.cancel_incoming and
                not (args.restore_call and args.restore_phase == 'alerting') else '0',
                *(['--record-pcm', str(root)] if args.record_media else []),
                stdout=bridge_log, stderr=asyncio.subprocess.STDOUT)
            processes.append(bridge)
            if args.incoming:
                for _ in range(900):
                    ready = ('SIP registration ready epoch=2' if args.restore_idle
                             else 'SIP registration ready')
                    if ready in (root / 'sip-bridge.log').read_text(errors='replace'):
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
                if args.cancel_incoming:
                    for _ in range(400):
                        if 'incoming state id=1 epoch=1 phase=alerting' in (root / 'error.log').read_text(errors='replace'):
                            break
                        if bridge.returncode is not None or remote.returncode is not None:
                            raise RuntimeError('call ended before incoming alerting')
                        await asyncio.sleep(0.05)
                    else:
                        raise RuntimeError('handset never alerted before SIP cancellation')
                    remote.stdin.write(b'h\n')
                    await remote.stdin.drain()
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
    if args.restore_outgoing:
        verify_outgoing_restore(root, remote_text, args.sip_response == 200, args.product)
        return
    if args.restore_call:
        verify_restore(root, remote_text, args.restore_phase, args.product)
        return
    if args.cancel_incoming:
        verify_cancel(root, remote_text, args.product)
        return
    if args.sip_response != 200:
        verify_failure(root, remote_text, args.sip_response, args.product, args.calls)
        return
    verify_success(root, remote_text, args)


def verify_success(root, remote_text, args):
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
    downlink = re.findall(
        r'gsm_call_adapter: media direction=downlink id=1 sequence=(\d+) result=(accepted|rejected)', log)
    if (len(downlink) < 100 or any(result != 'accepted' for _, result in downlink) or
            [int(sequence) for sequence, _ in downlink] != list(range(len(downlink)))):
        raise RuntimeError('handset did not accept a sustained ordered SIP downlink')
    epoch = 2 if args.restore_idle else 1
    if args.restore_idle:
        if ('SIP idle snapshot accepted epoch=2' not in bridge_text or
                bridge_text.count('SIP incoming identity=') != 1):
            raise RuntimeError('idle restoration did not admit exactly one fresh SIP call')
    cursor = 0
    product = getattr(args, 'product', '3210')
    # Exact encodings from these acceptance fixtures, including their observed
    # CC sequence bit; not a claim that the bit is a fixed product property.
    connect_data = '8307' if product == '3310' else '8347'
    release_complete_data = '036a0802e0d1' if product == '3310' else '032a0802e0d1'
    patterns = (
            rf'gsm_call_adapter: incoming state id=1 epoch={epoch} phase=paging',
            r'GSM service downlink kind=9 sapi=0 pd=03 message=05',
            (r'sip_state: physical Answer after idle restoration' if args.restore_idle
             else r'input-press: t=[0-9.]+ name=enter'),
            rf'GSM service uplink sapi=0 pd=03 message=07 length=2 data={connect_data}',
            rf'gsm_call_adapter: incoming state id=1 epoch={epoch} phase=connected',
            r'gsm_call_adapter: termination id=1 cause=16 result=accepted',
            rf'GSM service uplink sapi=0 pd=03 message=2a .*data={release_complete_data}',
            rf'gsm_call_adapter: incoming state id=1 epoch={epoch} phase=ended',
    ) if args.incoming else (
            outgoing_setup_pattern(product),
            r'GSM service downlink kind=12 sapi=0 pd=03 message=07',
            r'GSM service uplink sapi=0 pd=03 message=0f .*data=030f',
            r'GSM service uplink sapi=0 pd=03 message=2d .*data=03(?:2d|6d)',
            r'LAPDm service Channel Release acknowledged')
    if args.restore_idle:
        patterns = (r'sip_state: saved', r'sip_state: restored') + patterns
    for pattern in patterns:
        match = re.search(pattern, log[cursor:])
        if not match:
            raise RuntimeError(f'missing ordered firmware call checkpoint: {pattern}')
        cursor += match.end()
    (root / 'sip-result.json').write_text(json.dumps({
        'scope': f'{getattr(args, "product", "3210")} research-HLE physical {"incoming" if args.incoming else "outgoing"} SIP signaling and media transport; not native DSP speech',
        ('caller' if args.incoming else 'dialed_digits'): '5551234',
        'media': counts, 'passed': True}, indent=2) + '\n')
    print('OK - physical handset call connected to SIP with bidirectional host media and release')


def verify_failure(root, remote_text, status, product='3210', calls=1):
    bridge_text = (root / 'sip-bridge.log').read_text(errors='replace')
    log = (root / 'error.log').read_text(errors='replace')
    dialogs = set()
    for response in re.finditer(
            rf'^SIP/2.0 {status}[^\r\n]*\r?\n((?:[^\r\n]+\r?\n)*)', remote_text, re.MULTILINE):
        headers = HeaderParser().parsestr(response[1])
        call_ids = headers.get_all('Call-ID', [])
        if len(call_ids) == 1 and headers.get('CSeq', '').split()[-1:] == ['INVITE']:
            dialogs.add(call_ids[0])
    if len(dialogs) != calls:
        raise RuntimeError('missing distinct actual SIP failure dialogs')
    if (f'SIP/2.0 {status} ' not in remote_text or
            f'SIP disconnected status={status} identity=(1, 1)' not in bridge_text):
        raise RuntimeError('missing actual SIP failure response and correlated bridge result')
    if ('state changed to CONFIRMED' in remote_text or 'SIP confirmed' in bridge_text or
            re.search(r'GSM service downlink kind=12 sapi=0 pd=03 message=07', log)):
        raise RuntimeError('failed SIP call falsely connected')
    summaries = re.findall(r'SIP bridge ended (\{[^\n]+\})', bridge_text)
    if len(summaries) != calls:
        raise RuntimeError('failed SIP call never completed handset release')
    media = [json.loads(summary) for summary in summaries]
    if any(counts.get(name) != 0 for counts in media
           for name in ('uplink', 'downlink', 'pcm_transmitted', 'pcm_received')):
        raise RuntimeError('failed SIP call falsely claimed media')
    cursor = 0
    for request_id in range(1, calls + 1):
        if f'SIP disconnected status={status} identity=(1, {request_id})' not in bridge_text:
            raise RuntimeError('missing correlated SIP failure for each handset attempt')
        patterns = [outgoing_setup_pattern(product)]
        if calls > 1:
            patterns += [rf'gsm_call_adapter: request id={request_id} epoch=1 digits=5551234',
                         rf'outgoing decision consumed id={request_id} outcome={1 if status == 486 else 2}']
            if status == 480:
                patterns += [rf'outgoing termination consumed id={request_id} cause=18']
        patterns += [
            r'GSM service downlink kind=13 sapi=0 pd=03 message=25 length=5',
            # Bit 6 is the CC send-sequence flag; it is not another primitive.
            r'GSM service uplink sapi=0 pd=03 message=2d .*data=03(?:2d|6d)',
            r'GSM service downlink kind=26 sapi=0 pd=03 message=2a',
            r'LAPDm service Channel Release acknowledged']
        if calls > 1:
            patterns += [rf'gsm_call_adapter: state id={request_id} epoch=1 phase=ended']
        for pattern in patterns:
            match = re.search(pattern, log[cursor:])
            if not match:
                raise RuntimeError(f'missing SIP-failure firmware checkpoint: {pattern}')
            cursor += match.end()
    if status == 480 and 'outgoing termination consumed id=1 cause=18' not in log:
        raise RuntimeError('SIP 480 did not deliver cause 18 through the GSM session')
    if status == 486 and 'outgoing decision consumed id=1 outcome=1' not in log:
        raise RuntimeError('SIP 486 did not deliver the GSM busy decision')
    (root / 'sip-result.json').write_text(json.dumps({
        'scope': f'{product} HLE physical outgoing SIP failure and firmware release; no connection/media',
        'sip_status': status, 'completed_calls': calls,
        'media': media[0] if calls == 1 else media, 'passed': True}, indent=2) + '\n')
    print(f'OK - SIP {status} became a correlated handset failure and clean release without CONNECT/media')


def verify_outgoing_restore(root, remote_text, connected=False, product='3210'):
    bridge = (root / 'sip-bridge.log').read_text(errors='replace')
    log = (root / 'error.log').read_text(errors='replace')
    markers = (('state changed to CONFIRMED', 'Request msg BYE/') if connected else
               ('Response msg 180/INVITE/', 'Request msg CANCEL/', 'Response msg 487/INVITE/'))
    remote_cursor = 0
    for marker in markers:
        position = remote_text.find(marker, remote_cursor)
        if position < 0:
            raise RuntimeError(f'missing real pending SIP release: {marker}')
        remote_cursor = position + len(marker)
    if not connected and ('state changed to CONFIRMED' in remote_text or 'SIP confirmed' in bridge or
            re.search(r'GSM service downlink kind=12 sapi=0 pd=03 message=07', log)):
        raise RuntimeError('pending outgoing restoration falsely connected')
    if (bridge.count('SIP dial identity=') != 1 or
            'SIP epoch changed old=1 new=2' not in bridge or
            'SIP restored call cleared identity=(2, 1)' not in bridge):
        raise RuntimeError('restored outgoing request replayed SIP or failed to clear')
    if 'termination id=1 cause=41 result=rejected' in log:
        raise RuntimeError('restored outgoing call submitted duplicate or invalid termination')
    if len(re.findall(r'termination id=1 cause=41 result=accepted', log)) != 1:
        raise RuntimeError('restored outgoing call did not clear exactly once')
    cursor = 0
    patterns = (r'gsm_call_adapter: request id=1 epoch=1 digits=5551234',)
    if connected:
        patterns += (r'gsm_call_adapter: state id=1 epoch=1 phase=connected',)
    patterns += (
                    r'sip_state: saved', r'sip_state: restored',
                    r'gsm_call_adapter: request id=1 epoch=2 digits=5551234')
    termination = (r'termination id=1 cause=41 result=accepted',
                   r'outgoing termination consumed id=1 cause=41')
    patterns += tuple(reversed(termination)) if connected else termination
    patterns += (
                    r'GSM service downlink kind=13 sapi=0 pd=03 message=25',
                    r'GSM service uplink sapi=0 pd=03 message=2d',
                    r'GSM service downlink kind=26 sapi=0 pd=03 message=2a',
                    r'LAPDm service Channel Release acknowledged',
                    r'gsm_call_adapter: state id=1 epoch=2 phase=ended')
    for pattern in patterns:
        match = re.search(pattern, log[cursor:])
        if not match:
            raise RuntimeError(f'missing outgoing restoration checkpoint: {pattern}')
        cursor += match.end()
    (root / 'sip-result.json').write_text(json.dumps({
        'scope': (f'{product} connected outgoing HLE SIP save/load; BYE, no redial' if connected else
                  f'{product} pending outgoing HLE SIP save/load; CANCEL/487, no redial/CONNECT'),
        'passed': True}, indent=2) + '\n')
    print('OK - outgoing restoration cleared SIP and GSM without redial')


def verify_restore(root, remote_text, phase='connected', product='3210'):
    bridge = (root / 'sip-bridge.log').read_text(errors='replace')
    log = (root / 'error.log').read_text(errors='replace')
    if phase == 'connected':
        if ('state changed to CONFIRMED' not in remote_text or
                'Request msg BYE/' not in remote_text):
            raise RuntimeError('restoration did not close a real connected SIP dialog')
    else:
        if not re.search(r'Response msg [4-6][0-9]{2}/INVITE/', remote_text):
            raise RuntimeError('restoration did not reject the real alerting SIP INVITE')
        if ('state changed to CONFIRMED' in remote_text or 'SIP physical answer' in bridge or
                'SIP confirmed' in bridge or
                re.search(r'GSM service uplink sapi=0 pd=03 message=07', log)):
            raise RuntimeError('alerting restoration falsely answered or connected')
    for marker in ('SIP epoch changed old=1 new=2',
                   'SIP restored call cleared identity=(2, 1)'):
        if marker not in bridge:
            raise RuntimeError(f'missing SIP restoration checkpoint: {marker}')
    if bridge.count('SIP incoming identity=') != 1:
        raise RuntimeError('restoration replayed the incoming SIP dialog')
    if (len(re.findall(r'termination id=1 cause=41 result=accepted', log)) != 1 or
            re.search(r'termination id=1 .*result=rejected', log)):
        raise RuntimeError('restoration did not clear the handset call exactly once')
    cursor = 0
    for pattern in (
            rf'incoming state id=1 epoch=1 phase={phase}',
            r'sip_state: saved', r'sip_state: restored',
            r'termination id=1 cause=41 result=accepted',
            r'GSM service downlink kind=13 sapi=0 pd=03 message=25',
            r'GSM service uplink sapi=0 pd=03 message=2a',
            r'LAPDm service Channel Release acknowledged',
            r'incoming state id=1 epoch=2 phase=ended'):
        match = re.search(pattern, log[cursor:])
        if not match:
            raise RuntimeError(f'missing handset restoration checkpoint: {pattern}')
        cursor += match.end()
    (root / 'sip-result.json').write_text(json.dumps({
        'scope': f'{product} {phase} HLE SIP call save/load; external dialog cleared, not restored',
        'passed': True}, indent=2) + '\n')
    print('OK - save/load cleared real SIP dialog and restored GSM call under new epoch')


def verify_cancel(root, remote_text, product='3210'):
    bridge_text = (root / 'sip-bridge.log').read_text(errors='replace')
    log = (root / 'error.log').read_text(errors='replace')
    if ('Request msg CANCEL/' not in remote_text or 'Response msg 487/INVITE/' not in remote_text or
            'SIP disconnected status=487 identity=(1, 1)' not in bridge_text):
        raise RuntimeError('missing real SIP CANCEL/487 exchange')
    if ('state changed to CONFIRMED' in remote_text or 'SIP confirmed' in bridge_text or
            'SIP physical answer' in bridge_text or
            re.search(r'GSM service uplink sapi=0 pd=03 message=07', log)):
        raise RuntimeError('cancelled incoming SIP call falsely answered')
    match = re.search(r'SIP bridge ended (\{[^\n]+\})', bridge_text)
    if not match:
        raise RuntimeError('cancelled incoming call never completed handset release')
    counts = json.loads(match[1])
    if any(counts.get(name) != 0 for name in ('uplink', 'downlink', 'pcm_transmitted', 'pcm_received')):
        raise RuntimeError('cancelled incoming SIP call falsely claimed media')
    cursor = 0
    if (len(re.findall(r'gsm_call_adapter: termination id=1 cause=16 result=accepted', log)) != 1 or
            re.search(r'gsm_call_adapter: termination id=1 .*result=rejected', log)):
        raise RuntimeError('cancelled incoming call did not clear exactly once')
    for pattern in (
            r'gsm_call_adapter: incoming state id=1 epoch=1 phase=paging',
            r'GSM service downlink kind=9 sapi=0 pd=03 message=05',
            r'gsm_call_adapter: incoming state id=1 epoch=1 phase=alerting',
            r'gsm_call_adapter: termination id=1 cause=16 result=accepted',
            r'GSM service downlink kind=13 sapi=0 pd=03 message=25',
            r'GSM service uplink sapi=0 pd=03 message=2a',
            r'LAPDm service Channel Release acknowledged',
            r'gsm_call_adapter: incoming state id=1 epoch=1 phase=ended'):
        match = re.search(pattern, log[cursor:])
        if not match:
            raise RuntimeError(f'missing cancelled incoming call checkpoint: {pattern}')
        cursor += match.end()
    (root / 'sip-result.json').write_text(json.dumps({
        'scope': f'{product} HLE incoming SIP CANCEL while alerting; no Answer/connection/media',
        'sip_status': 487, 'media': counts, 'passed': True}, indent=2) + '\n')
    print('OK - SIP CANCEL before Answer cleared the ringing handset without connection/media')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pjsua', type=Path, required=True)
    parser.add_argument('--run-dir', type=Path, required=True)
    parser.add_argument('--sip-port', type=int, default=25100)
    parser.add_argument('--http-port', type=int, default=18100)
    parser.add_argument('--incoming', action='store_true')
    parser.add_argument('--record-media', action='store_true')
    parser.add_argument('--product', choices=('3210', '3310'), default='3210')
    parser.add_argument('--calls', type=int, choices=(1, 2), default=1)
    parser.add_argument('--cancel-incoming', action='store_true')
    parser.add_argument('--restore-call', action='store_true')
    parser.add_argument('--restore-phase', choices=('connected', 'alerting'), default='connected')
    parser.add_argument('--restore-idle', action='store_true')
    parser.add_argument('--restore-outgoing', action='store_true')
    parser.add_argument('--sip-response', type=int, choices=(180, 200, 480, 486), default=200)
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.calls != 1 and (args.product != '3310' or args.incoming or args.sip_response not in (480, 486)):
        parser.error('two-call fixture requires 3310 outgoing SIP failure/redial')
    if args.incoming and args.sip_response != 200:
        parser.error('--sip-response failure fixtures are outgoing only')
    if args.cancel_incoming and not args.incoming:
        parser.error('--cancel-incoming requires --incoming')
    if args.restore_call and (not args.incoming or args.cancel_incoming):
        parser.error('--restore-call requires an answered incoming call')
    if args.restore_idle and (not args.incoming or args.cancel_incoming or args.restore_call):
        parser.error('--restore-idle requires a fresh incoming call after idle restoration')
    if args.restore_outgoing and (args.incoming or args.restore_idle or args.restore_call or args.sip_response not in (180, 200)):
        parser.error('--restore-outgoing requires an outgoing provisional or accepted SIP response')
    if args.sip_response == 180 and not args.restore_outgoing:
        parser.error('provisional-only response requires --restore-outgoing')
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
