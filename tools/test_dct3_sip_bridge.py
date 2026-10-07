import queue
import unittest

from tools.dct3_sip_bridge import MediaQueues, incoming_caller


class SipMediaQueuesTest(unittest.TestCase):
    def test_numeric_sip_identity_is_required_for_incoming_cli(self):
        self.assertEqual(incoming_caller('sip:5551234@127.0.0.1:25100'), '5551234')
        self.assertEqual(incoming_caller('"Caller" <sip:123@localhost>'), '123')
        self.assertEqual(incoming_caller('"<sip:999@fake>" <sip:123@localhost>'), '123')
        for uri in ('sip:alice@localhost', 'sip:12x@localhost',
                    'sip:' + '1' * 21 + '@localhost', 'tel:123',
                    '"sip:123@fake" <sip:alice@localhost>'):
            self.assertIsNone(incoming_caller(uri))

    def test_bounded_media_queue_does_not_block(self):
        media = MediaQueues()
        for index in range(12):
            media.put(media.uplink, bytes([index]) * 320)
        self.assertEqual(media.dropped, 4)
        self.assertEqual(media.uplink.qsize(), 8)
        self.assertEqual([media.uplink.get_nowait()[0] for _ in range(8)], list(range(8)))
        with self.assertRaises(queue.Empty):
            media.uplink.get_nowait()

    def test_wrong_pcm_geometry_is_not_queued(self):
        media = MediaQueues()
        for data in (b'', bytes(318), bytes(640)):
            media.put(media.downlink, data)
        self.assertTrue(media.downlink.empty())
        media.put(media.downlink, bytes(320))
        self.assertEqual(media.downlink.qsize(), 1)


if __name__ == '__main__':
    unittest.main()
