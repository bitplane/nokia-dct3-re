import unittest
import asyncio
import argparse
import json
from types import SimpleNamespace
from unittest.mock import AsyncMock, patch
from websockets.exceptions import ConnectionClosedError

from tools import run_host_sms_gate as gate


class HostSmsRunnerTest(unittest.IsolatedAsyncioTestCase):
    async def test_completion_requires_current_identity_and_ended_phase(self):
        socket = AsyncMock()
        socket.recv.side_effect = [json.dumps(event) for event in (
            {'type': 'outgoing_sms_state', 'request_id': 1, 'epoch': 1, 'phase': 'ended'},
            {'type': 'outgoing_sms_state', 'request_id': 2, 'epoch': 2, 'phase': 'ended'},
            {'type': 'outgoing_sms_state', 'request_id': 1, 'epoch': 2, 'phase': 'accepted'},
            {'type': 'incoming_sms_state', 'request_id': 1, 'epoch': 2, 'phase': 'ended'},
            {'type': 'outgoing_sms_state', 'request_id': 1, 'epoch': 2, 'phase': 'ended'},
        )]
        await gate.wait_for_sms_end(socket, 2, 1)
        self.assertEqual(socket.recv.await_count, 5)

    async def test_unrelated_traffic_cannot_extend_total_budget(self):
        async def unrelated():
            await asyncio.sleep(0.002)
            return '{}'
        socket = AsyncMock()
        socket.recv.side_effect = unrelated
        with self.assertRaises(asyncio.TimeoutError):
            await gate.wait_for_sms_end(socket, 1, 0.02)
        self.assertGreater(socket.recv.await_count, 1)

    def test_completion_budget_is_finite_positive(self):
        self.assertEqual(gate.positive_seconds('180'), 180)
        for value in ('0', '-1', 'nan', 'inf'):
            with self.subTest(value=value), self.assertRaises(argparse.ArgumentTypeError):
                gate.positive_seconds(value)

    async def test_connection_close_is_a_lifecycle_failure(self):
        process = SimpleNamespace(returncode=0)
        socket = AsyncMock()
        socket.__aenter__.return_value = socket
        socket.recv.side_effect = ConnectionClosedError(None, None)
        args = SimpleNamespace(command=['mame'], cwd=None, port=18990)
        with patch.object(gate.asyncio, 'create_subprocess_exec',
                          AsyncMock(return_value=process)), \
                patch.object(gate, 'connect', AsyncMock(return_value=socket)):
            with self.assertRaisesRegex(RuntimeError, 'before lifecycle completion'):
                await gate.run(args)
