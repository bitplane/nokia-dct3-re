import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from PIL import Image
from tools.dct3_toolkit_check import display_text_events, interactive_events, menu_events
from tools.run_noki8890_toolkit_retained import main, verify, verify_interactive_protocol, verify_menu_state
from tools.test_noki8850_toolkit_menu_state_check import complete as menu_state_trace


class RetainedToolkitTest(unittest.TestCase):
    def test_menu_state_rejects_network_scenarios(self):
        for option in ('--sms', '--call', '--call-decline'):
            with self.subTest(option=option), patch('sys.argv',
                    ['toolkit', 'unused', '--menu-state', option]), \
                    patch('tools.run_noki8890_toolkit_retained.subprocess.run') as run, \
                    patch('sys.stderr'), self.assertRaises(SystemExit) as result:
                main()
            self.assertEqual(result.exception.code, 2)
            run.assert_not_called()

    def test_menu_state_preserves_own_confirmation(self):
        text = menu_state_trace().replace('8850', '8890').replace(
            '8890_toolkit_interactive: action=inkey_5',
            '8890_toolkit_interactive: action=inkey_5\n'
            '8890_toolkit_interactive: action=inkey_confirm')
        verify_menu_state(text)
        verify_menu_state(text.replace('terminal-response data=8103022200020282810301000d020435',
                                      'terminal-response data=[:sim_card] 81[:sim_card] 03[:sim_card] 02[:sim_card] 22[:sim_card] 00[:sim_card] 02[:sim_card] 02[:sim_card] 82[:sim_card] 81[:sim_card] 03[:sim_card] 01[:sim_card] 00[:sim_card] 0d[:sim_card] 02[:sim_card] 04[:sim_card] 35'))
        for original, replacement in (
            ('action=inkey_confirm', 'action=missing'),
            ('event=restored pc=1c', 'event=restored pc=20'),
            ('action=menu_select phase=restored', 'action=ignored'),
        ):
            with self.subTest(original=original), self.assertRaises(ValueError):
                verify_menu_state(text.replace(original, replacement))

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

    def test_invalid_call_consent_combinations(self):
        for arguments in ({'decline': True}, {'call': True},
                          {'menu': True, 'sms': True, 'call': True}):
            with self.subTest(arguments=arguments), self.assertRaises(ValueError):
                verify_interactive_protocol('', **arguments)

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
