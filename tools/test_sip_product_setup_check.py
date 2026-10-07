import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

from tools.run_sip_handset_gate import verify_success


SETUP = {
    '3210': 'length=15 data=03450401a05e0581551532f4150101',
    '3310': 'length=19 data=03450404600200815e0581551532f4a2150101',
    '3330': 'length=18 data=03450404600200815e0581551532f4150101',
    '3410': 'length=15 data=03450401a05e0581551532f4150101',
    '5210': 'length=15 data=03450401a05e0581551532f4150101',
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

    def check_incoming(self, product, wire_product, release_override=None, answer_override=None):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            connect, release = {
                '3210': ('8347', '032a0802e0d1'),
                '3310': ('8307', '036a0802e0d1'),
                '3330': ('8307', '032a0802e0d1'),
                '3410': ('8307', '036a0802e0d1'),
                '5210': ('8307', '036a0802e0d1'),
            }[wire_product]
            if release_override is not None:
                release = release_override
            answer_key = answer_override or ('send' if wire_product in ('3410', '5210') else 'enter')
            (root / 'error.log').write_text(
                'gsm_call_adapter: incoming state id=1 epoch=1 phase=paging\n'
                'GSM service downlink kind=9 sapi=0 pd=03 message=05\n'
                f'input-press: t=18.0 name={answer_key}\n'
                f'GSM service uplink sapi=0 pd=03 message=07 length=2 data={connect}\n'
                'gsm_call_adapter: incoming state id=1 epoch=1 phase=connected\n'
                'gsm_call_adapter: termination id=1 cause=16 result=accepted\n'
                f'GSM service uplink sapi=0 pd=03 message=2a length=6 data={release}\n'
                'gsm_call_adapter: incoming state id=1 epoch=1 phase=ended\n' + ''.join(
                    f'gsm_call_adapter: media direction=downlink id=1 sequence={i} result=accepted\n'
                    for i in range(100)))
            counts = dict.fromkeys(('uplink', 'downlink', 'pcm_transmitted', 'pcm_received'), 100)
            (root / 'sip-bridge.log').write_text(
                'SIP incoming caller=5551234\nSIP physical answer identity=(1, 1)\n'
                'SIP confirmed status=200\nSIP bridge ended ' + json.dumps(counts) + '\n')
            verify_success(root, 'state changed to CONFIRMED\nDISCONNECTED [reason=200 (OK)]',
                           SimpleNamespace(incoming=True, restore_idle=False, product=product))

    def test_own_incoming_connect_and_release_are_accepted(self):
        for product in SETUP:
            with self.subTest(product=product):
                self.check_incoming(product, product)

    def test_other_product_incoming_encoding_is_rejected(self):
        for product, other in (('3210', '3310'), ('3310', '3210')):
            with self.subTest(product=product), self.assertRaises(RuntimeError):
                self.check_incoming(product, other)

    def test_send_products_require_physical_send_not_navi(self):
        for product in ('3410', '5210'):
            for key in ('enter', 'send_extra'):
                with self.assertRaises(RuntimeError):
                    self.check_incoming(product, product, answer_override=key)

    def test_3330_release_accepts_sequence_bit_not_wrong_payload(self):
        for release in ('032a0802e0d1', '036a0802e0d1'):
            self.check_incoming('3330', '3330', release)
        for release in ('036a0802e0d100', '036a0802e0d2'):
            with self.assertRaises(RuntimeError):
                self.check_incoming('3330', '3330', release)


if __name__ == '__main__':
    unittest.main()
