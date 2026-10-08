import unittest
from contextlib import redirect_stderr
import io
from pathlib import Path
import tempfile
from unittest.mock import patch
from tools.run_noki8890_host_sms import HOST_DECISIONS, check_output, main
from tools.radio_incoming_host_sms_trace_check import verify
from tools.test_radio_incoming_host_sms_trace_check import GOOD


class HostSmsAcceptanceTest(unittest.TestCase):
    def test_host_decisions_preserve_existing_api(self):
        self.assertEqual(HOST_DECISIONS,
                         {'rp_ack': 'accept', 'rp_error': 'rp_error', 'rp_silence': 'rp_silence'})

    def test_incoming_cannot_apply_outgoing_rejection(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory) / 'untouched'
            with patch('sys.argv', ['runner', str(run), '--direction', 'incoming',
                                    '--outcome', 'rp_error']), redirect_stderr(io.StringIO()), \
                    self.assertRaises(SystemExit) as failure:
                main()
            self.assertEqual(failure.exception.code, 1)
            self.assertFalse(run.exists())

    def test_artifact_errors_are_not_acceptance(self):
        for error in ('[LUA ERROR]', 'Disk quota exceeded', 'Error writing NVRAM file',
                      'Error generating PNG'):
            with self.subTest(error=error), self.assertRaisesRegex(ValueError, 'artifacts'):
                check_output('otherwise successful run\n' + error)
        check_output('Average speed: 500%')

    def test_own_configured_carrier_is_required(self):
        text = GOOD.replace('arfcn=1', 'arfcn=60')
        verify(text, arfcn=60)
        for carrier in (1, 600, 601):
            with self.subTest(carrier=carrier), self.assertRaises(ValueError):
                verify(text.replace('arfcn=60', f'arfcn={carrier}'), arfcn=60)


if __name__ == '__main__':
    unittest.main()
