import unittest
from tools.nsk1_nv_source_check import CODE, check_sources, check_empty_encoding, check_lifecycle


def fixture():
    bank = bytes(2048)
    lines = [f'nsk1_nv_source: address={0x550+i*33:04x} pointer=0010ce38 length=33 '
             f'caller=0028ca0f data={bytes(33).hex()} t={17+i}.000000000 observer_writes=0'
             for i in range(18)]
    return '\n'.join(lines), bank


class SourceTest(unittest.TestCase):
    def test_shared_update_lifecycle_is_not_card_specific(self):
        lines = [
            'nsk1_nv_selector: input=05e7 stage=01 flags=20 caller=0028db52 t=16.000000000 observer_writes=0',
            'nsk1_nv_clear: caller=00270ead r0=00000000 r4=002a968c r5=0010fd0a r6=000005e7 sp=0010cec0 t=16.500000000 observer_writes=0']
        index = 0
        for kind,count in ((3,8),(4,5),(5,5)):
            for slot in range(count):
                lines.append(f'nsk1_nv_lifecycle: class={kind} index={slot} digit=0010ce88 digit_first=00 '
                    f'other=0010ce88 other_first=00 caller=0024b411 sp=0010ce88 t={17+index}.000000000 observer_writes=0')
                index += 1
        text = '\n'.join(lines)
        self.assertEqual(check_lifecycle(text),18)
        for bad in (text.replace('stage=01','stage=00'), text.replace('flags=20','flags=00'),
                    text.replace('digit_first=00','digit_first=01',1),
                    text.replace('index=7','index=8',1),
                    text+'\nnsk1_nv_lifecycle: malformed',
                    text+'\n'+lines[1], text.replace('t=16.500','t=18.500'),
                    text.replace('t=18.000','t=16.000')):
            with self.assertRaises(ValueError):
                check_lifecycle(bad)

    def test_empty_encoding_explains_only_inactive_payload_changes(self):
        text, before = fixture()
        after = bytearray(before)
        for address in range(0x550,0x7a2,33):
            for offset in (1,2,3,5,6,7):
                after[address+offset] = 1
        image = bytearray(0x85aaa)
        for offset, encoded in CODE.items():
            image[offset:offset+len(bytes.fromhex(encoded))] = bytes.fromhex(encoded)
        encoding = '\n'.join(
            f'nsk1_nv_encode: source=0010ce88 first=00 output=0010ce28 lr=00249c51 t={16+i}.500000000 observer_writes=0'
            for i in range(18))
        text += '\n'+encoding
        self.assertEqual(check_empty_encoding(text,image,before,after),108)
        for bad in (text.replace('first=00','first=01',1),
                    text.replace('output=0010ce28','output=0010ce29',1),
                    text.replace('t=16.500000000','t=17.500000000',1),
                    text+'\nnsk1_nv_encode: malformed'):
            with self.assertRaises(ValueError):
                check_empty_encoding(bad,image,before,after)
        changed = bytearray(after)
        changed[0x550+16] = 1
        with self.assertRaises(ValueError):
            check_empty_encoding(text,image,before,changed)
        changed = bytearray(after)
        changed[0x550] = 1
        with self.assertRaises(ValueError):
            check_empty_encoding(text,image,before,changed)
        for offset in CODE:
            changed = bytearray(image)
            changed[offset] ^= 1
            with self.assertRaises(ValueError):
                check_empty_encoding(text,changed,before,after)

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
