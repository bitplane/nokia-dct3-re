import hashlib
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch

from tools import nsb1_native_state_check as checker
from tools.test_nse2_native_state_check import trace as architecture


def trace():
    return architecture().replace('nse2_', 'nsb1_').replace('18.500000000', '40.000000000')


class Nsb1NativeStateTest(unittest.TestCase):
    def test_architecture_replay_physical_input_and_pixels(self):
        card, pixels = bytes(3570), b'Phone book'
        with patch.object(checker, 'MENU', hashlib.sha256(pixels).hexdigest()):
            result = checker.check_trace(trace(), card, card, pixels, pixels)
            self.assertEqual(result['io_replay_events'], 8180)
            self.assertFalse(result['native_registration_verified'])
            for bad in (trace().replace('ILLEGAL:0', 'ILLEGAL:1'),
                        trace().replace('restored=8180', 'restored=8179'),
                        trace().replace('down=0', 'down=1'),
                        trace().replace('40.000000000', '39.000000000'),
                        trace() + '\nnsb1_state: phase=saved malformed',
                        trace() + '\nnsb1_reset_reason: reason=68',
                        trace() + '\nunhandled opcode'):
                with self.subTest(tail=bad[-100:]), self.assertRaises(ValueError):
                    checker.check_trace(bad, card, card, pixels, pixels)
            with self.assertRaises(ValueError):
                checker.check_trace(trace(), card, card, pixels, b'blank')
            with self.assertRaises(ValueError):
                checker.check_trace(trace(), card, card[:-1] + b'\x01', pixels, pixels)

    def test_no_blanket_exclusion_for_architecture_error_lines(self):
        pixels, card = b'Phone book', bytes(3570)
        with patch.object(checker, 'MENU', hashlib.sha256(pixels).hexdigest()):
            for error in ('illegal instruction', 'unsupported instruction', 'fatal error',
                          'LUA ERROR', 'nsb1_state: FAIL incomplete'):
                with self.subTest(error=error), self.assertRaises(ValueError):
                    checker.check_trace(trace() + '\n' + error, card, card, pixels, pixels)

    def test_entire_eeprom_is_checked_not_only_the_diagnostic_selector(self):
        with TemporaryDirectory() as temporary:
            directory = Path(temporary)
            storage = directory / 'nvram/nsb1r4t'
            storage.mkdir(parents=True)
            (storage / 'flash').write_bytes(b'own code')
            (storage / 'sim_card').write_bytes(b'own card')
            (storage / 'eeprom').write_bytes(bytes(8192))
            with patch.object(checker, 'contract') as code, patch.object(checker, 'card_check') as card, \
                    patch.object(checker, 'EEPROM', hashlib.sha256(bytes(8192)).hexdigest()):
                checker.check_storage(directory)
                code.assert_called_once_with(b'own code')
                card.assert_called_once_with(b'own card')
                for offset in (0, 0x4a1, 8191):
                    changed = bytearray(8192)
                    changed[offset] = 1
                    (storage / 'eeprom').write_bytes(changed)
                    with self.subTest(offset=offset), self.assertRaises(ValueError):
                        checker.check_storage(directory)


if __name__ == '__main__':
    unittest.main()
