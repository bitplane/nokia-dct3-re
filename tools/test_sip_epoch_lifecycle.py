import asyncio
from collections import deque
import json
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import websockets

from tools import dct3_sip_bridge as sip


class Endpoint:
    instances = []

    def __init__(self, *_):
        self.events = deque()
        self.media = sip.MediaQueues()
        self.dials = []
        self.hangups = 0
        self.instances.append(self)

    def poll(self):
        pass

    def dial(self, destination, identity):
        self.dials.append(identity)
        self.events.append((identity, 'confirmed', 200))

    def hangup(self):
        self.hangups += 1

    def close(self):
        pass


class Codec:
    def close(self):
        pass


class SipEpochLifecycleTest(unittest.IsolatedAsyncioTestCase):
    async def test_explicit_idle_snapshot_allows_fresh_call(self):
        async def host(socket):
            async def send(kind, **fields):
                await socket.send(json.dumps({'type': kind, **fields}))

            await send('call_adapter_ready', protocol_version=1, epoch=1)
            await send('call_adapter_ready', protocol_version=1, epoch=2, calls_idle=True)
            await send('outgoing_call', epoch=2, request_id=2, digits='456')
            decision = json.loads(await asyncio.wait_for(socket.recv(), 2))
            self.assertEqual(decision, {'type': 'outgoing_call_decision',
                'epoch': 2, 'request_id': 2, 'decision': 'connect'})
            await send('outgoing_call_state', epoch=2, request_id=2, phase='ended')
            await socket.wait_closed()

        with patch.object(sip, 'SipEndpoint', Endpoint), patch.object(sip, 'GsmFrCodec', Codec):
            async with websockets.serve(host, '127.0.0.1', 0) as server:
                port = server.sockets[0].getsockname()[1]
                args = SimpleNamespace(url=f'ws://127.0.0.1:{port}', sip_port=0,
                    destination='sip:probe@localhost', once=True, require_frames=0)
                await asyncio.wait_for(sip.bridge(args, None), 3)
        self.assertEqual(Endpoint.instances[-1].dials, [(2, 2)])

    async def test_unrelated_end_cannot_release_restoration_guard(self):
        async def host(socket):
            async def send(kind, **fields):
                await socket.send(json.dumps({'type': kind, **fields}))

            await send('call_adapter_ready', protocol_version=1, epoch=1)
            await send('outgoing_call', epoch=1, request_id=1, digits='123')
            decision = json.loads(await asyncio.wait_for(socket.recv(), 2))
            self.assertEqual(decision['decision'], 'connect')
            await send('outgoing_call_state', epoch=1, request_id=1,
                       phase='connected', media_downlink_sequence=0)
            await send('call_adapter_ready', protocol_version=1, epoch=2)
            await send('outgoing_call_state', epoch=2, request_id=1,
                       phase='connected', media_downlink_sequence=0)
            termination = json.loads(await asyncio.wait_for(socket.recv(), 2))
            self.assertEqual(termination, {'type': 'outgoing_call_terminate',
                'epoch': 2, 'request_id': 1, 'cause': 41})
            await send('outgoing_call_state', epoch=2, request_id=999, phase='ended')
            await send('outgoing_call', epoch=2, request_id=2, digits='456')
            await asyncio.sleep(0.04)
            self.assertEqual(Endpoint.instances[-1].dials, [(1, 1)])
            await send('outgoing_call_state', epoch=2, request_id=1, phase='ended')
            await socket.wait_closed()

        with patch.object(sip, 'SipEndpoint', Endpoint), patch.object(sip, 'GsmFrCodec', Codec):
            async with websockets.serve(host, '127.0.0.1', 0) as server:
                port = server.sockets[0].getsockname()[1]
                args = SimpleNamespace(url=f'ws://127.0.0.1:{port}', sip_port=0,
                    destination='sip:probe@localhost', once=True, require_frames=0)
                await asyncio.wait_for(sip.bridge(args, None), 3)
        self.assertGreaterEqual(Endpoint.instances[-1].hangups, 1)


if __name__ == '__main__':
    unittest.main()
