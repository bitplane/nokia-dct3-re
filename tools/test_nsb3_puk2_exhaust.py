import tempfile
from pathlib import Path
import unittest
from unittest.mock import patch

from tools.nsb3_puk2_exhaust import check_permanent
from tools.nsb3_puk2_check import check_recovery
from tools.nsb3_puk2_retry_check import check_rejected_card
from tools.test_nsb3_puk2_retry_check import wrong_trace
from tools.test_nsb3_pin_request_check import trace
from tools.nsb3_pin_change_check import CHANGE_KEYS
from tools.make_sim_card_profile import make_current_profile
from tools.sim_security_trace_check import ATTEMPTS_OFFSET


class Nsb3Puk2ExhaustTest(unittest.TestCase):
    def test_each_puk2_retry_preserves_other_card_domains(self):
        for remaining in range(11):
            card = bytearray(make_current_profile(True))
            card[ATTEMPTS_OFFSET + 1] = 0
            card[ATTEMPTS_OFFSET + 3] = remaining
            check_rejected_card(card, remaining)
            with self.assertRaises(ValueError):
                check_rejected_card(card, (remaining + 1) % 11)

    def test_last_wrong_puk2_is_not_another_recoverable_error(self):
        text = wrong_trace().replace('ins=2c sw=9804', 'ins=2c sw=9840')
        check_recovery(text, wrong=True, blocked=True)
        with self.assertRaises(ValueError):
            check_recovery(text, wrong=True)

    def test_permanent_refusal_requires_pin1_and_physical_navigation(self):
        card = bytearray(make_current_profile(True))
        card[ATTEMPTS_OFFSET + 1] = card[ATTEMPTS_OFFSET + 3] = 0
        keys = CHANGE_KEYS[:12] + ('Scroll Down', 'Left Softkey / Menu', 'Scroll Down')
        text = trace(3) + '\n' + '\n'.join(
            f'nsb3_security_key: key={key} down={state} t=77.0 physical_only=1'
            for key in keys for state in (1, 0))
        with tempfile.TemporaryDirectory() as directory:
            previous, run = Path(directory) / 'previous', Path(directory) / 'run'
            for path in (previous, run):
                storage = path / 'nvram/nsb3r4t'
                storage.mkdir(parents=True)
                (storage / 'sim_card').write_bytes(card)
            with patch('tools.nsb3_puk2_exhaust.check_link'), patch('tools.nsb3_puk2_exhaust.check_own'), patch('tools.nsb3_puk2_exhaust.frame_check'):
                (run / 'error.log').write_text(text)
                check_permanent(previous, run)
                (run / 'error.log').write_text(text.replace('nsb3_security_key:', 'missing_key:'))
                with self.assertRaises(ValueError):
                    check_permanent(previous, run)
                dismiss = trace(3) + '\n' + '\n'.join(
                    f'nsb3_security_key: key={key} down={state} t=80.0 physical_only=1'
                    for key in keys[:-1] + ('Right Softkey / C', 'End', 'Left Softkey / Menu')
                    for state in (1, 0))
                (run / 'error.log').write_text(dismiss)
                check_permanent(previous, run, dismiss=True)
                with self.assertRaises(ValueError):
                    check_permanent(previous, run)


if __name__ == '__main__':
    unittest.main()
