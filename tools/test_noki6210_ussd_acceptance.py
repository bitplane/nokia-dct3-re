import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.run_noki6210_acceptance import check_ussd, events, USSD_RESULT_SHA256, OPERATOR_SHA256


class UssdAcceptanceTest(unittest.TestCase):
    def test_own_product_frames_and_physical_send(self):
        with patch('tools.noki8210_supplementary_check.verify_transaction') as verify:
            check_ussd('trace', Path('frames'))
        args, kwargs = verify.call_args
        self.assertEqual(args[3], ('Keypad *', 'Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad #', 'Send'))
        self.assertEqual(args[5:7], (USSD_RESULT_SHA256, OPERATOR_SHA256))
        self.assertEqual(kwargs, {'product': '6210', 'geometry': (96, 60)})

    def test_service_decode_survives_filter(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'error.log'
            path.write_text('irrelevant\ngsm_ss: request=ussd outcome=0\n6210_ussd_physical: key=Send\n')
            self.assertEqual(events(path), 'gsm_ss: request=ussd outcome=0\n6210_ussd_physical: key=Send\n')


if __name__ == '__main__':
    unittest.main()
