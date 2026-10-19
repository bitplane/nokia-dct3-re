import unittest

from tools.c54x_rom4_call_observe_check import inspect


def fixture():
    lines = ['rom4_call_codec: encoder=0 decoder=0',
             'rom4_call_end: machine=nse1astd illegal=0 mode=0000 flags=0024']
    for kind in ('host_write', 'payload_read', 'cobba_write'):
        lines += [f'rom4_call_count: kind={kind} count=1 cap=512',
                  f'rom4_call_bus: kind={kind} address=0000002d value=00000626 mask=0000ffff t=8.0']
    return '\n'.join(lines)


class ObservationTest(unittest.TestCase):
    def test_complete_observation_does_not_claim_call_or_speech(self):
        result = inspect(fixture(), '', 'nse1astd')
        self.assertTrue(result['observation_complete'])
        self.assertFalse(result['connected_call_claim'])
        self.assertFalse(result['native_speech_claim'])

    def test_rejects_incomplete_faulted_or_truncated_capture(self):
        original = fixture()
        for text in ('', original + '\n' + original,
                     original.replace('illegal=0', 'illegal=1'),
                     original.replace('count=1', 'count=513'),
                     original.replace('count=1', 'count=0'),
                     original.replace('mask=0000ffff', 'mask=00000000'),
                     original.replace('t=8.0', 't=6.0')):
            with self.subTest(text=text):
                with self.assertRaises(ValueError):
                    inspect(text, '', 'nse1astd')

    def test_rejects_wrong_machine_and_observer_error(self):
        with self.assertRaises(ValueError):
            inspect(fixture(), '', 'nse1pipe')
        with self.assertRaises(ValueError):
            inspect(fixture(), 'Lua error: broken observer', 'nse1astd')
