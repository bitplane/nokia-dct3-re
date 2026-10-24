import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from tools.run_sip_handset_gate import verify_success


SETUP = {
    '3210': 'length=15 data=03450401a05e0581551532f4150101',
    '3310': 'length=19 data=03450404600200815e0581551532f4a2150101',
    '3330': 'length=18 data=03450404600200815e0581551532f4150101',
    '3410': 'length=15 data=03450401a05e0581551532f4150101',
    '5210': 'length=15 data=03450401a05e0581551532f4150101',
    '6210': 'length=15 data=03450401a05e0581214365f7150101',
    '8210': 'length=15 data=03450401a05e0581214365f7150101',
    '8850': 'length=18 data=03450404600200815e0581551532f4150101',
}


class SipProductSetupCheckTest(unittest.TestCase):
    def check(self, product, setup_product, accepted=100, rejected=False, reason='200 (OK)',
              disconnect=False, missing_release=False, dcs1800=False):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'error.log').write_text(
                'GSM service uplink sapi=0 pd=03 message=05 ' + SETUP[setup_product] + '\n'
                'GSM service downlink kind=12 sapi=0 pd=03 message=07\n'
                'GSM service uplink sapi=0 pd=03 message=0f data=030f\n'
                + (('GSM service uplink sapi=0 pd=03 message=25 data=036502e090\n'
                    + ('' if missing_release else
                       'GSM service downlink kind=25 sapi=0 pd=03 message=2d\n')
                    + 'GSM service uplink sapi=0 pd=03 message=2a data=032a\n')
                   if disconnect else 'GSM service uplink sapi=0 pd=03 message=2d data=032d\n') +
                'LAPDm service Channel Release acknowledged\n' + ''.join(
                    f'gsm_call_adapter: media direction=downlink id=1 sequence={i} result=accepted\n'
                    for i in range(accepted)) + (
                    f'gsm_call_adapter: media direction=downlink id=1 sequence={accepted} result=rejected\n'
                    if rejected else ''))
            counts = dict.fromkeys(('uplink', 'downlink', 'pcm_transmitted', 'pcm_received'), 100)
            number = '1234567' if product in ('6210', '8210') else '5551234'
            (root / 'sip-bridge.log').write_text(
                f'SIP dial digits={number}\nSIP confirmed status=200\n'
                'SIP bridge ended ' + json.dumps(counts) + '\n')
            verify_success(root, 'state changed to CONFIRMED\nDISCONNECTED [reason=' + reason + ']',
                           SimpleNamespace(incoming=False, restore_idle=False, product=product,
                                           dcs1800=dcs1800))
            result = json.loads((root / 'sip-result.json').read_text())
            self.assertTrue(result['scope'].startswith(product + ' research-HLE'))

    def test_own_product_setup_is_accepted(self):
        for product in SETUP:
            if product in ('8210', '8850'):
                continue  # Own physical-End lifecycle is tested separately.
            with self.subTest(product=product):
                self.check(product, product)

    def test_5210_physical_disconnect_requires_complete_release(self):
        self.check('5210', '5210', disconnect=True)
        with self.assertRaisesRegex(RuntimeError, 'message=2d'):
            self.check('5210', '5210', disconnect=True, missing_release=True)

    def test_8210_uses_full_own_product_lifecycle(self):
        with self.assertRaisesRegex(ValueError, 'physical Send'):
            self.check('8210', '8210')
        with patch('tools.noki8210_outgoing_call_check.verify') as own_verify:
            self.check('8210', '8210')
            own_verify.assert_called_once()
            self.assertEqual(own_verify.call_args.kwargs,
                             {'number': '1234567', 'configured_carrier': True})

    def test_8850_uses_full_own_product_lifecycle(self):
        with self.assertRaisesRegex(ValueError, 'physical Send'):
            self.check('8850', '8850')
        with patch('tools.noki8850_outgoing_call_check.verify') as own_verify:
            self.check('8850', '8850')
            own_verify.assert_called_once()
            self.assertEqual(own_verify.call_args.kwargs, {'number': '5551234'})

    def test_8210_dcs_selects_exact_dcs_contract(self):
        with patch('tools.noki8210_outgoing_call_check.verify') as own_verify:
            self.check('8210', '8210', dcs1800=True)
            self.assertEqual(own_verify.call_args.kwargs,
                             {'number': '1234567', 'dcs1800': True})
        with patch('tools.noki8210_incoming_call_check.verify') as own_verify:
            self.check_incoming('8210', '3210', dcs1800=True)
            self.assertEqual(own_verify.call_args.kwargs, {'dcs1800': True})

    def test_normal_clearing_status_wording(self):
        self.check('3210', '3210', reason='200 (Normal call clearing)')
        for reason in ('2000 (OK)', '486 (Normal call clearing)'):
            with self.subTest(reason=reason), self.assertRaises(RuntimeError):
                self.check('3210', '3210', reason=reason)

    def test_other_product_setup_is_not_substituted(self):
        for product, other in (('3210', '3310'), ('3310', '3210'), ('6210', '3210')):
            with self.subTest(product=product), self.assertRaises(RuntimeError):
                self.check(product, other)

    def test_transmitted_frames_do_not_prove_handset_acceptance(self):
        with self.assertRaises(RuntimeError):
            self.check('3210', '3210', accepted=17)
        with self.assertRaises(RuntimeError):
            self.check('3210', '3210', rejected=True)

    def check_incoming(self, product, wire_product, release_override=None, answer_override=None,
                       pin_end=False, missing=None, dcs1800=False):
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
            end_lines = [
                '5210_pin_physical: key=End',
                'GSM service uplink sapi=0 pd=03 message=25 length=5 data=836502e090',
                'GSM service downlink kind=25 sapi=0 pd=03 message=2d length=2',
                'GSM service uplink sapi=0 pd=03 message=2a length=2 data=832a',
            ]
            release_trace = ('\n'.join(line for index, line in enumerate(end_lines)
                                      if index != missing) + '\n' if pin_end else
                             'gsm_call_adapter: termination id=1 cause=16 result=accepted\n'
                             f'GSM service uplink sapi=0 pd=03 message=2a length=6 data={release}\n')
            answer_trace = ('5210_pin_physical: key=' + (answer_override or 'Send') + '\n'
                            if pin_end else f'input-press: t=18.0 name={answer_key}\n')
            (root / 'error.log').write_text(
                'gsm_call_adapter: incoming state id=1 epoch=1 phase=paging\n'
                'GSM service downlink kind=9 sapi=0 pd=03 message=05\n'
                + answer_trace +
                f'GSM service uplink sapi=0 pd=03 message=07 length=2 data={connect}\n'
                'gsm_call_adapter: incoming state id=1 epoch=1 phase=connected\n'
                + release_trace +
                'gsm_call_adapter: incoming state id=1 epoch=1 phase=ended\n' + ''.join(
                    f'gsm_call_adapter: media direction=downlink id=1 sequence={i} result=accepted\n'
                    for i in range(100)))
            counts = dict.fromkeys(('uplink', 'downlink', 'pcm_transmitted', 'pcm_received'), 100)
            (root / 'sip-bridge.log').write_text(
                'SIP incoming caller=5551234\nSIP physical answer identity=(1, 1)\n'
                'SIP confirmed status=200\nSIP bridge ended ' + json.dumps(counts) + '\n')
            verify_success(root, 'state changed to CONFIRMED\nDISCONNECTED [reason=200 (OK)]',
                           SimpleNamespace(incoming=True, restore_idle=False, product=product,
                                           dcs1800=dcs1800))

    def test_5210_pin_physical_end_requires_entire_release(self):
        self.check_incoming('5210', '5210', pin_end=True)
        for missing in range(4):
            with self.subTest(missing=missing), self.assertRaises(RuntimeError):
                self.check_incoming('5210', '5210', pin_end=True, missing=missing)
        for key in ('Enter', 'Send_extra'):
            with self.subTest(key=key), self.assertRaises(RuntimeError):
                self.check_incoming('5210', '5210', pin_end=True, answer_override=key)

    def test_own_incoming_connect_and_release_are_accepted(self):
        for product in SETUP:
            if product in ('6210', '8210', '8850'):
                continue  # The outgoing research probe does not promote incoming Answer.
            with self.subTest(product=product):
                self.check_incoming(product, product)

    def test_8210_incoming_requires_own_physical_lifecycle(self):
        with self.assertRaises(ValueError):
            self.check_incoming('8210', '3210')
        with patch('tools.noki8210_incoming_call_check.verify') as own_verify:
            self.check_incoming('8210', '3210')
            own_verify.assert_called_once()
            self.assertEqual(own_verify.call_args.kwargs, {'configured_carrier': True})

    def test_8850_incoming_requires_own_physical_lifecycle(self):
        with self.assertRaises(ValueError):
            self.check_incoming('8850', '3210')
        with patch('tools.noki8850_incoming_call_check.verify') as own_verify:
            self.check_incoming('8850', '3210')
            own_verify.assert_called_once()

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
