import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools import nsb3_idle_profile as profile


class Nsb3IdleProfileTest(unittest.TestCase):
    def test_empty_text_changes_only_own_first_byte(self):
        original = b'\xff' * 8192
        result = profile.empty_startup_text(original)
        self.assertEqual([i for i, (a, b) in enumerate(zip(original, result)) if a != b], [0x5ec])
        self.assertEqual(result[0x5ec], 0)
        self.assertEqual(result[0x4a1], 255)

    def test_non_erased_text_selector_and_wrong_extent_are_rejected(self):
        for offset in (0x5ec, 0x600, 0x4a1):
            value = bytearray(b'\xff' * 8192)
            value[offset] = 0
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                profile.empty_startup_text(value)
        with self.assertRaises(ValueError):
            profile.empty_startup_text(b'\xff' * 8191)

    def test_executable_identity_and_card_are_not_borrowed(self):
        with self.assertRaises(ValueError):
            profile.contract(b'other firmware')
        with self.assertRaises(ValueError):
            profile.check_card(b'other card')

    def fixture(self, root):
        source, baseline, run, cold = (root / name for name in ('source', 'baseline', 'run', 'cold'))
        for directory in (source, baseline, run, cold):
            (directory / 'cfg').mkdir(parents=True)
            (directory / 'cfg/nsb3r4t.cfg').write_text('<mameconfig><system name="nsb3r4t"><input><keyboard enabled="1"/></input></system></mameconfig>')
            (directory / 'nvram/nsb3r4t').mkdir(parents=True)
            (directory / 'nvram/nsb3r4t/flash').write_bytes(b'own test image')
            card = profile.make_current_profile(True)
            (directory / 'nvram/nsb3r4t/sim_card').write_bytes(card)
            (directory / 'initial_sim_card.bin').write_bytes(card)
        original = b'\xff' * 8192
        (source / 'nvram/nsb3r4t/eeprom').write_bytes(original)
        initial = profile.empty_startup_text(original)
        (baseline / 'initial_eeprom.bin').write_bytes(initial)
        (baseline / 'nvram/nsb3r4t/eeprom').write_bytes(initial)
        final = bytearray(initial)
        final[0x4a1] = 0x41
        for directory in (run, cold):
            (directory / 'nvram/nsb3r4t/eeprom').write_bytes(final)
        (baseline / 'startup_text_fixture.json').write_text(json.dumps({
            'record': '0x0745', 'changed_offsets': ['0x5ec'], 'empty_first_byte': True,
            'factory_provisioning_claim': False, 'diagnostic_selector_changed_by_fixture': False}))
        for directory, previous in ((baseline, source), (run, baseline), (cold, run)):
            (directory / 'cold_source.json').write_text(json.dumps({
                'source': str(previous), 'copied_guest_ram_or_save_state': False,
                'storage_sha256': {name: hashlib.sha256((previous / 'nvram/nsb3r4t' / name).read_bytes()).hexdigest()
                                   for name in ('flash', 'eeprom', 'sim_card')}}))
            text = '\n'.join(f'nsb3_pin_key: correct=1 key=Keypad {d} down=1' for d in '1234')
            if directory == run:
                text += '\n' + '\n'.join(f'nsb3_fieldtest_key: key={key} down={state} t=40.0 physical_only=1'
                                         for key in profile.KEYS for state in ('1', '0'))
            (directory / 'error.log').write_text(text)
        return source, baseline, run, cold

    @patch.object(profile, 'contract')
    @patch.object(profile, 'check_security_menu')
    def test_cold_gate_rejects_state_keys_and_frame_drift(self, *_):
        with tempfile.TemporaryDirectory() as name:
            source, baseline, run, cold = self.fixture(Path(name))
            def image(path):
                return profile.TEST_EDITOR_FRAME if path.name == 'nsb3_fieldtest_02.png' else profile.IDLE_FRAME
            with patch.object(profile, 'frame', side_effect=image):
                profile.check(source, baseline, run, cold)
                path = cold / 'nvram/nsb3r4t/eeprom'
                good = path.read_bytes()
                for offset in (0x4a1, 0x5ec, 0x123):
                    bad = bytearray(good)
                    bad[offset] ^= 1
                    path.write_bytes(bad)
                    with self.subTest(offset=offset), self.assertRaises(ValueError):
                        profile.check(source, baseline, run, cold)
                path.write_bytes(good)
                log = run / 'error.log'
                text = log.read_text()
                log.write_text(text.replace('key=End', 'key=Send'))
                with self.assertRaises(ValueError):
                    profile.check(source, baseline, run, cold)
                log.write_text(text)
                config = cold / 'cfg/nsb3r4t.cfg'
                original_config = config.read_text()
                config.write_text(original_config.replace('enabled="1"', 'enabled="0"'))
                with self.assertRaises(ValueError):
                    profile.check(source, baseline, run, cold)
                config.write_text(original_config)
            with patch.object(profile, 'frame', return_value='wrong pixels'), self.assertRaises(ValueError):
                profile.check(source, baseline, run, cold)


if __name__ == '__main__':
    unittest.main()
