import hashlib
import io
import struct
import unittest
from unittest.mock import patch
import zipfile

from tools import nse2_package_check as check


def loader_image():
    image = bytearray(0xa1200)
    table = check.TABLE - check.BASE
    struct.pack_into('>II', image, table - 8, 112, 0x10ba14)
    pointers = [0x29bc20] * 27 + [0]
    pointers[25] = 0x2a0f6c
    struct.pack_into('>28I', image, table, *pointers)
    descriptor = pointers[25] - check.BASE
    struct.pack_into('>6H', image, descriptor, 0xf00, 0, 186, 0x700, 180, 0)
    payload = descriptor + 12
    image[payload + 24:payload + 30] = bytes.fromhex('76f808030004')
    for relative, target in ((0x44, 0x8023), (0x7c, 0x8023), (0xf4, 0x802b)):
        struct.pack_into('>HH', image, payload + relative, 0xf074, target)
    return image


def package(members):
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, 'w') as archive:
        for name, data in members:
            archive.writestr(name, data)
    return stream.getvalue()


class Nse2PackageTest(unittest.TestCase):
    def test_unknown_wrapper_is_rejected_before_extraction(self):
        with self.assertRaisesRegex(ValueError, 'hash-qualified'):
            check.unpack_package(package([('other', b'data')]))

    def test_member_names_sizes_and_hashes_are_all_required(self):
        members = [('mcu', b'abc'), ('ppm', b'1234')]
        expected = {name: (len(data), hashlib.sha256(data).hexdigest())
                    for name, data in members}
        variants = [members, [('other', b'abc'), members[1]],
                    [('mcu', b'ab'), members[1]], [('mcu', b'abd'), members[1]]]
        with patch.dict(check.MEMBERS, expected, clear=True):
            for index, variant in enumerate(variants):
                source = package(variant)
                with self.subTest(index=index), patch.dict(
                        check.ARCHIVES, {check.sha256(source): 'test'}, clear=True):
                    if index == 0:
                        contents, report = check.unpack_package(source)
                        self.assertEqual(contents, dict(members))
                        self.assertEqual(report['container'], 'test')
                    else:
                        with self.assertRaises(ValueError):
                            check.unpack_package(source)

    def test_own_catalogue_keeps_populated_slots_11_and_26(self):
        result = check.loader_contract(loader_image())
        self.assertEqual(result['null_slots'], [27])
        self.assertEqual(len(result['entries']), 27)
        self.assertEqual(result['verifier_identity_store'], 4)
        self.assertEqual(result['resident_call_targets'], ['0x8023', '0x8023', '0x802b'])
        self.assertFalse(result['fitted_mask_or_execution_proved'])

    def test_older_catalogue_shape_is_not_inherited(self):
        for slot in (11, 26):
            image = loader_image()
            struct.pack_into('>I', image, check.TABLE - check.BASE + slot * 4, 0)
            with self.subTest(slot=slot), self.assertRaisesRegex(ValueError, 'null slots'):
                check.loader_contract(image)

    def test_pointer_and_payload_bounds(self):
        for pointer in (check.BASE - 2, check.BASE + 1, check.BASE + 0xa11fe):
            image = loader_image()
            struct.pack_into('>I', image, check.TABLE - check.BASE, pointer)
            with self.subTest(pointer=pointer), self.assertRaisesRegex(ValueError, 'outside MCU'):
                check.loader_contract(image)
        image = loader_image()
        struct.pack_into('>6H', image, 0x9bc20, 0xffff, 0, 2, 0, 0, 0)
        with self.assertRaisesRegex(ValueError, 'extent'):
            check.loader_contract(image)

    def test_verifier_identity_and_calls_are_not_assumed(self):
        payload = 0xa0f6c + 12
        for offset in (24, 29, 0x44, 0x47, 0x7f, 0xf7):
            image = loader_image()
            image[payload + offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                check.loader_contract(image)

    def test_truncated_catalogue_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'truncated'):
            check.loader_contract(b'')


if __name__ == '__main__':
    unittest.main()
