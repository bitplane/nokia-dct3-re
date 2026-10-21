from pathlib import Path
import unittest
from tools.nsb7_storage_check import checksum, checksum_fixture, security_retry_fixture, inspect, BASE, CACHE, SECTOR


class StorageTests(unittest.TestCase):
    def test_rotated_sector_and_fixture_ownership(self):
        flash = self.own_initial_flash()
        original = bytes(flash[BASE:BASE+SECTOR])
        flash[BASE:BASE+SECTOR] = b'\xff'*SECTOR
        flash[BASE+SECTOR:BASE+2*SECTOR] = original
        shadow, _, report = inspect(flash)
        self.assertEqual(report['sector_base'], hex(BASE+SECTOR))
        result, report = checksum_fixture(flash, shadow)
        self.assertEqual(result[BASE:BASE+SECTOR], flash[BASE:BASE+SECTOR])
        self.assertGreaterEqual(int(report['fixture']['physical_append'], 16), BASE+SECTOR)
        self.assertTrue(inspect(result)[2]['checksum_valid'])

    def test_dual_live_sectors_are_not_guessed(self):
        flash = self.own_initial_flash()
        flash[BASE+SECTOR:BASE+2*SECTOR] = flash[BASE:BASE+SECTOR]
        with self.assertRaises(ValueError):
            inspect(flash)

    def test_checksum_excludes_the_full_word(self):
        cache = bytearray(b'\xff' * CACHE)
        self.assertEqual(checksum(cache), 0xbd42)
        cache[0x154:0x156] = b'\x00\x12'
        self.assertEqual(checksum(cache), 0xbd42)
        cache[0x120] = 0
        self.assertEqual(checksum(cache), 0xbc43)

    def own_initial_flash(self):
        source = Path(__file__).resolve().parents[1] / 'roms/noki8290/8290f522a.fls'
        if not source.exists():
            self.skipTest('own NSB-7 image unavailable')
        flash = bytearray(source.read_bytes() + b'\xff' * 0x30000)
        flash[BASE+6:BASE+12] = b'EEPROM'
        flash[BASE+0x18:BASE+0x1a] = b'\x00\xc8'
        flash[BASE+0x20:BASE+0x26] = bytes.fromhex('000080000000')
        return flash

    def test_checksum_fixture_changes_only_checksum_logically(self):
        flash = self.own_initial_flash()
        shadow = b'\xff' * CACHE
        result, report = checksum_fixture(flash, shadow)
        cache, _, _ = inspect(result)
        expected = bytearray(shadow)
        expected[0x1e0:0x1e2] = bytes.fromhex('bd42')
        self.assertEqual(cache, expected)
        self.assertEqual(result[:BASE], flash[:BASE])
        changed = [i for i, (a, b) in enumerate(zip(result, flash)) if a != b]
        self.assertEqual(changed, list(range(BASE+0x8026, BASE+0x802c)))
        self.assertEqual(report['fixture']['record'], '08e001e0bd42')
        with self.assertRaises(ValueError):
            checksum_fixture(result, cache)

    def test_requires_exact_runtime_shadow_and_record_integrity(self):
        flash = self.own_initial_flash()
        with self.assertRaises(ValueError):
            checksum_fixture(flash, None)
        with self.assertRaises(ValueError):
            inspect(flash, bytes(CACHE))
        flash[BASE+0x21] = 1
        with self.assertRaises(ValueError):
            inspect(flash)

    def test_prefix_fixture_preserves_calibration_and_identity_contents(self):
        initial = self.own_initial_flash()
        calibrated, _ = checksum_fixture(initial, b'\xff'*CACHE)
        shadow, _, _ = inspect(calibrated)
        result, report = checksum_fixture(calibrated, shadow, prefix=True)
        after, _, status = inspect(result)
        expected = bytearray(shadow)
        expected[0x11c:0x120] = bytes.fromhex('00001ae4')
        self.assertEqual(after, expected)
        self.assertEqual(after[:0x11c], shadow[:0x11c])
        self.assertTrue(status['checksum_valid'])
        self.assertTrue(status['prefix_valid'])
        self.assertEqual(report['fixture']['logical_offset'], '0x11c')
        self.assertEqual(report['fixture']['record'], '101b011c00001ae4')

    def test_rejects_wrong_executable_and_extent(self):
        for flash in (b'', bytes(0x200000)):
            with self.assertRaises(ValueError):
                inspect(flash)

    def test_retry_fixture_preserves_code_identity_and_all_other_fields(self):
        flash = self.own_initial_flash()
        result, report = security_retry_fixture(flash, b'\xff'*CACHE)
        cache, _, _ = inspect(result)
        expected = bytearray(b'\xff'*CACHE)
        expected[0x9d8] = 0
        self.assertEqual(cache, expected)
        self.assertEqual(result[:BASE], flash[:BASE])
        self.assertEqual(report['fixture']['record'], '04e109d800')
        for source, shadow in ((result, cache), (flash, None), (flash, bytes(CACHE))):
            with self.assertRaises(ValueError):
                security_retry_fixture(source, shadow)

    def test_runtime_branch_must_match_replayed_inputs(self):
        flash = self.own_initial_flash()
        trace = ('nsb7_calibration_compare: computed=bd42 stored=ffff marker=ffff\n'
                 'nsb7_fault12: pc=00248948 value=0c mask=ff000000')
        inspect(flash, b'\xff'*CACHE, trace)
        for old, new in [('stored=ffff', 'stored=bd42'), ('value=0c', 'value=00'),
                         ('pc=00248948', 'pc=00248942')]:
            with self.assertRaises(ValueError):
                inspect(flash, b'\xff'*CACHE, trace.replace(old, new))


if __name__ == '__main__':
    unittest.main()
