import unittest
import json
from types import SimpleNamespace
from unittest.mock import AsyncMock, patch

from websockets.exceptions import ConnectionClosedError

from tools import run_host_incoming_sms_gate as gate


class HostIncomingSmsRunnerTest(unittest.IsolatedAsyncioTestCase):
    async def test_restore_before_reference_delivery_is_supported(self):
        process=SimpleNamespace(returncode=0,wait=AsyncMock(return_value=0),
            stdout=SimpleNamespace(read=AsyncMock(return_value=b'clean handset\n')))
        socket=AsyncMock()
        socket.__aenter__.return_value=socket
        events=[dict(type='call_adapter_ready',epoch=1),
                dict(type='network_state',epoch=1,registered=True),
                dict(type='incoming_sms_state',epoch=1,request_id=1,phase='queued'),
                dict(type='call_adapter_ready',epoch=2)]
        events += [dict(type='incoming_sms_state',epoch=2,request_id=1,phase=phase)
                   for phase in ('queued','delivered')]
        socket.recv.side_effect=[json.dumps(event) for event in events]
        args=SimpleNamespace(command=['mame'],cwd=None,port=18990,require_restore=True)
        with patch.object(gate.asyncio,'create_subprocess_exec',AsyncMock(return_value=process)), \
             patch.object(gate,'connect',AsyncMock(return_value=socket)),patch('builtins.print'):
            await gate.run(args)
        self.assertEqual(socket.send.await_count,1)
        self.assertEqual(socket.recv.await_count,len(events))

    async def test_invalid_reference_lifecycle_cannot_be_hidden_by_restore(self):
        cases = (
            ([('queued',1),('queued',1),('delivered',1)],2),
            ([('delivered',1)],2),
            ([('queued',2),('delivered',1)],2),
            ([('queued',1),('delivered',1)],0),
        )
        for reference,epoch in cases:
            with self.subTest(reference=reference,epoch=epoch):
                process=SimpleNamespace(returncode=0,wait=AsyncMock(return_value=0),
                    stdout=SimpleNamespace(read=AsyncMock(return_value=b'clean handset\n')))
                socket=AsyncMock()
                socket.__aenter__.return_value=socket
                events=[dict(type='call_adapter_ready',epoch=1),
                        dict(type='network_state',epoch=1,registered=True)]
                events += [dict(type='incoming_sms_state',epoch=1,request_id=request,phase=phase)
                           for phase,request in reference]
                events += [dict(type='call_adapter_ready',epoch=epoch)]
                events += [dict(type='incoming_sms_state',epoch=epoch,request_id=1,phase=phase)
                           for phase in ('queued','delivered')]
                socket.recv.side_effect=[json.dumps(event) for event in events]
                args=SimpleNamespace(command=['mame'],cwd=None,port=18990,require_restore=True)
                with patch.object(gate.asyncio,'create_subprocess_exec',AsyncMock(return_value=process)), \
                     patch.object(gate,'connect',AsyncMock(return_value=socket)):
                    with self.assertRaises(RuntimeError):
                        await gate.run(args)
                self.assertEqual(socket.send.await_count,1)

    async def test_reference_delivery_waits_for_restored_epoch_and_console_health(self):
        for output in (b'clean handset\n',b'[LUA ERROR] cancelled capture\n'):
            process=SimpleNamespace(returncode=0,wait=AsyncMock(return_value=0),
                stdout=SimpleNamespace(read=AsyncMock(return_value=output)))
            socket=AsyncMock()
            socket.__aenter__.return_value=socket
            events=[dict(type='call_adapter_ready',epoch=1),dict(type='network_state',epoch=1,registered=True)]
            events += [dict(type='incoming_sms_state',epoch=1,request_id=1,phase=phase) for phase in ('queued','delivered')]
            events += [dict(type='call_adapter_ready',epoch=2)]
            events += [dict(type='incoming_sms_state',epoch=2,request_id=1,phase=phase) for phase in ('queued','delivered')]
            socket.recv.side_effect=[json.dumps(event) for event in events]
            args=SimpleNamespace(command=['mame'],cwd=None,port=18990,require_restore=True)
            with patch.object(gate.asyncio,'create_subprocess_exec',AsyncMock(return_value=process)), \
                 patch.object(gate,'connect',AsyncMock(return_value=socket)),patch('builtins.print'):
                if b'LUA ERROR' in output:
                    with self.assertRaises(RuntimeError):await gate.run(args)
                else:await gate.run(args)
            self.assertEqual(socket.send.await_count,1)
            self.assertEqual(socket.recv.await_count,len(events))

    async def test_connection_close_is_a_lifecycle_failure(self):
        process = SimpleNamespace(returncode=0,stdout=SimpleNamespace(read=AsyncMock(return_value=b'')))
        socket = AsyncMock()
        socket.__aenter__.return_value = socket
        socket.recv.side_effect = ConnectionClosedError(None, None)
        args = SimpleNamespace(command=['mame'], cwd=None, port=18990)
        with patch.object(gate.asyncio, 'create_subprocess_exec',
                          AsyncMock(return_value=process)), \
                patch.object(gate, 'connect', AsyncMock(return_value=socket)):
            with self.assertRaisesRegex(RuntimeError, 'before lifecycle completion'):
                await gate.run(args)
