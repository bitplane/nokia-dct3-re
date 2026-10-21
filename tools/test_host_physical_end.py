"""A signaling-only host observer requires connected state before release."""
import json
import unittest
from unittest.mock import AsyncMock
from tools.run_host_call_adapter_gate import observe_physical_end


class PhysicalEndTest(unittest.IsolatedAsyncioTestCase):
    def socket(self, events):
        socket=AsyncMock()
        socket.recv.side_effect=[json.dumps(event) for event in events]
        return socket

    def state(self, phase, epoch=1, request=1):
        return dict(type='outgoing_call_state',epoch=epoch,request_id=request,phase=phase)

    async def test_connection_then_local_release(self):
        socket=self.socket([dict(type='network_state'),self.state('connected'),
                            self.state('media_closed'),self.state('ended')])
        await observe_physical_end(socket,1,1)
        self.assertEqual(socket.recv.await_count,4)
        socket.send.assert_not_called()

    async def test_release_without_connection_rejected(self):
        with self.assertRaises(RuntimeError):
            await observe_physical_end(self.socket([self.state('ended')]),1,1)

    async def test_epoch_and_request_identity(self):
        for event in (self.state('connected',epoch=2),self.state('connected',request=2)):
            with self.subTest(event=event),self.assertRaises(RuntimeError):
                await observe_physical_end(self.socket([event]),1,1)


if __name__=='__main__':
    unittest.main()
