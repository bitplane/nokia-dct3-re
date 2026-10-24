import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from PIL import Image
from tools.test_radio_5210_incoming_call_trace_check import GOOD as NSM5_GOOD

GOOD = NSM5_GOOD.replace('message=08 length=5', 'message=08 length=5 data=8308150101').replace(
    'data=040000001117001a60000056', 'data=040000001117001a60000004').replace(
    'GSM service uplink sapi=0 pd=03 message=07 length=2',
    '8210_incoming_physical: action=Call / Send\n8210_keypad_decoded: key=0e\n'
    'GSM service uplink sapi=0 pd=03 message=07 length=2').replace(
    'GSM service uplink sapi=0 pd=03 message=25 length=5',
    '8210_incoming_physical: action=End\n8210_keypad_decoded: key=0f\n'
    'GSM service uplink sapi=0 pd=03 message=25 length=5')

try:
    from tools.noki8210_incoming_call_check import verify, check_frames
except ModuleNotFoundError:
    from noki8210_incoming_call_check import verify, check_frames


class IncomingCheckTest(unittest.TestCase):
    def test_both_async_completion_orders(self):
        verify(GOOD)
        lines = GOOD.splitlines()
        info = next(i for i, line in enumerate(lines) if '05324762704221000000' in line)
        complete = next(i for i, line in enumerate(lines) if 'pd=06 message=32' in line)
        lines[info], lines[complete] = lines[complete], lines[info]
        verify('\n'.join(lines))

    def test_both_completions_remain_required_before_setup(self):
        for token in ('05324762704221000000', 'pd=06 message=32'):
            lines = GOOD.splitlines()
            selected = next(line for line in lines if token in line)
            lines.remove(selected)
            with self.assertRaises(ValueError):
                verify('\n'.join(lines))
            with self.assertRaises(ValueError):
                verify('\n'.join(lines + [selected]))
            with self.assertRaises(ValueError):
                verify(GOOD + '\n' + selected)

    def test_rejects_blank_caller_presentation(self):
        with TemporaryDirectory() as directory:
            frames = Path(directory)
            Image.new('L', (84, 48)).save(frames / '8210_incoming_ringing.png')
            with self.assertRaisesRegex(ValueError, 'caller 5551234'):
                check_frames(frames)

    def test_requires_registration(self):
        with self.assertRaisesRegex(ValueError, 'registration release'):
            verify('')

    def test_rejects_fixture_error(self):
        with self.assertRaisesRegex(ValueError, 'fixture error'):
            verify('[LUA ERROR]')


if __name__ == '__main__':
    unittest.main()
