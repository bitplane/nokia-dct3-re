import hashlib
import unittest

from tools.nsb3_menu_check import check_fixture, check_trace


class Nsb3MenuTest(unittest.TestCase):
    def test_fixture_is_explicit_and_bounded(self):
        seed = bytes(8192)
        metadata = dict(fresh_phone_retry_initialized=True, insert_sim=True,
                        security_checksum_included=True,
                        changed_offsets=['0x11c', '0x11d', '0x11e', '0x11f', '0x1fe', '0x1ff', '0x4ac'],
                        initial_eeprom_sha256=hashlib.sha256(seed).hexdigest())
        check_fixture(metadata, seed)
        for key in ('fresh_phone_retry_initialized', 'insert_sim', 'security_checksum_included'):
            with self.assertRaises(ValueError):
                check_fixture(dict(metadata, **{key: False}), seed)
        with self.assertRaises(ValueError):
            check_fixture(dict(metadata, changed_offsets=metadata['changed_offsets'] + ['0x4a8']), seed)

    def test_matched_branch_and_physical_menu_required(self):
        lines = ['nsb3_phonecode_matched_branch: retry=00 observer_writes=0',
                 'nsb3_phonecode_menu: down=1 physical_only=1',
                 'nsb3_phonecode_menu: down=0 physical_only=1']
        lines += [f'name=Keypad {digit} down={down}' for digit in '12345' for down in (1, 0)]
        check_trace('\n'.join(lines))
        for index in range(len(lines)):
            with self.assertRaises(ValueError):
                check_trace('\n'.join(lines[:index] + lines[index + 1:]))


if __name__ == '__main__':
    unittest.main()
