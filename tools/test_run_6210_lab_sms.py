import unittest

from tools.run_6210_lab_sms import check, information

BOOT = 'native_dsp_reset_line: x\n' * 3 + 'npe3_session_released: registered=1 \n' * 2


def up(sapi, layer3):
    return f'npe3_session_uplink: result=6 sapi={sapi} layer3={layer3} start_frame=1 physical_claim=0\n'


def down(kind, octets):
    return f'npe3_session_downlink: kind={kind} start_frame=1 octets={octets} raw_adc_only=1\n'


INCOMING = (BOOT + down('information_16', '0f0053' + '0901210140' + '00' * 15) + up(3, '8904') +
            up(3, '8901020240') + down('information_17', '0f4409090400'))
OUTGOING = (BOOT + up(3, '390118000100069121436587090d11010781551532f40000ff0141') +
            down('information_17', '0f4009b904') + down('information_18', '0f4215b901020301') + up(3, '3904'))


class LabSmsCheckTest(unittest.TestCase):
    def test_information_strips_lapdm_header(self):
        self.assertEqual(information('0f4009b9042b2b'), 'b904')

    def test_accepts_complete_exchanges(self):
        check(INCOMING, 'incoming')
        check(OUTGOING, 'outgoing')

    def test_rejects_missing_acknowledgements(self):
        with self.assertRaises(ValueError):
            check(INCOMING.replace(up(3, '8901020240'), ''), 'incoming')
        with self.assertRaises(ValueError):
            check(OUTGOING.replace(up(3, '3904'), ''), 'outgoing')

    def test_rejects_a_dsp_reset_after_boot(self):
        with self.assertRaises(ValueError):
            check(INCOMING + 'native_dsp_reset_line: y\n', 'incoming')


if __name__ == '__main__':
    unittest.main()
