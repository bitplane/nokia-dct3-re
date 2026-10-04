import unittest

from tools.noki6250_pmm_check import assess, checksum, record_checksum, initial_record_fixture
from tools.test_nse5_pmm_journal import sector, write
from tools.noki6250_staged_check import check_initial_fixture


class PmmTests(unittest.TestCase):
    def test_sector_inventory_does_not_invent_a_backup_journal(self):
        image = bytes(sector(write(0, bytes(0xa28)))) + b"\xff" * 0x2000
        rows = assess(image)["sector_inventory"]
        self.assertEqual(len(rows), 2)
        self.assertTrue(rows[0]["eeprom_signature"])
        self.assertEqual(rows[0]["reader_state_word"], "0001")
        self.assertTrue(rows[1]["erased"])
        self.assertFalse(rows[1]["eeprom_signature"])
        self.assertEqual(rows[1]["reader_state_word"], "ffff")

    def test_initial_runtime_requires_clear_fault_and_long_endpoint(self):
        text = ("runtime_hle_handoff pc=2c75 native_suspended=1\n"
                "6250_service_control_consumer: class=74 command=0d status=00 armed=c4\n"
                "6250_startup_fault_endpoint: bytes=" + "00" * 24 + "\n"
                "6250_startup_fault_endpoint: bytes=" + "00" * 24 + "\n"
                "6250_runtime_boundary: dsp_pc=2c75 t=20.000000\n")
        check_initial_fixture(text)
        for broken in (text + "6250_nv_sum_failure:", text.replace("20.000000", "8.000000"),
                       text.replace("bytes=" + "00" * 24, "bytes=" + "00" * 12 + "0c" + "00" * 11)):
            with self.assertRaises(ValueError):
                check_initial_fixture(broken)

    def test_initial_fixture_preserves_payload_and_other_sectors(self):
        cache = bytearray(0xa28)
        cache[0x120] = 3
        cache[0x254:0x256] = b"\x00\x03"
        image = bytes(sector(write(0, cache) + write(0x120, b"\x04", True))) + b"outside"
        fixture = initial_record_fixture(image)
        self.assertEqual(fixture[0x26:0xa4e], cache)
        self.assertEqual(fixture[0x2000:], b"outside")
        result = assess(fixture)
        self.assertEqual(result["records"], 1)
        self.assertEqual(result["record_checksum_mismatches"], [])
        self.assertEqual(result["replayed_computed"], result["replayed_stored"])

    def test_initial_fixture_refuses_bad_application_checksum(self):
        cache = bytearray(0xa28)
        cache[0x120] = 1
        with self.assertRaisesRegex(ValueError, "application checksum"):
            initial_record_fixture(sector(write(0, cache)))

    def test_record_checksum_uses_both_address_bytes_and_wraps(self):
        self.assertEqual(record_checksum(0x150, bytes.fromhex("a761818e")), 0x68)
        self.assertEqual(record_checksum(0x254, bytes.fromhex("7095")), 0x5b)
        with self.assertRaises(ValueError):
            record_checksum(0x10000, b"")

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
