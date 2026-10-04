import unittest

from tools.noki6250_pmm_check import assess, checksum
from tools.test_nse5_pmm_journal import sector, write


class PmmTests(unittest.TestCase):
    def test_checksum_excludes_two_bytes_and_wraps(self):
        cache = bytearray([255] * 0xa28)
        self.assertEqual(checksum(cache), (306 * 255) & 0xffff)
        cache[0x154:0x156] = bytes(2)
        self.assertEqual(checksum(cache), (306 * 255) & 0xffff)

    def test_replay_distinguishes_valid_base_and_stale_update(self):
        cache = bytearray(0xa28)
        cache[0x120] = 3
        cache[0x254:0x256] = b"\x00\x03"
        image = sector(write(0, cache) + write(0x120, b"\x04", True))
        result = assess(image)
        self.assertEqual(result["base_computed"], result["base_stored"])
        self.assertEqual(result["replayed_computed"], "0004")
        self.assertEqual(result["replayed_stored"], "0003")

    def test_trace_must_match_replayed_shadow(self):
        image = sector(write(0, bytes(0xa28)))
        for trace in ("", "6250_nv_sum_shadow: bytes=00"):
            with self.assertRaises(ValueError):
                assess(image, trace)
        assess(image, "6250_nv_sum_shadow: bytes=" + "00" * 310)

    def test_rejects_incomplete_base(self):
        with self.assertRaises(ValueError):
            assess(sector(write(0, bytes(0x256))))
        with self.assertRaises(ValueError):
            checksum(bytes(0x255))


if __name__ == "__main__":
    unittest.main()
