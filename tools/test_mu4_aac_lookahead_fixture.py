import unittest

from mu4_aac_lookahead_fixture import append_lookahead


def packet(length, fill=0):
    header = bytearray.fromhex('fff95080001ffc')
    header[3] |= length >> 11
    header[4] = (length >> 3) & 255
    header[5] |= (length & 7) << 5
    return bytes(header)+bytes([fill])*(length-7)


class LookaheadFixtureTest(unittest.TestCase):
    def test_prefix_is_unchanged_and_suffix_has_explicit_source(self):
        first, second, last = packet(36,1),packet(40,2),packet(14,3)
        source = first+second+last
        extended,report = append_lookahead(source)
        self.assertEqual(extended,source+second)
        self.assertEqual(report['appended_source_offset'],36)
        self.assertEqual(report['appended_bytes'],40)
        self.assertEqual(report['original_packets'],3)
        self.assertIn('not original file EOF',report['scope'])

    def test_incomplete_chain_or_insufficient_reserve_reject(self):
        for source in (b'',packet(40),packet(40)[:-1],packet(40)+b'broken',
                       packet(14)+packet(40)):
            with self.subTest(source=source[:10]), self.assertRaises(ValueError):
                append_lookahead(source)


if __name__ == '__main__':
    unittest.main()
