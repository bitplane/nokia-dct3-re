import json
from types import SimpleNamespace
import unittest
from unittest.mock import AsyncMock, patch

from tools import run_host_incoming_call_gate as gate


class Socket:
    def __init__(self, events):
        self.events = iter(events)
        self.sent = []
        self.delivered = []

    async def __aenter__(self):
        return self

    async def __aexit__(self, *args):
        return False

    async def recv(self):
        event = next(self.events)
        self.delivered.append(event)
        return json.dumps(event)

    async def send(self, data):
        event = json.loads(data)
        self.sent.append((event, list(self.delivered)))


def state(epoch, phase):
    return dict(type='incoming_call_state', epoch=epoch, request_id=1,
                phase=phase, media_downlink_sequence=0)


def frame(epoch, index):
    return dict(type='incoming_call_media_uplink', epoch=epoch, request_id=1,
                good=True, frame='00' * 33, emulation_time_us=index * 20000)


class IncomingMediaQuotaTest(unittest.IsolatedAsyncioTestCase):
    async def exercise(self, require_restore, *, physical_end=False):
        events = [dict(type='call_adapter_ready', protocol_version=1, epoch=1)]
        events += [state(1, phase) for phase in ('queued', 'paging', 'alerting', 'connected')]
        events += [frame(1, index) for index in range(5)]
        if require_restore:
            events += [dict(type='call_adapter_ready', protocol_version=1, epoch=2),
                       state(2, 'connected'), frame(1, 6), frame(2, 0), frame(2, 1)]
        events += [state(2 if require_restore else 1, 'ended')]
        socket = Socket(events)
        process = SimpleNamespace(returncode=0, wait=AsyncMock(return_value=0))
        args = SimpleNamespace(command=['mame'], cwd=None, port=1, caller='5551234',
                               frames=2, require_restore=require_restore,
                               ready_file=None, physical_end=physical_end, restored_state_file=None)
        with patch.object(gate.asyncio, 'create_subprocess_exec', AsyncMock(return_value=process)), \
                patch.object(gate, 'connect', AsyncMock(return_value=socket)):
            await gate.run(args)
        terminations = [(message, delivered) for message, delivered in socket.sent
                        if message['type'] == 'incoming_call_terminate']
        if physical_end:
            self.assertEqual(len(terminations), 0)
            stale = [message for message, _ in socket.sent
                     if message['type'] == 'incoming_call_media_downlink' and message['epoch'] == 1
                     and message.get('source_time_us') == 0]
            self.assertGreaterEqual(len(stale), 1)
            return None, socket.sent
        self.assertEqual(len(terminations), 1)
        return terminations[0], socket.sent

    async def test_pre_restore_frames_cannot_satisfy_restored_quota(self):
        (termination, delivered), sent = await self.exercise(True)
        self.assertEqual(termination['epoch'], 2)
        restored = [event for event in delivered
                    if event['type'] == 'incoming_call_media_uplink' and event['epoch'] == 2]
        self.assertEqual(len(restored), 2)
        incoming = [message for message, _ in sent if message['type'] == 'incoming_call']
        self.assertEqual(len(incoming), 1)

    async def test_non_restore_quota_retains_existing_behavior(self):
        (termination, delivered), _ = await self.exercise(False)
        self.assertEqual(termination['epoch'], 1)
        media = [event for event in delivered if event['type'] == 'incoming_call_media_uplink']
        self.assertEqual(len(media), 2)

    async def test_physical_end_does_not_send_remote_termination(self):
        await self.exercise(True, physical_end=True)
