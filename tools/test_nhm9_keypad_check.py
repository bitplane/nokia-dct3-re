from pathlib import Path
import unittest

from tools.nhm9_keypad_check import check_tables, check_trace


class KeypadCheckTest(unittest.TestCase):
    marker = 'nhm9_keypad: PASS matrix_keys=18 scans=90 power_mask=02 ui_delivery=unvalidated\n'

    def test_complete(self):
        check_trace(self.marker, 0)

    def test_incomplete_or_crashed(self):
        for trace, code in [('', 0), (self.marker, 1),
                            (self.marker.replace('scans=90', 'scans=85'), 0),
                            (self.marker * 2, 0)]:
            with self.subTest(trace=trace, code=code), self.assertRaises(ValueError):
                check_trace(trace, code)

    def test_lua_error_after_pass(self):
        with self.assertRaises(ValueError):
            check_trace(self.marker + '[LUA ERROR]', 0)

    def test_unknown_source_rejected(self):
        with self.assertRaises(ValueError):
            check_tables(b'not firmware')

    def test_own_tables_when_available(self):
        source = Path(__file__).resolve().parents[1] / 'roms/noki3350/3350f530a.fls'
        if not source.exists():
            self.skipTest('own normalized source unavailable')
        check_tables(source.read_bytes())
