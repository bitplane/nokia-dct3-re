import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

from tools.run_sip_handset_gate import verify_success


SETUP = {
    '3210': 'length=15 data=03450401a05e0581551532f4150101',
    '3310': 'length=19 data=03450404600200815e0581551532f4a2150101',
}


class SipProductSetupCheckTest(unittest.TestCase):
    def check(self, product, setup_product, accepted=100, rejected=False):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'error.log').write_text(
                'GSM service uplink sapi=0 pd=03 message=05 ' + SETUP[setup_product] + '\n'
                'GSM service downlink kind=12 sapi=0 pd=03 message=07\n'
                'GSM service uplink sapi=0 pd=03 message=0f data=030f\n'
                'GSM service uplink sapi=0 pd=03 message=2d data=032d\n'
                'LAPDm service Channel Release acknowledged\n' + ''.join(
                    f'gsm_call_adapter: media direction=downlink id=1 sequence={i} result=accepted\n'
                    for i in range(accepted)) + (
                    f'gsm_call_adapter: media direction=downlink id=1 sequence={accepted} result=rejected\n'
                    if rejected else ''))
            counts = dict.fromkeys(('uplink', 'downlink', 'pcm_transmitted', 'pcm_received'), 100)
            (root / 'sip-bridge.log').write_text(
                'SIP dial digits=5551234\nSIP confirmed status=200\n'
                'SIP bridge ended ' + json.dumps(counts) + '\n')
            verify_success(root, 'state changed to CONFIRMED\nDISCONNECTED [reason=200 (OK)]',
                           SimpleNamespace(incoming=False, restore_idle=False, product=product))
            result = json.loads((root / 'sip-result.json').read_text())
            self.assertTrue(result['scope'].startswith(product + ' research-HLE'))

    def test_own_product_setup_is_accepted(self):
        for product in SETUP:
            with self.subTest(product=product):
                self.check(product, product)

    def test_other_product_setup_is_not_substituted(self):
        for product, other in (('3210', '3310'), ('3310', '3210')):
            with self.subTest(product=product), self.assertRaises(RuntimeError):
                self.check(product, other)

    def test_transmitted_frames_do_not_prove_handset_acceptance(self):
        with self.assertRaises(RuntimeError):
            self.check('3210', '3210', accepted=17)
        with self.assertRaises(RuntimeError):
            self.check('3210', '3210', rejected=True)


if __name__ == '__main__':
    unittest.main()
