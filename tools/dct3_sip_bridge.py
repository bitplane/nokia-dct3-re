#!/usr/bin/env python3
"""MAME host calls to an external PJSUA2 SIP endpoint.

SIP/RTP and wall-clock audio stay outside MAME. SMS and USSD are not implemented
by this backend; use the existing laboratory endpoints.
"""

import argparse
import asyncio
from collections import deque
import json
import queue
import re
import time

try:
    from tools.gsm_fr_codec import GsmFrCodec
except ModuleNotFoundError:
    from gsm_fr_codec import GsmFrCodec


class MediaQueues:
    def __init__(self):
        self.uplink = queue.Queue(maxsize=8)
        self.downlink = queue.Queue(maxsize=8)
        self.dropped = 0
        self.transmitted = 0
        self.received = 0

    def put(self, destination, pcm):
        if len(pcm) != 320:
            return
        try:
            destination.put_nowait(pcm)
        except queue.Full:
            self.dropped += 1


def incoming_caller(remote_uri):
    # PJSUA2 supplies a parsed dialog's serialized name-address. A display name
    # may itself contain angle brackets, so inspect the final URI, not its text.
    uri = remote_uri.strip()
    if uri.endswith('>') and '<' in uri:
        uri = uri.rsplit('<', 1)[1][:-1]
    match = re.fullmatch(r'sip:([0-9]{1,20})@[^<>\s]+', uri)
    return match[1] if match else None


def failure_messages(identity, status):
    # RFC 3398 8.2.6.1, SIP -> ISDN (not the inverse table). Unsupported
    # statuses use a declared temporary-failure policy, not a fabricated answer.
    cause = {403: 21, 404: 1, 408: 102, 480: 18, 486: 17,
             500: 41, 503: 41, 600: 17, 603: 21, 604: 1}.get(status, 41)
    base = {'epoch': identity[0], 'request_id': identity[1]}
    replies = [{**base, 'type': 'outgoing_call_decision',
                'decision': 'busy' if cause == 17 else 'no_answer'}]
    if cause != 17:
        # no_answer suppresses CONNECT, but does not itself clear CC/RR. The
        # session accepts this following termination and owns its release order.
        replies.append({**base, 'type': 'outgoing_call_terminate', 'cause': cause})
    return replies


class SipEndpoint:
    def __init__(self, pj, port):
        self.pj = pj
        self.events = deque()
        self.call = None
        self.call_identity = None
        self.media_port = None
        self.media = MediaQueues()
        self.incoming_epoch = None
        self.incoming_enabled = False
        self.next_incoming_request = 1
        self.endpoint = pj.Endpoint()
        self.endpoint.libCreate()
        config = pj.EpConfig()
        config.uaConfig.threadCnt = 0
        config.uaConfig.mainThreadOnly = True
        config.medConfig.clockRate = 8000
        config.medConfig.sndClockRate = 8000
        config.medConfig.channelCount = 1
        config.medConfig.audioFramePtime = 20
        config.medConfig.noVad = True
        self.endpoint.libInit(config)
        transport = pj.TransportConfig()
        transport.port = port
        transport.boundAddress = '127.0.0.1'
        self.endpoint.transportCreate(pj.PJSIP_TRANSPORT_UDP, transport)
        self.endpoint.libStart()
        self.endpoint.audDevManager().setNullDev()

        class Account(pj.Account):
            def onIncomingCall(account, parameter):
                response = pj.CallOpParam()
                if (not self.incoming_enabled or self.incoming_epoch is None or
                        (self.call and self.call.isActive())):
                    call = pj.Call(account, parameter.callId)
                    response.statusCode = 486
                    call.answer(response)
                    return
                identity = (self.incoming_epoch, self.next_incoming_request)
                # Call destruction hangs up the native dialog. Construct the
                # owned callback object once; never inspect via a temporary Call.
                self._new_call(identity, parameter.callId)
                caller = incoming_caller(self.call.getInfo().remoteUri)
                if caller is None:
                    response.statusCode = 484
                    self.call.answer(response)
                    return
                self.next_incoming_request += 1
                self.incoming_enabled = False
                response.statusCode = 180
                self.call.answer(response)
                self.events.append((identity, 'invite', caller))

        self.account = Account()
        account_config = pj.AccountConfig()
        account_config.idUri = f'sip:dct3@127.0.0.1:{port}'
        self.account.create(account_config)

    def _new_call(self, identity, call_id=-1):
        pj = self.pj
        owner = self
        self.call = None
        self.call_identity = identity
        self.media = MediaQueues()
        media = self.media

        class Port(pj.AudioMediaPort):
            def onFrameRequested(port, frame):
                try:
                    pcm = media.uplink.get_nowait()
                    media.transmitted += 1
                except queue.Empty:
                    pcm = bytes(320)
                frame.type = pj.PJMEDIA_TYPE_AUDIO
                frame.buf.assign_from_bytes(pcm)

            def onFrameReceived(port, frame):
                if frame.type == pj.PJMEDIA_TYPE_AUDIO and frame.buf.size() == 320:
                    pcm = bytearray(320)
                    frame.buf.copy_to_bytearray(pcm)
                    media.put(media.downlink, bytes(pcm))
                    media.received += 1

        class Call(pj.Call):
            def onCallState(call, _):
                info = call.getInfo()
                if info.state == pj.PJSIP_INV_STATE_CONFIRMED:
                    owner.events.append((identity, 'confirmed', info.lastStatusCode))
                elif info.state == pj.PJSIP_INV_STATE_DISCONNECTED:
                    owner.events.append((identity, 'disconnected', info.lastStatusCode))

            def onCallMediaState(call, _):
                info = call.getInfo()
                for entry in info.media:
                    if entry.type == pj.PJMEDIA_TYPE_AUDIO and entry.status == pj.PJSUA_CALL_MEDIA_ACTIVE:
                        if owner.media_port is None:
                            owner.media_port = Port()
                            format = pj.MediaFormatAudio()
                            format.type = pj.PJMEDIA_TYPE_AUDIO
                            format.clockRate = 8000
                            format.channelCount = 1
                            format.bitsPerSample = 16
                            format.frameTimeUsec = 20000
                            owner.media_port.createPort('DCT3 host PCM', format)
                        audio = call.getAudioMedia(entry.index)
                        audio.startTransmit(owner.media_port)
                        owner.media_port.startTransmit(audio)

        self.media_port = None
        self.call = Call(self.account, call_id)

    def dial(self, destination, identity):
        self._new_call(identity)
        pj = self.pj
        self.call.makeCall(destination, pj.CallOpParam(True))

    def answer(self, status):
        parameter = self.pj.CallOpParam()
        parameter.statusCode = status
        self.call.answer(parameter)

    def release(self, identity):
        if identity == self.call_identity:
            self.call = None
            self.media_port = None
            self.call_identity = None

    def hangup(self):
        if self.call and self.call.isActive():
            self.call.hangup(self.pj.CallOpParam())

    def poll(self):
        self.endpoint.libHandleEvents(0)

    def close(self):
        self.hangup()
        # PJSUA2 wrappers unregister native ports/call data in their destructors.
        # Destroy them while the endpoint is still alive, not after libDestroy.
        self.call = None
        self.media_port = None
        if self.account:
            self.account.shutdown()
        self.account = None
        self.endpoint.libDestroy()


async def bridge(args, pj):
    import websockets
    endpoint = SipEndpoint(pj, args.sip_port)
    identity = None
    direction = 'outgoing'
    epoch = None
    decision = False
    connected = False
    blocked_restore = False
    restored_identity = None
    registered = False
    downlink_sequence = 0
    uplink_sequence = -1
    counts = {'uplink': 0, 'downlink': 0}
    codec = GsmFrCodec()
    started = time.monotonic()
    try:
        async with websockets.connect(args.url, ping_interval=None, max_size=4096) as websocket:
            receive = asyncio.create_task(websocket.recv())
            try:
                while True:
                    endpoint.poll()
                    if receive.done():
                        event = json.loads(receive.result())
                        receive = asyncio.create_task(websocket.recv())
                        kind = event.get('type')
                        if kind == 'call_adapter_ready':
                            if event.get('protocol_version') != 1:
                                raise RuntimeError('unsupported host protocol')
                            if epoch is not None and epoch != event.get('epoch'):
                                endpoint.hangup()
                                blocked_restore = event.get('calls_idle') is not True
                                restored_identity = None
                                connected = False
                                identity = None
                                decision = False
                                uplink_sequence = -1
                                counts = {'uplink': 0, 'downlink': 0}
                                codec.close()
                                codec = GsmFrCodec()
                                print(f'SIP epoch changed old={epoch} new={event.get("epoch")}; clearing external dialog', flush=True)
                                if not blocked_restore:
                                    print(f'SIP idle snapshot accepted epoch={event.get("epoch")}', flush=True)
                            epoch = event.get('epoch')
                            endpoint.incoming_epoch = epoch
                            endpoint.incoming_enabled = False
                            registered = False
                        elif event.get('epoch') == epoch:
                            incoming_identity = (epoch, event.get('request_id'))
                            if kind == 'network_state':
                                registered = event.get('registered') is True
                                endpoint.incoming_enabled = (
                                    registered and identity is None and not blocked_restore)
                                if endpoint.incoming_enabled:
                                    print(f'SIP registration ready epoch={epoch}', flush=True)
                            elif kind == 'outgoing_call':
                                if blocked_restore:
                                    if restored_identity is not None:
                                        continue
                                    restored_identity = incoming_identity
                                    # A pending outgoing request has no call-state
                                    # event yet. Supply the ordinary non-connecting
                                    # decision before its correlated termination.
                                    replies = failure_messages(incoming_identity, 503)
                                    if event.get('decision_pending') is False:
                                        replies = replies[1:]
                                    for reply in replies:
                                        await websocket.send(json.dumps(reply))
                                elif identity is None:
                                    digits = event.get('digits')
                                    if not isinstance(digits, str) or not digits.isascii() or not digits.isdigit():
                                        raise RuntimeError('invalid outgoing digits')
                                    identity = incoming_identity
                                    direction = 'outgoing'
                                    endpoint.incoming_enabled = False
                                    endpoint.dial(args.destination, identity)
                                    print(f'SIP dial identity={identity} digits={digits}', flush=True)
                            elif kind in ('outgoing_call_state', 'incoming_call_state'):
                                if blocked_restore and event.get('phase') != 'ended':
                                    # Clear once, not once per republished phase.
                                    if restored_identity is not None:
                                        continue
                                    restored_identity = incoming_identity
                                    await websocket.send(json.dumps({
                                        'type': kind.replace('_state', '_terminate'), 'epoch': epoch,
                                        'request_id': event['request_id'], 'cause': 41}))
                                elif incoming_identity == identity and kind == f'{direction}_call_state':
                                    if event.get('phase') == 'connected':
                                        connected = True
                                        downlink_sequence = event['media_downlink_sequence']
                                        if direction == 'incoming' and not decision:
                                            endpoint.answer(200)
                                            decision = True
                                            print(f'SIP physical answer identity={identity}', flush=True)
                                    elif event.get('phase') in ('ended', 'expired'):
                                        endpoint.hangup()
                                        counts.update(pcm_transmitted=endpoint.media.transmitted,
                                                      pcm_received=endpoint.media.received,
                                                      dropped=endpoint.media.dropped)
                                        print(f'SIP bridge ended {json.dumps(counts)}', flush=True)
                                        if args.once:
                                            if min(counts[name] for name in (
                                                    'uplink', 'downlink', 'pcm_transmitted',
                                                    'pcm_received')) < args.require_frames:
                                                raise RuntimeError('call ended without required bidirectional media')
                                            return
                                        identity = None
                                        connected = decision = False
                                        uplink_sequence = -1
                                        counts = {'uplink': 0, 'downlink': 0}
                                        codec.close()
                                        codec = GsmFrCodec()
                                        endpoint.incoming_enabled = registered and not blocked_restore
                                if (blocked_restore and event.get('phase') == 'ended' and
                                        incoming_identity == restored_identity):
                                    print(f'SIP restored call cleared identity={incoming_identity}', flush=True)
                                    blocked_restore = False
                                    restored_identity = None
                                    endpoint.incoming_enabled = registered and identity is None
                                    if args.once:
                                        return
                            elif (kind == f'{direction}_call_media_uplink' and connected and
                                  incoming_identity == identity):
                                sequence = event.get('sequence')
                                if isinstance(sequence, int) and sequence > uplink_sequence:
                                    uplink_sequence = sequence
                                    if event.get('good') is True:
                                        endpoint.media.put(endpoint.media.uplink, codec.decode(bytes.fromhex(event['frame'])))
                                        counts['uplink'] += 1
                    while endpoint.events:
                        call_identity, phase, status = endpoint.events.popleft()
                        if phase == 'disconnected':
                            endpoint.release(call_identity)
                            if identity is None:
                                endpoint.incoming_enabled = registered and not blocked_restore
                        if phase == 'invite' and identity is None and not blocked_restore:
                            identity = call_identity
                            direction = 'incoming'
                            await websocket.send(json.dumps({
                                'type': 'incoming_call', 'epoch': identity[0],
                                'request_id': identity[1], 'caller': status}))
                            print(f'SIP incoming identity={identity} caller={status}', flush=True)
                            continue
                        if call_identity != identity or blocked_restore:
                            continue
                        response = {'epoch': identity[0], 'request_id': identity[1]}
                        followup = []
                        if phase == 'confirmed' and direction == 'incoming':
                            print(f'SIP confirmed status={status} identity={identity}', flush=True)
                            continue
                        if phase == 'confirmed' and not decision:
                            response.update(type=f'{direction}_call_decision', decision='connect')
                            decision = True
                        elif phase == 'disconnected':
                            connected = False
                            if decision or direction == 'incoming':
                                response.update(type=f'{direction}_call_terminate', cause=16)
                            else:
                                response, *followup = failure_messages(identity, status)
                                decision = True
                        else:
                            continue
                        await websocket.send(json.dumps(response))
                        for reply in followup:
                            await websocket.send(json.dumps(reply))
                        print(f'SIP {phase} status={status} identity={identity}', flush=True)
                    while connected:
                        try:
                            pcm = endpoint.media.downlink.get_nowait()
                        except queue.Empty:
                            break
                        await websocket.send(json.dumps({
                            'type': f'{direction}_call_media_downlink',
                            'epoch': identity[0], 'request_id': identity[1],
                            'sequence': downlink_sequence,
                            'source_time_us': int((time.monotonic() - started) * 1000000),
                            'frame': codec.encode(pcm).hex()}))
                        downlink_sequence += 1
                        counts['downlink'] += 1
                    await asyncio.sleep(0.005)
            finally:
                receive.cancel()
                await asyncio.gather(receive, return_exceptions=True)
    finally:
        codec.close()
        endpoint.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='ws://127.0.0.1:18080/nokia/dct3/calls')
    parser.add_argument('--destination', required=True, help='explicit SIP destination; handset digits are logged, not rewritten into a URI')
    parser.add_argument('--sip-port', type=int, default=25070)
    parser.add_argument('--once', action='store_true')
    parser.add_argument('--require-frames', type=int, default=0)
    args = parser.parse_args()
    if not args.destination.startswith('sip:') or any(c in args.destination for c in '\r\n'):
        parser.error('--destination must be a SIP URI without line breaks')
    if args.require_frames < 0:
        parser.error('--require-frames must be nonnegative')
    try:
        import pjsua2 as pj
        asyncio.run(bridge(args, pj))
    except (ImportError, OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f'FAIL - {error}\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
