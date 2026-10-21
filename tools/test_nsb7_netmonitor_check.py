"""Physical input and isolated settings-transition acceptance tests."""
import unittest

from tools.nsb7_netmonitor_check import check_keys, check_storage


def key_fixture():
    lines = []
    for second, key, code in [(26, 'Left Softkey / Menu', 0x19), (29, 'Scroll Up', 0x17),
                              (32, 'Left Softkey / Menu', 0x19), (35, 'Keypad 0', 0x0a),
                              (36, 'Keypad 0', 0x0a), (38, 'Left Softkey / Menu', 0x19)]:
        lines += [f'nsb7_netmonitor_input: key={key} state=1 t={second}.000000000',
                  f'nsb7_key_decode: key={code:02x} observer_writes=0',
                  f'nsb7_netmonitor_input: key={key} state=0 t={second}.200000000']
    return '\n'.join(lines + ['nsb7_netmonitor_done: t=46.000000000'])


class NetmonitorTest(unittest.TestCase):
    def test_physical_keys(self):
        check_keys(key_fixture())

    def test_missing_decode_or_key_or_completion(self):
        for token in ['key=Keypad 0', 'key=17 observer_writes=0', 'key=0a observer_writes=0',
                      't=46.', 't=38.000000000']:
            with self.subTest(token=token), self.assertRaises(ValueError):
                check_keys(key_fixture().replace(token, 'incorrect'))

    def test_script_error(self):
        with self.assertRaises(ValueError):
            check_keys(key_fixture() + '\n[LUA ERROR]')

    def test_single_setting_change(self):
        source = bytes([255]) * 0x8000
        changed = bytearray(source)
        changed[0x4f1] = 0x3d
        check_storage(source, changed, changed)

    def test_unrelated_change(self):
        source = bytes([255]) * 0x8000
        changed = bytearray(source)
        changed[0x4f1] = 0x3d
        changed[0x4f2] = 0
        with self.assertRaises(ValueError):
            check_storage(source, changed, changed)

    def test_cold_setting_lost(self):
        source = bytes([255]) * 0x8000
        changed = bytearray(source)
        changed[0x4f1] = 0x3d
        with self.assertRaises(ValueError):
            check_storage(source, changed, source)


if __name__ == '__main__':
    unittest.main()
