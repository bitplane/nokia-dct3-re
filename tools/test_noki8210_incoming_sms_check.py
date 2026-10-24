import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from PIL import Image
from tools.test_radio_incoming_sms_trace_check import GOOD as RADIO_GOOD, nvram_with_message
from tools.radio_incoming_sms_trace_check import SMS_NVRAM_OFFSET

GOOD = RADIO_GOOD.replace('00f4ffffffff', '0080ffffffff') + '''
GSM service uplink sapi=3 pd=09 message=04 length=2
GSM service uplink sapi=3 pd=09 message=01 length=5 data=8901020240
GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2
LAPDm service Channel Release acknowledged
8210_sms_physical: action=read_2
8210_keypad_decoded: key=19
sim_device: update fid=6f3c record=1 length=176
'''


def read_message():
    storage = bytearray(nvram_with_message())
    storage[SMS_NVRAM_OFFSET] = 1
    return bytes(storage)

try:
    from tools.noki8210_incoming_sms_check import verify, verify_frame
except ModuleNotFoundError:
    from noki8210_incoming_sms_check import verify, verify_frame


class IncomingSmsTest(unittest.TestCase):
    def test_both_completion_orders(self):
        verify(GOOD, read_message())
        lines = GOOD.splitlines()
        info = next(i for i, line in enumerate(lines) if '05324762704221000000' in line)
        complete = next(i for i, line in enumerate(lines) if 'pd=06 message=32' in line)
        lines[info], lines[complete] = lines[complete], lines[info]
        verify('\n'.join(lines), read_message())

    def test_information_cannot_be_missing_or_after_sms_establishment(self):
        lines = GOOD.splitlines()
        info = next(line for line in lines if '05324762704221000000' in line)
        lines.remove(info)
        for bad in ('\n'.join(lines), '\n'.join(lines + [info])):
            with self.assertRaises(ValueError):
                verify(bad, read_message())
        with self.assertRaises(ValueError):
            verify(GOOD + '\n' + info, read_message())

    def test_blank_message_body_is_not_acceptance(self):
        with TemporaryDirectory() as directory:
            path = Path(directory) / 'frame.png'
            Image.new('L', (84, 48)).save(path)
            with self.assertRaisesRegex(ValueError, 'hello message body'):
                verify_frame(path)

    def test_rejects_empty_evidence(self):
        with self.assertRaises(ValueError):
            verify('', b'')

    def test_rejects_fixture_error(self):
        with self.assertRaisesRegex(ValueError, 'fixture error'):
            verify('[LUA ERROR]', b'')


if __name__ == '__main__':
    unittest.main()
