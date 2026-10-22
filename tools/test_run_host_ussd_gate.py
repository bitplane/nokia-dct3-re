import json
from types import SimpleNamespace
import unittest
from unittest.mock import AsyncMock,patch
from tools import run_host_ussd_gate as gate


def ready(epoch=1):return dict(type='call_adapter_ready',epoch=epoch)
def request(epoch=1,identity=1):
    return dict(type='outgoing_ussd',epoch=epoch,request_id=identity,dcs=15,data='aa986c3602')
def state(phase,epoch=1,identity=1):
    return dict(type='outgoing_ussd_state',epoch=epoch,request_id=identity,phase=phase)


class OutgoingUssdRunnerTest(unittest.IsolatedAsyncioTestCase):
    async def execute(self,events,*,restore=False,output=b'healthy\n'):
        process=SimpleNamespace(returncode=0,wait=AsyncMock(return_value=0),
            stdout=SimpleNamespace(read=AsyncMock(return_value=output)))
        socket=AsyncMock()
        socket.__aenter__.return_value=socket
        socket.recv.side_effect=[json.dumps(event) for event in events]
        args=SimpleNamespace(command=['mame'],cwd=None,port=18990,require_restore=restore)
        with patch.object(gate.asyncio,'create_subprocess_exec',AsyncMock(return_value=process)), \
             patch.object(gate,'connect',AsyncMock(return_value=socket)),patch('builtins.print'):
            await gate.run(args)
        self.assertEqual(socket.recv.await_count,len(events))
        sent=[json.loads(call.args[0]) for call in socket.send.await_args_list]
        self.assertEqual([event['request_id'] for event in sent],[1,2,1] if restore else [2,1])
        if restore:
            self.assertEqual([event['epoch'] for event in sent],[1,2,2])
        self.assertEqual(sent[-1]['data'],gate.pack_gsm7('Host network'))

    async def test_correlated_request_and_restored_republication(self):
        await self.execute([ready(),request(),state('accepted'),state('ended')])
        await self.execute([ready(),request(),ready(2),request(2),
                            state('accepted',2),state('ended',2)],restore=True)

    async def test_wrong_identity_or_order_cannot_succeed(self):
        for tail in ([state('accepted',identity=2),state('ended')],
                     [state('ended')],
                     [state('accepted'),state('accepted'),state('ended')],
                     [request(),state('accepted'),state('ended')]):
            with self.subTest(tail=tail),self.assertRaises(RuntimeError):
                await self.execute([ready(),request(),*tail])

    async def test_restore_requires_same_request_and_next_epoch(self):
        for second_ready,second_request in ((ready(3),request(3)),(ready(2),request(2,2))):
            with self.subTest(second_ready=second_ready),self.assertRaises(RuntimeError):
                await self.execute([ready(),request(),second_ready,second_request,
                                    state('accepted',2),state('ended',2)],restore=True)

    async def test_bad_console_is_failure(self):
        with self.assertRaises(RuntimeError):
            await self.execute([ready(),request(),state('accepted'),state('ended')],
                               output=b'[LUA ERROR] input failed\n')


if __name__=='__main__':unittest.main()
