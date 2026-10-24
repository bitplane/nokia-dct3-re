import json
from pathlib import Path
import tempfile
import subprocess
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from tools.run_sip_handset_gate import verify_success, verify_pre_restore_media


class Nsb6SipMediaTest(unittest.TestCase):
    def test_restore_requires_actual_media_before_save_and_one_discard(self):
        counts = dict(uplink=15, downlink=10, pcm_transmitted=15, pcm_received=20)
        bridge = 'SIP epoch media discarded ' + json.dumps(counts) + '\n'
        log = 'dsp_hle: speech tick\nsip_state: saved\nsip_state: restored\n'
        verify_pre_restore_media(log, bridge)
        for field in counts:
            bad = dict(counts)
            bad[field] = 0
            with self.subTest(field=field), self.assertRaises(RuntimeError):
                verify_pre_restore_media(log, 'SIP epoch media discarded ' + json.dumps(bad))
        for bad_log, bad_bridge in ((log, ''), (log, bridge * 2),
                                    ('sip_state: saved\ndsp_hle: speech tick', bridge)):
            with self.assertRaises(RuntimeError):
                verify_pre_restore_media(bad_log, bad_bridge)

    def check(self, incoming=False, counts=None, setup_number='214365f7', pcs1900=False):
        counts = counts or dict(uplink=150, downlink=140, pcm_transmitted=150, pcm_received=145)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            identity = ('SIP incoming identity=(1, 1) caller=5551234\nSIP physical answer identity=(1, 1)' if incoming else
                        'digits=1234567')
            (root / 'sip-bridge.log').write_text(
                identity + '\nSIP confirmed status=200 identity=(1, 1)\nSIP bridge ended ' + json.dumps(counts))
            text = ('GSM service uplink sapi=0 pd=03 message=05 length=19 '
                    'data=03450404600200815e0581' + setup_number + 'a1150101\n')
            (root / 'error.log').write_text(text)
            args = SimpleNamespace(product='8890', incoming=incoming,
                                   signaling_only=False, restore_idle=False, pcs1900=pcs1900)
            module = ('tools.noki8890_incoming_call_check.verify' if incoming else
                      'tools.noki8890_outgoing_call_check.verify')
            with patch(module) as own_check, patch('tools.run_sip_handset_gate.verify_downlink_lifecycle'):
                verify_success(root, 'state changed to CONFIRMED\nDISCONNECTED [reason=200 (OK)]', args)
                own_check.assert_called_once_with(text, **(
                    {'caller': '5551234', 'configured_gsm900': not pcs1900, 'pcs1900': pcs1900} if incoming else
                    {'number': '1234567', 'configured_gsm900': not pcs1900, 'pcs1900': pcs1900}))
            return json.loads((root / 'sip-result.json').read_text())

    def test_incoming_and_outgoing_use_own_product_contract_with_media(self):
        for incoming in (False, True):
            result = self.check(incoming)
            self.assertEqual(result['media']['uplink'], 150)
            self.assertIn('not native DSP speech', result['scope'])

    def test_pcs_media_selects_own_pcs_call_validator(self):
        for incoming in (False, True):
            self.check(incoming, pcs1900=True)

    def test_pcs_cannot_silently_select_failure_or_other_product(self):
        root = Path(__file__).resolve().parents[1]
        for flags in (['--product', '8210'], ['--product', '8890', '--sip-response', '486'],
                      ['--product', '8890', '--signaling-only']):
            with self.subTest(flags=flags):
                result = subprocess.run([sys.executable, str(root / 'tools/run_sip_handset_gate.py'),
                    '--pjsua', '/nonexistent', '--run-dir', '/nonexistent',
                    '--pcs1900', *flags, '--', '/nonexistent'], capture_output=True, text=True)
                self.assertEqual(result.returncode, 2)
                self.assertIn('--pcs1900 requires 8890 media or call-phase restoration', result.stderr)

    def test_every_media_direction_and_pcm_counter_is_required(self):
        for field in ('uplink', 'downlink', 'pcm_transmitted', 'pcm_received'):
            counts = dict(uplink=150, downlink=140, pcm_transmitted=150, pcm_received=145)
            counts[field] = 99
            with self.subTest(field=field), self.assertRaises(RuntimeError):
                self.check(counts=counts)

    def test_sibling_called_number_cannot_replace_physical_number(self):
        with self.assertRaises(RuntimeError):
            self.check(setup_number='555132f4')


if __name__ == '__main__':
    unittest.main()
