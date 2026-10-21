import unittest
from tools.nsb7_menu_check import check_trace
from tools.test_nsb7_security_check import trace as security_trace


def trace():
    rows = [security_trace()]
    for key, decoded in [('Left Softkey / Menu', 0x19), ('Scroll Down', 0x18), ('Left Softkey / Menu', 0x19)]:
        rows += [f'nsb7_menu_input: key={key} state=1 t=26',
                 f'nsb7_key_decode: key={decoded:02x} observer_writes=0',
                 f'nsb7_menu_input: key={key} state=0 t=27']
    return '\n'.join(rows)


class MenuCheckTests(unittest.TestCase):
    def test_complete(self):
        check_trace(trace())

    def test_missing_contacts_or_decode(self):
        for line in trace().splitlines()[-9:]:
            with self.subTest(line=line), self.assertRaises(ValueError):
                text = trace()
                index = text.rfind(line)
                check_trace(text[:index] + text[index+len(line):])

    def test_security_remains_required(self):
        with self.assertRaises(ValueError):
            check_trace(trace().replace('match=1', 'match=0'))


if __name__ == '__main__':
    unittest.main()
