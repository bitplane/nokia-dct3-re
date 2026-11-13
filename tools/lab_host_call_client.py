"""Synthetic remote party for the native lab BTS's host call protocol.

It places one incoming call once the handset is registered, answers every
uplink speech frame with one GSM-FR frame of a 1 kHz tone (so the air, not
the wall clock, paces it) and hangs up after a fixed number of uplink frames.
No SIP: dct3_sip_bridge.py is the real remote party.
"""
import asyncio
import json
import math
import struct
import threading

try:
    from tools.gsm_fr_codec import GsmFrCodec
except ModuleNotFoundError:
    from gsm_fr_codec import GsmFrCodec

CALLER = '4420'
TONE_HZ = 1000
MICROPHONE_HZ = 500


def tone_fraction(pcm, hz):
    """Share of the energy at hz across 160-sample blocks (50 Hz bins)."""
    samples = struct.unpack(f'<{len(pcm) // 2}h', pcm)
    total = tone = 0.0
    for start in range(0, len(samples) - 159, 160):
        block = samples[start:start + 160]
        energy = sum(value * value for value in block)
        re = sum(value * math.cos(2 * math.pi * hz * n / 8000) for n, value in enumerate(block))
        im = sum(value * math.sin(2 * math.pi * hz * n / 8000) for n, value in enumerate(block))
        total += energy
        tone += 2 * (re * re + im * im) / 160
    return tone / total if total else 0.0


class HostCallClient(threading.Thread):
    def __init__(self, url, hangup_after=150, timeout=120, quiet_after_hangup=False):
        super().__init__(daemon=True)
        self.quiet_after_hangup = quiet_after_hangup
        self.url = url
        self.hangup_after = hangup_after
        self.timeout = timeout
        self.phases = []
        self.uplink = bytearray()
        self.uplink_good = self.uplink_bad = self.downlink = 0
        self.error = None
        self.received = []

    def run(self):
        try:
            asyncio.run(asyncio.wait_for(self.session(), self.timeout))
        except Exception as error:  # reported by the caller after MAME exits
            self.error = error

    async def connect(self):
        import websockets
        for _ in range(600):
            try:
                return await websockets.connect(self.url, ping_interval=None, max_size=4096)
            except (OSError, websockets.exceptions.InvalidHandshake, websockets.exceptions.InvalidMessage):
                # The HTTP server starts before the machine adds the endpoint.
                await asyncio.sleep(0.1)
        raise RuntimeError('MAME host endpoint did not open')

    async def session(self):
        decoder, encoder = GsmFrCodec(), GsmFrCodec()
        tone = [int(8192 * math.sin(2 * math.pi * TONE_HZ * n / 8000)) for n in range(8)]
        epoch = None
        placed = False
        sequence = 0
        try:
            websocket = await self.connect()
            async with websocket:
                async for raw in websocket:
                    event = json.loads(raw)
                    kind = event.get('type')
                    self.received.append(kind)
                    if kind == 'call_adapter_ready':
                        epoch = event['epoch']
                    elif kind == 'network_state' and event.get('registered') and not placed and epoch is not None:
                        placed = True
                        await websocket.send(json.dumps({'type': 'incoming_call', 'epoch': epoch,
                                                         'request_id': 1, 'caller': CALLER}))
                    elif kind == 'incoming_call_state':
                        self.phases.append(event['phase'])
                        if event['phase'] == 'connected':
                            sequence = event['media_downlink_sequence']
                        if event['phase'] == 'ended':
                            return
                    elif kind == 'incoming_call_media_uplink':
                        if event['good']:
                            self.uplink_good += 1
                            self.uplink.extend(decoder.decode(bytes.fromhex(event['frame'])))
                        else:
                            self.uplink_bad += 1
                        # A fresh encoder each second keeps a steady tone decodable
                        # by a decoder that joined late (it is otherwise all LTP).
                        if sequence % 50 == 0:
                            encoder.close()
                            encoder = GsmFrCodec()
                        if self.uplink_good + self.uplink_bad > self.hangup_after and self.quiet_after_hangup:
                            continue
                        pcm = struct.pack('<160h', *(tone[n % 8] for n in range(160)))
                        await websocket.send(json.dumps({
                            'type': 'incoming_call_media_downlink', 'epoch': epoch, 'request_id': 1,
                            'sequence': sequence, 'source_time_us': sequence * 20000,
                            'frame': encoder.encode(pcm).hex()}))
                        sequence += 1
                        self.downlink += 1
                        if self.uplink_good + self.uplink_bad == self.hangup_after:
                            await websocket.send(json.dumps({'type': 'incoming_call_terminate',
                                                             'epoch': epoch, 'request_id': 1, 'cause': 16}))
        finally:
            decoder.close()
            encoder.close()

    def result(self):
        return dict(received=sorted(set(self.received)), phases=self.phases, uplink_good=self.uplink_good, uplink_bad=self.uplink_bad,
                    downlink=self.downlink, uplink_microphone_fraction=round(tone_fraction(bytes(self.uplink), MICROPHONE_HZ), 3))
