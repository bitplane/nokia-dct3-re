import unittest

from tools.nse2_nv_source_check import CODE, check


def fixture():
    image, bank = bytearray(0x100000), bytearray(8192)
    for address, encoded in CODE.items():
        raw = bytes.fromhex(encoded)
        image[address-0x200000:address-0x200000+len(raw)] = raw
    lines = []
    for index, address in enumerate(range(0x6d0, 0x922, 33)):
        raw = bytes(15) + b'\x01' + bytes(17)
        bank[address:address+33] = raw
        lines += [f'nse2_nv_encode: source=0010d68c first=00 output=0010d608 caller=002459a3 t={index*2+1}.0 observer_writes=0',
                  f'nse2_nv_source: address={address:04x} pointer=0010d618 length=33 caller=00290e17 data={raw.hex()} t={index*2+2}.0 observer_writes=0']
    return '\n'.join(lines), bytes(image), bytes(bank)


class SourceTests(unittest.TestCase):
    def test_complete_sequence(self):
        self.assertEqual(check(*fixture())['records'], 18)

    def test_every_static_contract_is_required(self):
        text, original, bank = fixture()
        for address in CODE:
            changed = bytearray(original)
            changed[address-0x200000] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                check(text, bytes(changed), bank)

    def test_missing_changed_or_reordered_evidence(self):
        text, image, bank = fixture()
        for changed in (text.replace('first=00', 'first=01', 1),
                        text.replace('length=33', 'length=32', 1),
                        text.replace('pointer=0010d618', 'pointer=0010d619', 1),
                        text.replace('t=1.0', 't=99.0', 1),
                        '\n'.join(text.splitlines()[2:]),
                        text + '\nnse2_nv_source: malformed'):
            with self.subTest(changed=changed[-60:]), self.assertRaises(ValueError):
                check(changed, image, bank)

    def test_saved_bytes_must_match_source(self):
        text, image, bank = fixture()
        changed = bytearray(bank)
        changed[0x6df] ^= 1
        with self.assertRaises(ValueError):
            check(text, image, bytes(changed))


if __name__ == '__main__':
    unittest.main()
