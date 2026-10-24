import hashlib
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch

from tools import nsk1_native_state_check as checker
from tools.test_nse2_native_state_check import trace as architecture
from tools.test_nsk1_diagnostic_display_check import trace as authentication


def trace():
    state = architecture().replace('nse2_', 'nsk1_').replace('18.500000000', '40.000000000')
    return (authentication(True) + '\n' + state +
            '\nnsk1_code_key: value=19 t=41.5 observer_writes=0' * 2)


class Nsk1NativeStateTest(unittest.TestCase):
    def test_architecture_replay_and_own_physical_authentication_continuation(self):
        card, pixels = bytes(3570), b'Phone book'
        with patch.object(checker, 'MENU', hashlib.sha256(pixels).hexdigest()):
            result = checker.check_trace(trace(), card, card, pixels, pixels)
            self.assertEqual(result['io_replay_events'], 8180)
            self.assertFalse(result['native_registration_verified'])
            for bad in (trace().replace('ILLEGAL:0', 'ILLEGAL:1'),
                        trace().replace('restored=8180', 'restored=8179'),
                        trace().replace('40.000000000', '39.000000000'),
                        trace().replace('match=00000001', 'match=00000000'),
                        trace().replace('value=19 t=41.5', 'value=18 t=41.5'),
                        trace() + '\nnsk1_code_key: malformed',
                        trace() + '\nnsk1_state: phase=saved malformed',
                        trace() + '\nunimplemented instruction',
                        trace() + '\nnsk1_restart_request: reason=68'):
                with self.subTest(tail=bad[-100:]), self.assertRaises(ValueError):
                    checker.check_trace(bad, card, card, pixels, pixels)
            with self.assertRaises(ValueError):
                checker.check_trace(trace(), card, card, pixels, b'blank')
            with self.assertRaises(ValueError):
                checker.check_trace(trace(), card, card[:-1] + b'\x01', pixels, pixels)

    def test_complete_pin_disabled_card_and_product_owned_bank_are_required(self):
        with TemporaryDirectory() as temporary:
            directory = Path(temporary)
            storage = directory / 'nvram/nsk1r4t'
            storage.mkdir(parents=True)
            (storage / 'flash').write_bytes(b'own code')
            (storage / 'eeprom').write_bytes(bytes(2048))
            (storage / 'sim_card').write_bytes(checker.make_current_profile(False))
            with patch.object(checker, 'contract') as contract, patch.object(checker, 'check_bank') as bank:
                checker.check_storage(directory)
                contract.assert_called_once_with(b'own code')
                bank.assert_called_once_with(bytes(2048), True)
                for card in (checker.make_current_profile(True),
                             checker.make_current_profile(False)[:-1]):
                    (storage / 'sim_card').write_bytes(card)
                    with self.assertRaises(ValueError):
                        checker.check_storage(directory)


if __name__ == '__main__':
    unittest.main()
