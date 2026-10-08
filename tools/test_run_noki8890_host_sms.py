import unittest
from tools.run_noki8890_host_sms import check_output
from tools.radio_incoming_host_sms_trace_check import verify
from tools.test_radio_incoming_host_sms_trace_check import GOOD


class HostSmsAcceptanceTest(unittest.TestCase):
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
