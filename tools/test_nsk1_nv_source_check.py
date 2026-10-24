import unittest
from tools.nsk1_nv_source_check import check_sources


def fixture():
    bank = bytes(2048)
    lines = [f'nsk1_nv_source: address={0x550+i*33:04x} pointer=0010ce38 length=33 '
             f'caller=0028ca0f data={bytes(33).hex()} t={17+i}.000000000 observer_writes=0'
             for i in range(18)]
    return '\n'.join(lines), bank


class SourceTest(unittest.TestCase):
    def test_all_complete_sources_required(self):
        text, bank = fixture()
        self.assertEqual(check_sources(text, bank), 18)
        for bad in (text.split('\n', 1)[1], text+'\n'+text.splitlines()[0],
                    text.replace('length=33', 'length=32', 1),
                    text.replace('pointer=0010ce38', 'pointer=0010ce39', 1),
                    text.replace('caller=0028ca0f', 'caller=0028ca11', 1),
                    text+'\nnsk1_nv_source: malformed',
                    text.replace('t=18.', 't=16.', 1)):
            with self.subTest(tail=bad[-100:]), self.assertRaises(ValueError):
                check_sources(bad, bank)
        changed = bytearray(bank)
        changed[0x551] = 1
        with self.assertRaises(ValueError):
            check_sources(text, changed)
        with self.assertRaises(ValueError):
            check_sources(text, bank[:-1])


if __name__ == '__main__':
    unittest.main()
