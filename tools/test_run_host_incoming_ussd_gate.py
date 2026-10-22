import asyncio
import json
import io
from types import SimpleNamespace
import unittest
from unittest.mock import AsyncMock, patch

from tools import run_host_incoming_ussd_gate as gate


class IncomingUssdRunnerTest(unittest.IsolatedAsyncioTestCase):
    async def run_fixture(self, reference=('queued','delivered'), *, restore=True,
                          output=b'clean handset\n', wrong_request=False,
                          request=False, reply='32', include_reply=True, empty_reply=False):
        process=SimpleNamespace(returncode=0,wait=AsyncMock(return_value=0),
            stdout=SimpleNamespace(readline=AsyncMock(side_effect=[output,b''])))
        socket=AsyncMock()
        socket.__aenter__.return_value=socket
        events=[dict(type='call_adapter_ready',epoch=1),
                dict(type='network_state',epoch=1,registered=True)]
        for phase in reference:
            if request and phase=='answered' and include_reply:
                events.append(dict(type='incoming_ussd_response',epoch=1,request_id=1,
                                   dcs=0 if empty_reply else 15,data=reply,empty=empty_reply))
            events.append(dict(type='incoming_ussd_state',epoch=1,
                               request_id=2 if wrong_request else 1,phase=phase))
        if restore:
            events += [dict(type='call_adapter_ready',epoch=2)]
            events.append(dict(type='incoming_ussd_state',epoch=2,request_id=1,phase='queued'))
            if request:
                events.append(dict(type='incoming_ussd_response',epoch=2,request_id=1,dcs=15,data=reply,empty=False))
            events.append(dict(type='incoming_ussd_state',epoch=2,request_id=1,
                               phase='answered' if request else 'delivered'))
        socket.recv.side_effect=[json.dumps(event) for event in events]
        args=SimpleNamespace(command=['mame'],cwd=None,port=18990,
                             text='Host notice',require_restore=restore,request=request,expected_reply='2',empty_reply=empty_reply)
        with patch.object(gate.asyncio,'create_subprocess_exec',AsyncMock(return_value=process)), \
             patch.object(gate,'connect',AsyncMock(return_value=socket)),patch('builtins.print'):
            await gate.run(args)
        self.assertEqual(socket.send.await_count,1)
        self.assertEqual(socket.recv.await_count,len(events))
        self.assertEqual(json.loads(socket.send.call_args.args[0]).get('operation'),
                         'request' if request else None)

    async def test_request_requires_actual_correlated_reply(self):
        await self.run_fixture(('queued','answered'),restore=False,request=True)
        await self.run_fixture(('queued','answered'),restore=True,request=True)
        for kwargs in ({'reply':'33'}, {'include_reply':False}, {'wrong_request':True}):
            with self.subTest(kwargs=kwargs),self.assertRaises(RuntimeError):
                await self.run_fixture(('queued','answered'),restore=False,request=True,**kwargs)

    async def test_empty_result_is_not_a_typed_reply(self):
        await self.run_fixture(('queued','answered'),restore=False,request=True,empty_reply=True,reply='')
        with self.assertRaises(RuntimeError):
            await self.run_fixture(('queued','answered'),restore=False,request=True,empty_reply=True,reply='32')

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


class IncomingUssdCliTest(unittest.TestCase):
    def test_connection_close_fails_cleanly_without_a_traceback(self):
        def closed(coroutine):
            coroutine.close()
            raise gate.ConnectionClosed(None,None)
        with patch('sys.argv',['runner','--port','18990','--','mame']), \
                patch.object(gate.asyncio,'run',side_effect=closed), \
                patch('sys.stdout',new_callable=io.StringIO) as output:
            self.assertEqual(gate.main(),1)
        self.assertIn('FAIL - no close frame received or sent',output.getvalue())
        self.assertNotIn('OK -',output.getvalue())


if __name__=='__main__':unittest.main()
