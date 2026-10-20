import unittest
from unittest.mock import patch
from tools.nsk1_fdn_dial_check import check_dial, LISTED_KEYS, UNLISTED_KEYS


class Nsk1FdnDialTests(unittest.TestCase):
    def test_local_cases_use_their_own_keys_and_persistent_protocol_phase(self):
        for unlisted, enabled in ((False, True), (True, True), (True, False)):
            with patch('tools.nsk1_fdn_dial_check.check_input') as inputs, \
                 patch('tools.nsk1_fdn_dial_check.check_phase') as protocol:
                check_dial('trace', unlisted, enabled)
                inputs.assert_called_once_with('trace', UNLISTED_KEYS if unlisted else LISTED_KEYS)
                protocol.assert_called_once_with('trace', 1 if enabled else 3, 'nsk1_sim_tx')

    def test_dial_cannot_mutate_sim_files(self):
        for ins in ('dc', 'd6'):
            trace = f'SIM status ins={ins} sw=9000 chv=3/3 puk=10/10 enabled=1'
            with patch('tools.nsk1_fdn_dial_check.check_input'), \
                 patch('tools.nsk1_fdn_dial_check.check_phase'):
                with self.assertRaises(ValueError):
                    check_dial(trace, False, True)


if __name__ == '__main__':
    unittest.main()
