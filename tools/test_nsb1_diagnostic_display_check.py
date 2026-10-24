from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch, call

from tools import nsb1_diagnostic_display_check as checker


class Nsb1DiagnosticDisplayTest(unittest.TestCase):
    def test_exact_one_byte_physical_selection(self):
        original = bytes(b'\xff'*8192)
        selected = checker.selected_eeprom(original)
        self.assertEqual([i for i, (a, b) in enumerate(zip(original, selected)) if a != b], [0x4a1])
        self.assertEqual(selected[0x4a1], 0x41)
        self.assertEqual(selected[0x5ec:0x60b], b'\xff'*31)
        for bad in (original[:-1], selected, original[:0x5ec]+b'\0'+original[0x5ed:]):
            with self.assertRaises(ValueError):
                checker.selected_eeprom(bad)

    def test_every_physical_contact_and_release_is_required(self):
        text = '\n'.join(f'nsb1_diagnostic_key: key={key} down={state} t=42.0 physical_only=1'
                         for key in checker.KEYS for state in ('1', '0'))
        checker.check_actions(text)
        for bad in (text.replace('key=zero', 'key=one'), text.replace('down=0', 'down=1'),
                    text.replace('physical_only=1', 'physical_only=0'), text+'\n'+text):
            with self.assertRaises(ValueError):
                checker.check_actions(bad)

    def test_unrelated_card_bytes_cannot_change(self):
        card = checker.make_current_profile(True)
        checker.card_check(card)
        for bad in (card[:-1], bytes([card[0]^1])+card[1:]):
            with self.assertRaises(ValueError):
                checker.card_check(bad)

    def test_other_roms_cannot_supply_the_descriptor_contract(self):
        with self.assertRaisesRegex(ValueError, 'own NSB-1'):
            checker.contract(b'foreign image')

    def test_whole_eeprom_and_both_cold_frame_phases_are_required(self):
        with tempfile.TemporaryDirectory() as temporary:
            source, run, cold = (Path(temporary) / name for name in ('source', 'run', 'cold'))
            original = b'\xff'*8192
            expected = checker.selected_eeprom(original)
            card = checker.make_current_profile(True)
            for directory in (source, run, cold):
                storage = directory / 'nvram/nsb1r4t'
                storage.mkdir(parents=True)
                (storage / 'flash').write_bytes(b'own')
                (storage / 'sim_card').write_bytes(card)
                (storage / 'eeprom').write_bytes(original if directory == source else expected)
                (directory / 'cfg').mkdir()
                (directory / 'cfg/nsb1r4t.cfg').write_text(
                    '<mameconfig><system name="nsb1r4t"><input/></system></mameconfig>')
                (directory / 'exit_status.json').write_text('{"returncode":0}')
            with patch.object(checker, 'contract'), patch.object(checker, 'check_cold_link'), \
                    patch.object(checker, 'check_card_run', return_value=('', card)), \
                    patch.object(checker, 'check_correct'), patch.object(checker, 'check_actions'), \
                    patch.object(checker, 'check_frame') as frame:
                checker.check(source, run, cold)
                frame.assert_has_calls([
                    call(cold / 'snap/nsb1_idle_25.png', checker.EARLY_IDLE),
                    call(cold / 'snap/nsb1_idle_36.png', checker.IDLE),
                    call(cold / 'snap/nsb1_idle_42.png', checker.IDLE)])
                for directory, offset in ((run, 0x5ec), (cold, 0x100)):
                    path = directory / 'nvram/nsb1r4t/eeprom'
                    damaged = bytearray(expected)
                    damaged[offset] ^= 1
                    path.write_bytes(damaged)
                    with self.assertRaisesRegex(ValueError, 'single physical'):
                        checker.check(source, run, cold)
                    path.write_bytes(expected)

    def test_config_comparison_is_input_semantic_not_video_formatting(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'input.cfg'
            path.write_text('<mameconfig><system name="nsb1r4t"><input><port tag=":A" value="0"/></input></system></mameconfig>')
            original = checker.input_signature(path)
            path.write_text('<mameconfig>\n<system name="nsb1r4t"><input>\n<port value="0" tag=":A"/>\n</input><video/></system></mameconfig>')
            self.assertEqual(original, checker.input_signature(path))
            path.write_text('<mameconfig><system name="nsb1r4t"><input><port tag=":A" value="1"/></input></system></mameconfig>')
            self.assertNotEqual(original, checker.input_signature(path))
            path.write_text('<mameconfig><system name="foreign"><input/></system></mameconfig>')
            with self.assertRaises(ValueError):
                checker.input_signature(path)


if __name__ == '__main__':
    unittest.main()
