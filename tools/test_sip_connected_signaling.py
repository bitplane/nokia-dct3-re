import json
import io
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from tools.run_sip_handset_gate import main, verify_success


class ConnectedSignalingTest(unittest.TestCase):
    def test_signaling_scope_rejects_unproved_combinations(self):
        for options in (['--incoming'], ['--cancel-incoming'], ['--record-media'], ['--restore-call'],
                        ['--product', '8890', '--incoming'], ['--sip-response', '486'], ['--calls', '2']):
            with self.subTest(options=options), \
                    patch('sys.argv', ['gate', '--pjsua', 'unused', '--run-dir', 'unused',
                                       '--product', '6210', '--signaling-only'] + options), \
                    patch('sys.stderr', new_callable=io.StringIO), \
                    self.assertRaises(SystemExit) as error:
                main()
            self.assertEqual(error.exception.code, 2)

    def run_check(self, counts, log='physical call trace', incoming=False, bridge_transform=None, product='6210'):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            bridge_text = (
                ('SIP incoming identity=(1, 1) caller=5551234\nSIP physical answer identity=(1, 1)\n' if incoming else
                 'digits=' + ('123' if product == '6250' else '1234567') + '\n') +
                'SIP confirmed status=200 identity=(1, 1)\n'
                'SIP bridge ended ' + json.dumps(counts) + '\n')
            (root / 'sip-bridge.log').write_text(bridge_transform(bridge_text) if bridge_transform else bridge_text)
            (root / 'error.log').write_text(log)
            args = SimpleNamespace(product=product, incoming=incoming, signaling_only=True)
            remote = 'state changed to CONFIRMED\nDISCONNECTED [reason=200 (OK)]'
            checker = (('tools.noki8890_incoming_call_check.verify' if incoming else
                        'tools.noki8890_outgoing_call_check.verify') if product == '8890' else
                       ('tools.noki6250_call_check.verify' if incoming else
                        'tools.noki6250_call_check.verify_outgoing') if product == '6250' else
                       'tools.noki6210_incoming_call_check.verify' if incoming else
                       'tools.noki6210_outgoing_call_check.verify')
            with patch(checker) as verify_call:
                verify_success(root, remote, args)
                if incoming and product == '8890':
                    verify_call.assert_called_once_with(log, caller='5551234', configured_gsm900=True)
                elif incoming and product == '6250':
                    verify_call.assert_called_once_with(log, configured_carrier=True)
                elif incoming:
                    verify_call.assert_called_once_with(log)
                elif product == '8890':
                    verify_call.assert_called_once_with(log, number='1234567', configured_gsm900=True)
                elif product == '6250':
                    verify_call.assert_called_once_with(log, number='123', configured_carrier=True)
                else:
                    verify_call.assert_called_once_with(log, number='1234567')
            return json.loads((root / 'sip-result.json').read_text())

    def test_8890_uses_own_gsm900_checker_without_promoting_speech(self):
        result = self.run_check({'uplink': 0, 'downlink': 300}, product='8890')
        self.assertEqual(result['dialed_digits'], '1234567')
        self.assertTrue(result['scope'].startswith('8890 '))
        self.assertFalse(result['speech_tested'])
        with self.assertRaises(RuntimeError):
            self.run_check({'uplink': 1}, product='8890')

    def test_8890_incoming_uses_own_caller_and_gsm900_contract(self):
        result = self.run_check({'uplink': 0}, product='8890', incoming=True)
        self.assertEqual(result['caller'], '5551234')
        self.assertNotIn('dialed_digits', result)
        self.assertFalse(result['speech_tested'])
        for transform in (
                lambda text: text.replace('SIP physical answer identity=(1, 1)',
                                          'SIP physical answer identity=(2, 1)'),
                lambda text: text + 'SIP physical answer identity=(1, 1)\n'):
            with self.assertRaises(RuntimeError):
                self.run_check({'uplink': 0}, product='8890', incoming=True,
                               bridge_transform=transform)

    def test_8890_signaling_scope_rejects_restoration_and_media(self):
        with tempfile.TemporaryDirectory() as directory:
            ready = str(Path(directory) / 'not-yet-captured.png')
            for options in (['--incoming', '--restore-outgoing'], ['--restore-call'], ['--record-media'],
                            ['--calls', '2'], ['--incoming', '--restore-call', '--restore-phase', 'alerting', '--ready-file', ready]):
                with self.subTest(options=options), \
                        patch('sys.argv', ['gate', '--pjsua', 'unused', '--run-dir', 'unused',
                                           '--product', '8890', '--signaling-only'] + options), \
                        patch('sys.stderr', new_callable=io.StringIO), self.assertRaises(SystemExit) as error:
                    main()
                self.assertEqual(error.exception.code, 2)

    def test_6250_uses_own_carrier_checker_without_promoting_speech(self):
        result = self.run_check({'uplink': 0, 'downlink': 300}, product='6250')
        self.assertEqual(result['dialed_digits'], '123')
        self.assertTrue(result['scope'].startswith('6250 '))
        self.assertFalse(result['speech_tested'])
        with self.assertRaises(RuntimeError):
            self.run_check({'uplink': 1}, product='6250')

    def test_6250_signaling_scope_rejects_unready_incoming_and_restoration(self):
        for options in (['--incoming'], ['--incoming', '--restore-outgoing'], ['--restore-call'],
                        ['--record-media'], ['--calls', '2'], ['--sip-response', '486']):
            with self.subTest(options=options), \
                    patch('sys.argv', ['gate', '--pjsua', 'unused', '--run-dir', 'unused',
                                       '--product', '6250', '--signaling-only'] + options), \
                    patch('sys.stderr', new_callable=io.StringIO), self.assertRaises(SystemExit) as error:
                main()
            self.assertEqual(error.exception.code, 2)

    def test_6250_incoming_uses_own_carrier_answer_checker(self):
        result = self.run_check({'uplink': 0}, product='6250', incoming=True)
        self.assertEqual(result['caller'], '5551234')
        self.assertNotIn('dialed_digits', result)
        self.assertFalse(result['speech_tested'])
        for transform in (
                lambda text: text.replace('SIP physical answer identity=(1, 1)',
                                          'SIP physical answer identity=(2, 1)'),
                lambda text: text + 'SIP physical answer identity=(1, 1)\n'):
            with self.assertRaises(RuntimeError):
                self.run_check({'uplink': 0}, product='6250', incoming=True,
                               bridge_transform=transform)

    def test_scope_does_not_promote_bridge_downlink_to_speech(self):
        result = self.run_check({'uplink': 0, 'downlink': 300, 'pcm_received': 300})
        self.assertTrue(result['passed'])
        self.assertFalse(result['speech_tested'])
        self.assertFalse(result['native_dsp_complete'])
        self.assertNotIn('media', result)

    def test_incoming_uses_own_answer_checker_and_caller_scope(self):
        result = self.run_check({'uplink': 0}, incoming=True)
        self.assertEqual(result['caller'], '5551234')
        self.assertNotIn('dialed_digits', result)
        self.assertFalse(result['speech_tested'])

    def test_incoming_requires_one_ordered_correlated_answer(self):
        for transform in (
                lambda text: text.replace('identity=(1, 1)', 'identity=(2, 1)'),
                lambda text: text + 'SIP physical answer identity=(1, 1)\n',
                lambda text: text.replace('SIP physical answer identity=(1, 1)\n', '') +
                             'SIP physical answer identity=(1, 1)\n'):
            with self.subTest(transform=transform), self.assertRaises(RuntimeError):
                self.run_check({'uplink': 0}, incoming=True, bridge_transform=transform)

    def test_rejects_unexpected_or_missing_handset_media_count(self):
        for counts in ({}, {'uplink': 1}):
            with self.subTest(counts=counts), self.assertRaises(RuntimeError):
                self.run_check(counts)

    def test_rejects_codec_execution(self):
        with self.assertRaises(RuntimeError):
            self.run_check({'uplink': 0}, 'dsp_hle: speech tick uplink=1')

    def test_requires_product_owned_physical_call_check(self):
        with patch('tools.noki6210_outgoing_call_check.verify', side_effect=ValueError('missing End')):
            # run_check's positive mock is not used here: exercise the verifier directly.
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'sip-bridge.log').write_text('digits=1234567\nSIP confirmed status=200\nSIP bridge ended {"uplink":0}\n')
                (root / 'error.log').write_text('')
                with self.assertRaisesRegex(ValueError, 'missing End'):
                    verify_success(root, 'state changed to CONFIRMED\nDISCONNECTED [reason=200 (OK)]',
                                   SimpleNamespace(product='6210', incoming=False, signaling_only=True))
