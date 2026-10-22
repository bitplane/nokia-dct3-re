import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import AsyncMock,patch
from tools.nsb7_sip_check import check
from tools.test_nsb7_host_outgoing_call_check import fixture
from tools.run_sip_handset_gate import main,verify_success


class SipTest(unittest.TestCase):
    @patch('tools.nsb7_sip_check.check_authentication')
    def test_physical_call_without_probe_only_decisions(self,authentication):
        text=fixture()
        text='\n'.join(row for row in text.splitlines() if
                       'decision id=2' not in row and 'outcome=0 result=rejected' not in row)
        check(text)
        for bad in (text.replace('epoch=1 phase=ended','epoch=2 phase=ended'),
                    text.replace('key=0e observer_writes=0','key=0f observer_writes=0'),
                    text+'\ndsp_hle: speech tick',
                    text+'\nmedia_uplink_sequence=1',
                    text+'\ngsm_call_adapter: decision id=1 outcome=0 result=accepted'):
            with self.subTest(bad=bad),self.assertRaises(ValueError): check(bad)

    def test_cli_excludes_unproved_scenarios(self):
        base=['gate','--pjsua','unused','--run-dir','unused','--product','8290']
        for options in ([],['--incoming'],['--record-media'],['--restore-call'],
                        ['--restore-outgoing'],['--restore-idle'],['--calls','2'],
                        ['--sip-response','486']):
            flags=[] if not options else ['--signaling-only']
            with self.subTest(options=options),patch('sys.argv',base+flags+options+['--','true']), \
                    patch('sys.stderr',new_callable=io.StringIO),self.assertRaises(SystemExit) as error:
                main()
            self.assertEqual(error.exception.code,2)
        with patch('sys.argv',base+['--signaling-only','--','true']), \
                patch('tools.run_sip_handset_gate.run',new_callable=AsyncMock) as run:
            main()
        self.assertEqual(run.call_args.args[0].product,'8290')

    def test_real_sip_result_requires_explicit_no_speech_scope(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            bridge=('digits=5551234\nSIP confirmed status=200 identity=(1, 1)\n'
                    'SIP bridge ended {"uplink":0,"downlink":20}\n')
            (root/'sip-bridge.log').write_text(bridge)
            (root/'error.log').write_text('physical call')
            args=SimpleNamespace(product='8290',incoming=False,signaling_only=True)
            remote='state changed to CONFIRMED\nDISCONNECTED [reason=200 (Normal call clearing)]'
            with patch('tools.nsb7_sip_check.check') as product:
                verify_success(root,remote,args)
                product.assert_called_once_with('physical call')
            result=json.loads((root/'sip-result.json').read_text())
            self.assertFalse(result['speech_tested'])
            self.assertFalse(result['native_dsp_complete'])
            self.assertEqual(result['dialed_digits'],'5551234')
            (root/'sip-bridge.log').write_text(bridge.replace('"uplink":0','"uplink":1'))
            with patch('tools.nsb7_sip_check.check'),self.assertRaises(RuntimeError):
                verify_success(root,remote,args)
