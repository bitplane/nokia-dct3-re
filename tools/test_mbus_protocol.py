import unittest
from tools.mbus_protocol import Frame, decode


class MbusProtocolTest(unittest.TestCase):
    def test_known_startup_query_and_ack_vectors(self):
        for vector in ('1fff00d00001010131', '1f0010d10005000100030001d8', '1f10007f0171'):
            data = bytes.fromhex(vector)
            self.assertEqual(decode(data).encode(), data)

    def test_payload_marker_is_not_a_delimiter(self):
        frame = Frame(0x10, 0, 0xd2, 2, b'\x1f\x00\x1f')
        self.assertEqual(decode(frame.encode()), frame)

    def test_bad_header_checksum_extent_and_ack_payload_fail(self):
        good = Frame(0x10, 0, 0xd2, 2, b'abc').encode()
        for bad in (b'', good[:-1], good + b'\x00', b'\x00' + good[1:], good[:-1] + bytes((good[-1] ^ 1,))):
            with self.assertRaises(ValueError):
                decode(bad)
        for frame in (Frame(256, 0, 0, 0), Frame(0, 0, 0x7f, 1, b'abc')):
            with self.assertRaises(ValueError):
                frame.encode()
