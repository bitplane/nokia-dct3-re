import json
from pathlib import Path
import tempfile
import unittest
import subprocess
import sys

from tools.run_sip_handset_gate import verify_cancel


LOG = '''
gsm_call_adapter: incoming state id=1 epoch=1 phase=paging
GSM service downlink kind=9 sapi=0 pd=03 message=05
gsm_call_adapter: incoming state id=1 epoch=1 phase=alerting
gsm_call_adapter: termination id=1 cause=16 result=accepted
GSM service downlink kind=13 sapi=0 pd=03 message=25
GSM service uplink sapi=0 pd=03 message=2a
LAPDm service Channel Release acknowledged nr=2
gsm_call_adapter: incoming state id=1 epoch=1 phase=ended
'''
REMOTE = 'Request msg CANCEL/\nResponse msg 487/INVITE/\n'
COUNTS = dict(uplink=0, downlink=0, pcm_transmitted=0, pcm_received=0)


class SipCancelCheckTest(unittest.TestCase):
    def check(self, log=LOG, remote=REMOTE, counts=None, extra='', product='3210', epoch=1):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'error.log').write_text(log)
            (root / 'sip-bridge.log').write_text(
                f'SIP disconnected status=487 identity=({epoch}, 1)\n'
                + 'SIP bridge ended ' + json.dumps(COUNTS if counts is None else counts)
                + '\n' + extra)
            verify_cancel(root, remote, product)
            return json.loads((root / 'sip-result.json').read_text())

    def test_cancel_completes_without_answer(self):
        self.check()

    def test_fresh_call_after_idle_restore_uses_new_epoch(self):
        result = self.check(log=LOG.replace('epoch=1', 'epoch=2'), epoch=2, product='6210')
        self.assertEqual(result['epoch'], 2)

    def test_stale_bridge_identity_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(log=LOG.replace('epoch=1', 'epoch=2'))

    def test_mixed_handset_epochs_are_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(log=LOG.replace('epoch=1 phase=alerting', 'epoch=2 phase=alerting'))

    def test_sibling_scopes(self):
        for product in ('3310', '3330', '3410', '5210', '6210', '8890'):
            self.assertTrue(self.check(product=product)['scope'].startswith(product + ' HLE'))

    def test_signaling_only_cli_rejects_answered_or_media_promotion(self):
        script = Path(__file__).with_name('run_sip_handset_gate.py')
        with tempfile.TemporaryDirectory() as directory:
            for product in ('6210', '8890'):
                for extra in ([], ['--incoming'],
                              ['--incoming', '--cancel-incoming', '--record-media'],
                              ['--incoming', '--cancel-incoming', '--restore-call']):
                    with self.subTest(product=product, extra=extra):
                        result = subprocess.run([sys.executable, str(script), '--pjsua', 'absent',
                                                 '--run-dir', directory, '--product', product, *extra],
                                                capture_output=True, text=True)
                        self.assertEqual(result.returncode, 2)
                        self.assertIn('limited to unanswered incoming CANCEL', result.stderr)

    def test_stale_ready_file_is_rejected_before_launch(self):
        script = Path(__file__).with_name('run_sip_handset_gate.py')
        with tempfile.TemporaryDirectory() as directory:
            ready = Path(directory) / 'ready.png'
            ready.touch()
            result = subprocess.run([sys.executable, str(script), '--pjsua', 'absent',
                                     '--run-dir', directory, '--incoming', '--cancel-incoming',
                                     '--ready-file', str(ready)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 2)
            self.assertIn('must not already exist', result.stderr)

    def test_incomplete_radio_release_is_rejected(self):
        for line in ('GSM service downlink kind=13 sapi=0 pd=03 message=25',
                     'LAPDm service Channel Release acknowledged nr=2'):
            with self.assertRaises(RuntimeError):
                self.check(log=LOG.replace(line, ''))

    def test_duplicate_clear_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(log=LOG + 'gsm_call_adapter: termination id=1 cause=16 result=accepted\n')

    def test_missing_cancel_or_release_is_rejected(self):
        for remote in ('', 'Request msg CANCEL/\n'):
            with self.assertRaises(RuntimeError):
                self.check(remote=remote)
        with self.assertRaises(RuntimeError):
            self.check(log=LOG.replace('phase=ended', 'phase=alerting'))

    def test_answer_or_media_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(extra='SIP physical answer')
        with self.assertRaises(RuntimeError):
            self.check(counts={**COUNTS, 'pcm_received': 1})

    def test_reordered_release_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(log='\n'.join(reversed(LOG.splitlines())))

    def test_handset_connection_is_rejected_even_with_zero_bridge_counters(self):
        for state in ('incoming state', 'state'):
            with self.subTest(state=state), self.assertRaises(RuntimeError):
                self.check(log=LOG + f'gsm_call_adapter: {state} id=1 epoch=1 phase=connected\n',
                           product='6210')

    def test_accepted_handset_media_is_rejected_even_with_zero_bridge_counters(self):
        for direction in ('uplink', 'downlink'):
            with self.subTest(direction=direction), self.assertRaises(RuntimeError):
                self.check(log=LOG + 'gsm_call_adapter: media '
                           f'direction={direction} id=1 sequence=0 result=accepted\n',
                           product='6210')


if __name__ == '__main__':
    unittest.main()
