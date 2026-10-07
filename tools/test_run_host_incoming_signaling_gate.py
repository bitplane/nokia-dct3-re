import json
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace
import unittest
from unittest.mock import AsyncMock, patch

from tools import run_host_incoming_signaling_gate as gate


class Socket:
    def __init__(self, messages):
        self.messages = iter(messages)
        self.send = AsyncMock()

    async def __aenter__(self):
        return self

    async def __aexit__(self, *args):
        return False

    async def recv(self):
        return json.dumps(next(self.messages))


class IncomingRunnerTest(unittest.IsolatedAsyncioTestCase):
    async def exercise(self, *, phases=None, epoch=7, request=1, exit_code=0):
        with TemporaryDirectory() as directory:
            ready = Path(directory) / 'ready.png'
            args = SimpleNamespace(ready_file=ready, command=['mame'], cwd=directory,
                                   port=18990, caller='447700900123')
            process = SimpleNamespace(returncode=None, wait=AsyncMock(return_value=exit_code),
                                      terminate=lambda: None, kill=lambda: None)
            messages = [{'type': 'call_adapter_ready', 'epoch': 7}]
            messages.extend({'type': 'incoming_call_state', 'epoch': epoch,
                             'request_id': request, 'phase': phase}
                            for phase in (phases or ['queued', 'paging', 'alerting',
                                                    'connected', 'ended']))
            socket = Socket(messages)

            async def connect(*_):
                ready.touch()
                return socket

            with patch.object(gate.asyncio, 'create_subprocess_exec',
                              AsyncMock(return_value=process)), \
                    patch.object(gate, 'connect', connect):
                await gate.run(args)
            return json.loads(socket.send.call_args.args[0])

    async def test_complete_correlated_lifecycle(self):
        self.assertEqual(await self.exercise(), {'type': 'incoming_call', 'epoch': 7,
                         'request_id': 1, 'caller': '447700900123'})

    async def test_duplicate_phases_are_not_extra_transitions(self):
        await self.exercise(phases=['queued', 'queued', 'paging', 'alerting',
                                    'connected', 'ended'])

    async def test_missing_connected_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'incomplete incoming lifecycle'):
            await self.exercise(phases=['queued', 'paging', 'alerting', 'ended'])

    async def test_wrong_epoch_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'identity/epoch mismatch'):
            await self.exercise(epoch=8)

    async def test_wrong_request_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'identity/epoch mismatch'):
            await self.exercise(request=2)

    async def test_child_failure_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'exited unsuccessfully'):
            await self.exercise(exit_code=1)

    async def test_stale_readiness_never_launches_child(self):
        with TemporaryDirectory() as directory:
            ready = Path(directory) / 'ready.png'
            ready.touch()
            with patch.object(gate.asyncio, 'create_subprocess_exec', AsyncMock()) as launch:
                with self.assertRaisesRegex(RuntimeError, 'already exists'):
                    await gate.run(SimpleNamespace(ready_file=ready))
                launch.assert_not_called()
