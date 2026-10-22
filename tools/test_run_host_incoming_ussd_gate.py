import asyncio
import json
from types import SimpleNamespace
import unittest
from unittest.mock import AsyncMock, patch

from tools import run_host_incoming_ussd_gate as gate


class IncomingUssdRunnerTest(unittest.IsolatedAsyncioTestCase):
    async def run_fixture(self, reference=('queued','delivered'), *, restore=True,
                          output=b'clean handset\n', wrong_request=False):
        process=SimpleNamespace(returncode=0,wait=AsyncMock(return_value=0),
            stdout=SimpleNamespace(readline=AsyncMock(side_effect=[output,b''])))
        socket=AsyncMock()
        socket.__aenter__.return_value=socket
        events=[dict(type='call_adapter_ready',epoch=1),
                dict(type='network_state',epoch=1,registered=True)]
        events += [dict(type='incoming_ussd_state',epoch=1,
                        request_id=2 if wrong_request else 1,phase=phase) for phase in reference]
        if restore:
            events += [dict(type='call_adapter_ready',epoch=2)]
            events += [dict(type='incoming_ussd_state',epoch=2,request_id=1,phase=phase)
                       for phase in ('queued','delivered')]
        socket.recv.side_effect=[json.dumps(event) for event in events]
        args=SimpleNamespace(command=['mame'],cwd=None,port=18990,
                             text='Host notice',require_restore=restore)
        with patch.object(gate.asyncio,'create_subprocess_exec',AsyncMock(return_value=process)), \
             patch.object(gate,'connect',AsyncMock(return_value=socket)),patch('builtins.print'):
            await gate.run(args)
        self.assertEqual(socket.send.await_count,1)
        self.assertEqual(socket.recv.await_count,len(events))

    async def test_reference_delivery_does_not_skip_restore(self):
        await self.run_fixture()

    async def test_partial_reference_and_plain_delivery(self):
        await self.run_fixture(('queued',))
        await self.run_fixture(restore=False)

    async def test_duplicate_reversed_and_wrong_request_fail(self):
        for phases in (('queued','queued','delivered'),('delivered',)):
            with self.subTest(phases=phases),self.assertRaises(RuntimeError):
                await self.run_fixture(phases)
        with self.assertRaises(RuntimeError):
            await self.run_fixture(wrong_request=True)

    async def test_lua_error_is_not_success(self):
        with self.assertRaises(RuntimeError):
            await self.run_fixture(output=b'[LUA ERROR] failed input\n')

    async def test_ready_marker_comes_from_drained_console(self):
        ready=asyncio.Event()
        stream=SimpleNamespace(readline=AsyncMock(side_effect=[b'boot\n',b'fixture ready\n',b'']))
        self.assertEqual(await gate.read_console(stream,ready,'fixture ready'),
                         'boot\nfixture ready\n')
        self.assertTrue(ready.is_set())


if __name__=='__main__':unittest.main()
