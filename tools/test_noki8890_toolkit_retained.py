import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from PIL import Image
from tools.dct3_toolkit_check import display_text_events, interactive_events, menu_events
from tools.run_noki8890_toolkit_retained import verify, verify_interactive_protocol


class RetainedToolkitTest(unittest.TestCase):
    def interactive_trace(self):
        return '\n'.join(interactive_events('8890')).replace(
            '8890_toolkit_interactive: action=inkey_5',
            '8890_toolkit_interactive: action=inkey_5\n'
            '8890_toolkit_interactive: action=inkey_confirm')

    def test_interactive_requires_own_confirmation(self):
        verify_interactive_protocol(self.interactive_trace())
        with self.assertRaises(ValueError):
            verify_interactive_protocol('\n'.join(interactive_events('8890')))

    def test_interactive_confirmation_must_precede_response(self):
        trace = self.interactive_trace().replace(
            '8890_toolkit_interactive: action=inkey_confirm\n', '')
        trace = trace.replace('8890_toolkit_interactive: action=input_4',
                              '8890_toolkit_interactive: action=inkey_confirm\n'
                              '8890_toolkit_interactive: action=input_4')
        with self.assertRaises(ValueError):
            verify_interactive_protocol(trace)

    def test_menu_preserves_own_confirmation_and_selection(self):
        trace = '\n'.join(menu_events('8890')).replace(
            '8890_toolkit_interactive: action=inkey_5',
            '8890_toolkit_interactive: action=inkey_5\n'
            '8890_toolkit_interactive: action=inkey_confirm')
        verify_interactive_protocol(trace, menu=True)
        for old, new in (('action=inkey_confirm', 'action=absent'),
                         ('d30702020181100101', 'd30702020181100102'),
                         ('action=menu_exit', 'action=absent')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify_interactive_protocol(trace.replace(old, new), menu=True)

    def test_sms_requires_menu_and_network_completion(self):
        with self.assertRaisesRegex(ValueError, 'card-menu'):
            verify_interactive_protocol('', sms=True)
        trace = '\n'.join(menu_events('8890', selection_status='9124')).replace(
            '8890_toolkit_interactive: action=inkey_5',
            '8890_toolkit_interactive: action=inkey_5\n'
            '8890_toolkit_interactive: action=inkey_confirm')
        with self.assertRaisesRegex(ValueError, 'menu selection item=1 accepted'):
            verify_interactive_protocol(trace, menu=True, sms=True)

    def test_protocol_storage_and_both_frames_required(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            (run / 'snap').mkdir()
            (run / 'nvram/nsb6hle').mkdir(parents=True)
            log = '\n'.join(display_text_events('8890'))
            (run / 'error.log').write_text(log)
            storage = bytearray(1611)
            storage[1604:1609] = bytes.fromhex('00f1100001')
            card = run / 'nvram/nsb6hle/sim_card'
            card.write_bytes(storage)
            frame = Image.new('L', (84, 48), 127)
            digest = hashlib.sha256(frame.tobytes()).hexdigest()
            names = ('display.png', 'idle.png')
            for name in names:
                frame.save(run / 'snap' / name)
            with patch('tools.run_noki8890_toolkit_retained.FRAMES', dict.fromkeys(names, digest)), \
                    patch('tools.run_noki8890_toolkit_retained.verify_registration') as registration:
                verify(run)
                registration.assert_called_once_with(log, preserved_location=True)
                storage[1610] = 1
                card.write_bytes(storage)
                with self.assertRaisesRegex(ValueError, 'location'):
                    verify(run)
                storage[1610] = 0
                card.write_bytes(storage)
                for name in names:
                    Image.new('L', (84, 48), 0).save(run / 'snap' / name)
                    with self.assertRaisesRegex(ValueError, 'frame'):
                        verify(run)
                    frame.save(run / 'snap' / name)
                (run / 'error.log').write_text(log.replace('030100', '03022001'))
                with self.assertRaises(ValueError):
                    verify(run)


if __name__ == '__main__':
    unittest.main()
