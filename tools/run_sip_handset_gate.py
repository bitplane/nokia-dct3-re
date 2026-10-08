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
    if product in ('6210', '6250', '8210', '8850', '8890'):
        # Content is checked with the product's physical-number decoder below.
        return r'GSM service uplink sapi=0 pd=03 message=05 length=\d+ data=[0-9a-f]+'
    frame = {
        '3210': '03450401a05e0581551532f4150101',
        '3310': '03450404600200815e0581551532f4a2150101',
        '3330': '03450404600200815e0581551532f4150101',
        '3410': '03450401a05e0581551532f4150101',
        '5210': '03450401a05e0581551532f4150101',
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
                if args.ready_file:
                    for _ in range(900):
                        if args.ready_file.is_file():
                            break
                        if handset.returncode is not None or bridge.returncode is not None:
                            raise RuntimeError('handset or bridge exited before incoming ready fixture')
                        await asyncio.sleep(0.05)
                    else:
                        raise RuntimeError('incoming handset ready fixture did not appear')
                remote = await asyncio.create_subprocess_exec(*remote_command,
                    '--id', f'sip:5551234@127.0.0.1:{args.sip_port}',
                    f'sip:dct3@127.0.0.1:{args.sip_port + 1}',
                    stdin=asyncio.subprocess.PIPE, stdout=remote_log, stderr=asyncio.subprocess.STDOUT)
                processes.append(remote)
                if args.cancel_incoming:
                    for _ in range(400):
                        if re.search(r'incoming state id=1 epoch=\d+ phase=alerting',
                                     (root / 'error.log').read_text(errors='replace')):
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


def verify_downlink_lifecycle(log):
    closure = re.search(
        r'gsm_call_adapter: ((?:incoming )?state) id=1 epoch=(\d+) phase=media_closed\b', log)
    packets = list(re.finditer(
        r'gsm_call_adapter: media direction=downlink id=1 sequence=(\d+) '
        r'result=(accepted|rejected)([^\n]*)', log))
    if [int(packet[1]) for packet in packets] != list(range(len(packets))):
        raise RuntimeError('SIP downlink sequence is not contiguous')
    accepted = 0
    closing = False
    for packet in packets:
        if packet[2] == 'accepted':
            if closing or (closure and packet.start() > closure.start()):
                raise RuntimeError('handset accepted media after closure')
            accepted += 1
            continue
        closing = True
        if not re.search(r'\breason=session_closed\b', packet[3]):
            raise RuntimeError('unclassified or active-session SIP downlink rejection')
        # Media closes before final CC/RR completion. Require the explicit
        # correlated boundary, not a guessed interval around final release.
        if closure is None or closure.start() >= packet.start():
            raise RuntimeError('closed-session rejection precedes media closure')
        boundary = re.search(
            r'gsm_call_adapter: ' + re.escape(closure[1]) + r' id=1 epoch=' +
            re.escape(closure[2]) + r' phase=ended\b', log[packet.end():])
        if boundary is None:
            raise RuntimeError('closed-session rejection lacks correlated completion')
        if 'LAPDm service Channel Release acknowledged' not in log[:packet.end() + boundary.end()]:
            raise RuntimeError('closed-session rejection lacks radio release')
    if accepted < 100:
        raise RuntimeError('handset did not accept a sustained ordered SIP downlink')


def verify_success(root, remote_text, args):
    if 'state changed to CONFIRMED' not in remote_text or not re.search(
            r'DISCONNECTED \[reason=200 \((?:OK|Normal call clearing)\)\]', remote_text):
        raise RuntimeError('remote SIP call did not confirm and release normally')
    bridge_text = (root / 'sip-bridge.log').read_text(errors='replace')
    number = {'6210': '1234567', '6250': '123', '8210': '1234567', '8890': '1234567'}.get(
        getattr(args, 'product', '3210'), '5551234')
    identity_marker = 'caller=5551234' if args.incoming else f'digits={number}'
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
    if not args.incoming:
        try:
            from tools.radio_outgoing_call_trace_check import SETUP, decode_called_digits
        except ModuleNotFoundError:
            from radio_outgoing_call_trace_check import SETUP, decode_called_digits
        setups = list(SETUP.finditer(log))
        if len(setups) != 1 or any(
                len(bytes.fromhex(match['data'])) != int(match['length']) or
                decode_called_digits(bytes.fromhex(match['data'])) != number for match in setups):
            raise RuntimeError('answered SIP SETUP differs from physically dialed number')
    verify_downlink_lifecycle(log)
    epoch = 2 if args.restore_idle else 1
    if args.restore_idle:
        if ('SIP idle snapshot accepted epoch=2' not in bridge_text or
                bridge_text.count('SIP incoming identity=') != 1):
            raise RuntimeError('idle restoration did not admit exactly one fresh SIP call')
        for marker in ('SIP epoch changed old=1 new=2',
                       'SIP incoming identity=(2, 1)',
                       'SIP physical answer identity=(2, 1)',
                       'SIP confirmed status=200 identity=(2, 1)'):
            if marker not in bridge_text:
                raise RuntimeError(f'idle restoration lacks fresh-epoch SIP evidence: {marker}')
    cursor = 0
    product = getattr(args, 'product', '3210')
    if product == '8850' and not args.incoming:
        try:
            from tools.noki8850_outgoing_call_check import verify as verify_8850_call
        except ModuleNotFoundError:
            from noki8850_outgoing_call_check import verify as verify_8850_call
        verify_8850_call(log, number=number)
    if product == '8210':
        if args.incoming:
            try:
                from tools.noki8210_incoming_call_check import verify as verify_8210_call
            except ModuleNotFoundError:
                from noki8210_incoming_call_check import verify as verify_8210_call
            verify_8210_call(log, configured_carrier=True)
        else:
            try:
                from tools.noki8210_outgoing_call_check import verify as verify_8210_call
            except ModuleNotFoundError:
                from noki8210_outgoing_call_check import verify as verify_8210_call
            # Physical End uses handset DISCONNECT, network RELEASE, then
            # RELEASE COMPLETE. Reuse the full own-product lifecycle.
            verify_8210_call(log, number=number, configured_carrier=True)
    # Exact encodings from these acceptance fixtures, including their observed
    # CC sequence bit; not a claim that the bit is a fixed product property.
    connect_data = '8307' if product in ('3310', '3330', '3410', '5210') else '8347'
    answer_key = 'send' if product in ('3410', '5210') else 'enter'
    # These release checks treat CC sequence bit 6 independently of the message body.
    release_complete_data = ('03(?:2a|6a)0802e0d1' if product in ('3330', '3410', '5210') else
                             '036a0802e0d1' if product == '3310' else '032a0802e0d1')
    patterns = (
            rf'gsm_call_adapter: incoming state id=1 epoch={epoch} phase=paging',
            r'GSM service downlink kind=9 sapi=0 pd=03 message=05',
            (r'sip_state: physical Answer after idle restoration' if args.restore_idle
             else rf'input-press: t=[0-9.]+ name={answer_key}(?:\s|$)'),
            rf'GSM service uplink sapi=0 pd=03 message=07 length=2 data={connect_data}(?:\s|$)',
            rf'gsm_call_adapter: incoming state id=1 epoch={epoch} phase=connected',
            r'gsm_call_adapter: termination id=1 cause=16 result=accepted',
            rf'GSM service uplink sapi=0 pd=03 message=2a .*data={release_complete_data}(?:\s|$)',
            rf'gsm_call_adapter: incoming state id=1 epoch={epoch} phase=ended',
    ) if args.incoming else (
            outgoing_setup_pattern(product),
            r'GSM service downlink kind=12 sapi=0 pd=03 message=07',
            r'GSM service uplink sapi=0 pd=03 message=0f .*data=030f',
            r'GSM service uplink sapi=0 pd=03 message=2d .*data=03(?:2d|6d)',
            r'LAPDm service Channel Release acknowledged')
    if args.restore_idle:
        patterns = (r'sip_state: saved', r'sip_state: restored') + patterns
    for pattern in (() if product == '8210' or
                    (product == '8850' and not args.incoming) else patterns):
        match = re.search(pattern, log[cursor:])
        if not match:
            raise RuntimeError(f'missing ordered firmware call checkpoint: {pattern}')
        cursor += match.end()
    (root / 'sip-result.json').write_text(json.dumps({
        'scope': f'{getattr(args, "product", "3210")} research-HLE physical {"incoming" if args.incoming else "outgoing"} SIP signaling and media transport; not native DSP speech',
        ('caller' if args.incoming else 'dialed_digits'): '5551234' if args.incoming else number,
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
            re.search(r'GSM service downlink kind=12 sapi=0 pd=03 message=07', log) or
            re.search(r'gsm_call_adapter: media direction=\w+ id=\d+ .*result=accepted', log)):
        raise RuntimeError('failed SIP call falsely connected')
    number = {'6210': '1234567', '6250': '123', '8210': '1234567', '8890': '1234567'}.get(product, '5551234')
    requests = re.findall(rf'gsm_call_adapter: request id=(\d+) epoch=1 digits={number}\b', log)
    try:
        from tools.radio_outgoing_call_trace_check import SETUP, decode_called_digits
    except ModuleNotFoundError:
        from radio_outgoing_call_trace_check import SETUP, decode_called_digits
    setups = list(SETUP.finditer(log))
    if len(setups) != calls or any(
            len(bytes.fromhex(match['data'])) != int(match['length']) or
            decode_called_digits(bytes.fromhex(match['data'])) != number for match in setups):
        raise RuntimeError(f'{product} SETUP differs from physically dialed number')
    if requests != [str(number) for number in range(1, calls + 1)]:
        raise RuntimeError('SIP failure left an unexpected or unhandled handset attempt')
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
        patterns = [outgoing_setup_pattern(product),
                    rf'gsm_call_adapter: request id={request_id} epoch=1 digits={number}\b']
        rejection_cause = {403: 21, 404: 1, 408: 102, 480: 18, 503: 41}.get(status)
        if rejection_cause is not None:
            patterns += [rf'outgoing decision consumed id={request_id} outcome=2(?!\d)',
                         rf'outgoing termination consumed id={request_id} cause={rejection_cause}(?!\d)']
        if status in (486, 600):
            patterns += [rf'outgoing decision consumed id={request_id} outcome=1(?!\d)']
        patterns += [
            r'GSM service downlink kind=13 sapi=0 pd=03 message=25 length=5',
            # Bit 6 is the CC send-sequence flag; it is not another primitive.
            r'GSM service uplink sapi=0 pd=03 message=2d .*data=03(?:2d|6d)',
            r'GSM service downlink kind=26 sapi=0 pd=03 message=2a',
            r'LAPDm service Channel Release acknowledged']
        patterns += [rf'gsm_call_adapter: state id={request_id} epoch=1 phase=ended']
        for pattern in patterns:
            match = re.search(pattern, log[cursor:])
            if not match:
                raise RuntimeError(f'missing SIP-failure firmware checkpoint: {pattern}')
            cursor += match.end()
    (root / 'sip-result.json').write_text(json.dumps({
        'scope': f'{product} HLE physical outgoing SIP failure and firmware release; no connection/media',
        'sip_status': status, 'completed_calls': calls,
        'media': media[0] if calls == 1 else media, 'passed': True}, indent=2) + '\n')
    print(f'OK - SIP {status} became a correlated handset failure and clean release without CONNECT/media')


def verify_outgoing_restore(root, remote_text, connected=False, product='3210'):
    bridge = (root / 'sip-bridge.log').read_text(errors='replace')
    log = (root / 'error.log').read_text(errors='replace')
    number = {'6210': '1234567', '6250': '123', '8210': '1234567', '8890': '1234567'}.get(product, '5551234')
    if not re.search(outgoing_setup_pattern(product), log):
        raise RuntimeError('restored outgoing call lacks the product SETUP frame')
    if product in ('6210', '6250', '8210', '8850', '8890'):
        try:
            from tools.radio_outgoing_call_trace_check import SETUP, decode_called_digits
        except ModuleNotFoundError:
            from radio_outgoing_call_trace_check import SETUP, decode_called_digits
        setups = list(SETUP.finditer(log))
        if len(setups) != 1 or any(
                len(bytes.fromhex(item['data'])) != int(item['length']) or
                decode_called_digits(bytes.fromhex(item['data'])) != number for item in setups):
            raise RuntimeError('restored outgoing SETUP differs from physical digits')
    markers = (('state changed to CONFIRMED', 'Request msg BYE/') if connected else
               ('Response msg 180/INVITE/', 'Request msg CANCEL/', 'Response msg 487/INVITE/'))
    remote_cursor = 0
    for marker in markers:
        position = remote_text.find(marker, remote_cursor)
        if position < 0:
            raise RuntimeError(f'missing real pending SIP release: {marker}')
        remote_cursor = position + len(marker)
    if not connected and ('state changed to CONFIRMED' in remote_text or 'SIP confirmed' in bridge or
            re.search(r'GSM service downlink kind=12 sapi=0 pd=03 message=07', log) or
            re.search(r'gsm_call_adapter: media direction=\w+ id=1 .*result=accepted', log)):
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
    patterns = (rf'gsm_call_adapter: request id=1 epoch=1 digits={number}\b',)
    if connected:
        patterns += (r'gsm_call_adapter: state id=1 epoch=1 phase=connected',)
    patterns += (
                    r'sip_state: saved', r'sip_state: restored',
                    rf'gsm_call_adapter: request id=1 epoch=2 digits={number}\b')
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
        if not re.search(r'state changed to CONFIRMED.*?Request msg BYE/',
                         remote_text, re.DOTALL):
            raise RuntimeError('restoration did not close a real connected SIP dialog')
    else:
        if not re.search(r'Response msg [4-6][0-9]{2}/INVITE/', remote_text):
            raise RuntimeError('restoration did not reject the real alerting SIP INVITE')
        if ('state changed to CONFIRMED' in remote_text or 'SIP physical answer' in bridge or
                'SIP confirmed' in bridge or
                re.search(r'GSM service uplink sapi=0 pd=03 message=07', log) or
                re.search(r'gsm_call_adapter: media direction=\w+ id=1 .*result=accepted', log)):
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
    physical_answer = (
        r'8210_incoming_physical: action=Call / Send',
        r'8210_keypad_decoded: key=0e\b',
        r'GSM service uplink sapi=0 pd=03 message=07 length=2',
    ) if product == '8210' and phase == 'connected' else ()
    for pattern in physical_answer + (
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
    calls = re.findall(r'gsm_call_adapter: incoming state id=1 epoch=(\d+) phase=paging', log)
    if len(calls) != 1:
        raise RuntimeError('cancelled incoming call lacks one fresh paging identity')
    epoch = int(calls[0])
    if ('Request msg CANCEL/' not in remote_text or 'Response msg 487/INVITE/' not in remote_text or
            f'SIP disconnected status=487 identity=({epoch}, 1)' not in bridge_text):
        raise RuntimeError('missing real SIP CANCEL/487 exchange')
    if ('state changed to CONFIRMED' in remote_text or 'SIP confirmed' in bridge_text or
            'SIP physical answer' in bridge_text or
            re.search(r'gsm_call_adapter: (?:incoming )?state id=\d+ epoch=\d+ phase=connected', log) or
            re.search(r'GSM service uplink sapi=0 pd=03 message=07', log)):
        raise RuntimeError('cancelled incoming SIP call falsely answered')
    if re.search(r'gsm_call_adapter: media direction=\w+ id=\d+ .*result=accepted', log):
        raise RuntimeError('cancelled incoming SIP call accepted handset media')
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
            rf'gsm_call_adapter: incoming state id=1 epoch={epoch} phase=paging',
            r'GSM service downlink kind=9 sapi=0 pd=03 message=05',
            rf'gsm_call_adapter: incoming state id=1 epoch={epoch} phase=alerting',
            r'gsm_call_adapter: termination id=1 cause=16 result=accepted',
            r'GSM service downlink kind=13 sapi=0 pd=03 message=25',
            r'GSM service uplink sapi=0 pd=03 message=2a',
            r'LAPDm service Channel Release acknowledged',
            rf'gsm_call_adapter: incoming state id=1 epoch={epoch} phase=ended'):
        match = re.search(pattern, log[cursor:])
        if not match:
            raise RuntimeError(f'missing cancelled incoming call checkpoint: {pattern}')
        cursor += match.end()
    (root / 'sip-result.json').write_text(json.dumps({
        'scope': f'{product} HLE incoming SIP CANCEL while alerting; no Answer/connection/media',
        'sip_status': 487, 'epoch': epoch, 'media': counts, 'passed': True}, indent=2) + '\n')
    print('OK - SIP CANCEL before Answer cleared the ringing handset without connection/media')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pjsua', type=Path, required=True)
    parser.add_argument('--run-dir', type=Path, required=True)
    parser.add_argument('--sip-port', type=int, default=25100)
    parser.add_argument('--http-port', type=int, default=18100)
    parser.add_argument('--incoming', action='store_true')
    parser.add_argument('--record-media', action='store_true')
    parser.add_argument('--product', choices=('3210', '3310', '3330', '3410', '5210', '6210', '6250', '8210', '8850', '8890'), default='3210')
    parser.add_argument('--ready-file', type=Path,
                        help='wait for a fresh handset readiness artifact before an incoming INVITE')
    parser.add_argument('--calls', type=int, choices=(1, 2), default=1)
    parser.add_argument('--cancel-incoming', action='store_true')
    parser.add_argument('--restore-call', action='store_true')
    parser.add_argument('--restore-phase', choices=('connected', 'alerting'), default='connected')
    parser.add_argument('--restore-idle', action='store_true')
    parser.add_argument('--restore-outgoing', action='store_true')
    parser.add_argument('--sip-response', type=int, choices=(180, 200, 403, 404, 408, 480, 486, 503, 600), default=200)
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.ready_file and not args.incoming:
        parser.error('--ready-file requires --incoming')
    if args.ready_file and args.ready_file.exists():
        parser.error('--ready-file must not already exist before the handset run')
    signaling_failure = (not args.incoming and
                         args.product in ('6210', '6250', '8210', '8850', '8890') and
                         args.sip_response in (480, 486))
    nsm3_media = (args.product == '8210' and not args.cancel_incoming and
                 args.sip_response == 200 and args.calls == 1 and
                 not args.restore_call and
                 not args.restore_idle and not args.restore_outgoing)
    nsm2_media = (args.product == '8850' and not args.incoming and
                 args.sip_response == 200 and args.calls == 1 and
                 not args.cancel_incoming and
                 not args.restore_call and not args.restore_idle and
                 not args.restore_outgoing)
    nsm3_restore = (args.product == '8210' and not args.incoming and
                    args.restore_outgoing and args.sip_response in (180, 200) and
                    not args.record_media and not args.restore_call and not args.restore_idle)
    nsm3_incoming_restore = (args.product == '8210' and args.incoming and
                            args.restore_call and not args.cancel_incoming and
                            not args.record_media and not args.restore_outgoing and not args.restore_idle)
    if args.product in ('6210', '6250', '8210', '8850', '8890') and ((not signaling_failure and not nsm3_media and not nsm2_media and not nsm3_restore and not nsm3_incoming_restore and
            (not args.incoming or not args.cancel_incoming)) or
            (args.record_media and not nsm3_media and not nsm2_media) or (args.restore_call and not nsm3_incoming_restore) or args.restore_idle or (args.restore_outgoing and not nsm3_restore)):
        parser.error(f'{args.product} requires unanswered incoming CANCEL or outgoing 480/486; media is unproved')
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
