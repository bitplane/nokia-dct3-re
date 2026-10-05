from pathlib import Path
import tempfile
import unittest
from PIL import Image

from tools.noki8850_sms_check import verify, verify_frame
from tools.radio_incoming_sms_trace_check import SMS_NVRAM_OFFSET
from tools.test_radio_incoming_sms_trace_check import GOOD, nvram_with_message

UI = '8850_sms_physical: action=read_4\n8850_keypad_decoded key=19\n'
COLD = 'sim_device: header cla=a0 ins=b2 p1=01 p2=04 p3=b0 selected=6f3c\n' + UI
FRESH = GOOD.replace('00f4ffffffff', '0080ffffffff') + '''
GSM service uplink sapi=3 pd=09 message=04 length=2
GSM service uplink sapi=3 pd=09 message=01 length=5 data=8901020240
GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2
LAPDm service Channel Release acknowledged nr=3
PCH no-identity fill
''' + UI + 'sim_device: update fid=6f3c record=1 length=176\n'


def storage():
    result = bytearray(nvram_with_message())
    result[SMS_NVRAM_OFFSET] = 1
    return bytes(result)


class Nokia8850SmsCheckTest(unittest.TestCase):
    def test_fresh_and_cold(self):
        verify(FRESH, storage())
        verify(COLD, storage(), preserved=True)

    def test_missing_rp_ack(self):
        with self.assertRaisesRegex(ValueError, 'RP-ACK'):
            verify(FRESH.replace('data=8901020240', 'data=8901020440'), storage())

    def test_cold_redelivery(self):
        with self.assertRaisesRegex(ValueError, 'redelivered'):
            verify(COLD + 'GSM service downlink pd=09 message=01\n', storage(), True)

    def test_wrong_storage(self):
        with self.assertRaisesRegex(ValueError, 'exact read'):
            verify(COLD, nvram_with_message(), True)

    def test_wrong_key(self):
        with self.assertRaisesRegex(ValueError, 'Read decode'):
            verify(COLD.replace('key=19', 'key=11'), storage(), True)

    def test_blank_frame(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'blank.png'
            Image.new('L', (84, 48), 255).save(path)
            with self.assertRaisesRegex(ValueError, 'hello pixels'):
                verify_frame(path)


if __name__ == '__main__':
    unittest.main()
