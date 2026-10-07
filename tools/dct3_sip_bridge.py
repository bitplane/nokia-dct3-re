#!/usr/bin/env python3
"""Outgoing MAME host calls to an external PJSUA2 SIP endpoint.

SIP/RTP and wall-clock audio stay outside MAME. Incoming SIP, SMS and USSD
are not implemented by this backend; use the existing laboratory endpoints.
"""

import argparse
import asyncio
from collections import deque
import json
import queue
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


class SipEndpoint:
    def __init__(self, pj, port):
        self.pj = pj
        self.events = deque()
        self.call = None
        self.media_port = None
        self.media = MediaQueues()
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
                call = pj.Call(account, parameter.callId)
                response = pj.CallOpParam()
                response.statusCode = 486
                call.answer(response)

        self.account = Account()
        account_config = pj.AccountConfig()
        account_config.idUri = f'sip:dct3@127.0.0.1:{port}'
        self.account.create(account_config)

    def dial(self, destination, identity):
        pj = self.pj
        owner = self
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
        self.call = Call(self.account)
        self.call.makeCall(destination, pj.CallOpParam(True))

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
    epoch = None
    decision = False
    connected = False
    blocked_restore = False
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
                                blocked_restore = True
                                connected = False
                                identity = None
                                codec.close()
                                codec = GsmFrCodec()
                            epoch = event.get('epoch')
                        elif event.get('epoch') == epoch:
                            incoming_identity = (epoch, event.get('request_id'))
                            if kind == 'outgoing_call':
                                if identity is None and not blocked_restore:
                                    digits = event.get('digits')
                                    if not isinstance(digits, str) or not digits.isascii() or not digits.isdigit():
                                        raise RuntimeError('invalid outgoing digits')
                                    identity = incoming_identity
                                    endpoint.dial(args.destination, identity)
                                    print(f'SIP dial identity={identity} digits={digits}', flush=True)
                            elif kind == 'outgoing_call_state':
                                if blocked_restore and event.get('phase') != 'ended':
                                    await websocket.send(json.dumps({
                                        'type': 'outgoing_call_terminate', 'epoch': epoch,
                                        'request_id': event['request_id'], 'cause': 41}))
                                elif incoming_identity == identity:
                                    if event.get('phase') == 'connected':
                                        connected = True
                                        downlink_sequence = event['media_downlink_sequence']
                                    elif event.get('phase') == 'ended':
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
                                if event.get('phase') == 'ended':
                                    blocked_restore = False
                            elif (kind == 'outgoing_call_media_uplink' and connected and
                                  incoming_identity == identity):
                                sequence = event.get('sequence')
                                if isinstance(sequence, int) and sequence > uplink_sequence:
                                    uplink_sequence = sequence
                                    if event.get('good') is True:
                                        endpoint.media.put(endpoint.media.uplink, codec.decode(bytes.fromhex(event['frame'])))
                                        counts['uplink'] += 1
                    while endpoint.events:
                        call_identity, phase, status = endpoint.events.popleft()
                        if call_identity != identity or blocked_restore:
                            continue
                        response = {'epoch': identity[0], 'request_id': identity[1]}
                        if phase == 'confirmed' and not decision:
                            response.update(type='outgoing_call_decision', decision='connect')
                            decision = True
                        elif phase == 'disconnected':
                            connected = False
                            if decision:
                                response.update(type='outgoing_call_terminate', cause=16)
                            else:
                                response.update(type='outgoing_call_decision',
                                                decision='busy' if status in (486, 600) else 'no_answer')
                                decision = True
                        else:
                            continue
                        await websocket.send(json.dumps(response))
                        print(f'SIP {phase} status={status} identity={identity}', flush=True)
                    while connected:
                        try:
                            pcm = endpoint.media.downlink.get_nowait()
                        except queue.Empty:
                            break
                        await websocket.send(json.dumps({
                            'type': 'outgoing_call_media_downlink',
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
