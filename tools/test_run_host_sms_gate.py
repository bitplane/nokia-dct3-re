import unittest
from types import SimpleNamespace
from unittest.mock import AsyncMock, patch
from websockets.exceptions import ConnectionClosedError

from tools import run_host_sms_gate as gate


class HostSmsRunnerTest(unittest.IsolatedAsyncioTestCase):
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
