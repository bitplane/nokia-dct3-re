import unittest

from tools.dct3_call_bridge import LoopbackProtocol


class Dct3CallBridgeTest(unittest.TestCase):
    def setUp(self):
        self.protocol = LoopbackProtocol()
        replies = self.protocol.handle({
            "type": "outgoing_call", "epoch": 3, "request_id": 7,
            "digits": "5551234",
        })
        self.assertEqual(replies, [{
            "type": "outgoing_call_decision", "epoch": 3,
            "request_id": 7, "decision": "connect",
        }])

    def connect(self, downlink_sequence=0):
        self.protocol.handle({
            "type": "outgoing_call_state", "epoch": 3, "request_id": 7,
            "phase": "connected",
            "media_uplink_sequence": 0,
            "media_downlink_sequence": downlink_sequence,
        })

    def test_valid_media_is_echoed_with_endpoint_sequence(self):
        self.connect(12)
        replies = self.protocol.handle({
            "type": "outgoing_call_media_uplink", "epoch": 3,
            "request_id": 7, "sequence": 9, "emulation_time_us": 20000,
            "good": True, "frame": "ab" * 33,
        })
        self.assertEqual(replies, [{
            "type": "outgoing_call_media_downlink", "epoch": 3,
            "request_id": 7, "sequence": 12, "source_time_us": 20000,
            "frame": "ab" * 33,
        }])

    def test_bad_frame_advances_uplink_but_not_downlink(self):
        self.connect()
        message = {
            "type": "outgoing_call_media_uplink", "epoch": 3,
            "request_id": 7, "sequence": 0, "emulation_time_us": 20000,
            "good": False, "frame": "00" * 33,
        }
        self.assertEqual(self.protocol.handle(message), [])
        message.update(sequence=1, emulation_time_us=40000, good=True)
        self.assertEqual(self.protocol.handle(message)[0]["sequence"], 0)
        self.assertEqual(self.protocol.stats.bad_frames, 1)

    def test_stale_epoch_and_malformed_frame_are_ignored(self):
        self.connect()
        base = {
            "type": "outgoing_call_media_uplink", "epoch": 2,
            "request_id": 7, "sequence": 0, "emulation_time_us": 0,
            "good": True, "frame": "00" * 33,
        }
        self.assertEqual(self.protocol.handle(base), [])
        base.update(epoch=3, frame="xx" * 33)
        self.assertEqual(self.protocol.handle(base), [])
        self.assertEqual(self.protocol.stats.uplink_frames, 0)

    def test_remote_and_handset_termination(self):
        self.connect()
        self.assertEqual(self.protocol.termination(), {
            "type": "outgoing_call_terminate", "epoch": 3,
            "request_id": 7, "cause": 16,
        })
        self.protocol.handle({
            "type": "outgoing_call_state", "epoch": 3, "request_id": 7,
            "phase": "ended",
        })
        self.assertEqual(self.protocol.phase, "ended")
        self.assertIsNone(self.protocol.termination())

    def test_restore_epoch_replaces_sequence_space(self):
        self.connect()
        self.protocol.handle({
            "type": "outgoing_call", "epoch": 4, "request_id": 7,
            "digits": "5551234",
        })
        self.protocol.handle({
            "type": "outgoing_call_state", "epoch": 4, "request_id": 7,
            "phase": "connected", "media_downlink_sequence": 41,
        })
        self.assertEqual(self.protocol.next_downlink_sequence, 41)
        self.assertEqual(self.protocol.stats.calls, 1)


if __name__ == "__main__":
    unittest.main()
