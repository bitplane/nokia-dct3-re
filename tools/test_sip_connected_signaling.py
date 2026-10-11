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
        for options in (['--incoming'], ['--record-media'], ['--restore-outgoing'],
                        ['--product', '8890'], ['--sip-response', '486'], ['--calls', '2']):
            with self.subTest(options=options), \
                    patch('sys.argv', ['gate', '--pjsua', 'unused', '--run-dir', 'unused',
                                       '--product', '6210', '--signaling-only'] + options), \
                    patch('sys.stderr', new_callable=io.StringIO), \
                    self.assertRaises(SystemExit) as error:
                main()
            self.assertEqual(error.exception.code, 2)

    def run_check(self, counts, log='physical call trace'):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'sip-bridge.log').write_text(
                'digits=1234567\nSIP confirmed status=200\n'
                'SIP bridge ended ' + json.dumps(counts) + '\n')
            (root / 'error.log').write_text(log)
            args = SimpleNamespace(product='6210', incoming=False, signaling_only=True)
            remote = 'state changed to CONFIRMED\nDISCONNECTED [reason=200 (OK)]'
            with patch('tools.noki6210_outgoing_call_check.verify') as verify_call:
                verify_success(root, remote, args)
                verify_call.assert_called_once_with(log, number='1234567')
            return json.loads((root / 'sip-result.json').read_text())

    def test_scope_does_not_promote_bridge_downlink_to_speech(self):
        result = self.run_check({'uplink': 0, 'downlink': 300, 'pcm_received': 300})
        self.assertTrue(result['passed'])
        self.assertFalse(result['speech_tested'])
        self.assertFalse(result['native_dsp_complete'])
        self.assertNotIn('media', result)

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
