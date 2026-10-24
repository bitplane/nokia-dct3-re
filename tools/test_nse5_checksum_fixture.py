import hashlib
import unittest
from unittest.mock import patch

from tools import nse5_checksum_fixture as checker


def image():
    result = bytearray(b'\xff' * 0x400000)
    sector = bytearray(b'\xff' * 0x2000)
    sector[6:12] = b'EEPROM'
    sector[0x18:0x1a] = b'\x00\x01'
    cache = bytearray(0x898)
    cache[:25] = bytes([255]*24 + [248])
    cache[0x3c:0x40] = bytes.fromhex('000018ed')
    sector[0x20:0x26] = bytes.fromhex('000008980000')
    sector[0x26:0x8be] = cache
    sector[0x8be:0x8c4] = bytes.fromhex('000004e80100')
    sector[0x8c4:0xdac] = bytes(0x4e8)
    sector[0xdac:0xdb4] = bytes.fromhex('1000003c000018ed')
    result[checker.SECTOR:checker.SECTOR+0x2000] = sector
    return bytes(result)


def trace():
    lines = ['nse5_checksum_compare: computed=000018e0 stored=000018e0 observer_writes=0',
             'nse5_checksum_result: accepted=00000001 observer_writes=0'] * 2
    for name, pc, count in (
            ('service_init', '003bc2a8', 2), ('identity_check', '003af540', 2),
            ('identity_invalid', '003af64e', 2), ('context_result', '003ae594', 1),
            ('selftest_reply', '0030e7a6', 1), ('subsystem_release', '0049fddc', 1)):
        lines += [f'nse5_full_boundary: name={name} pc={pc} index={i} t=1 observer_writes=0'
                  for i in range(1, count+1)]
    lines.append('nse5_full_boundary_state: name=subsystem_release index=1 flags=c8 r0=00000002')
    return '\n'.join(lines)


class Nse5ChecksumFixtureTest(unittest.TestCase):
    def test_single_latest_checksum_byte_changes_no_other_logical_content(self):
        original = image()
        with patch.dict(checker.IMAGES, {'501d-full': ('synthetic', hashlib.sha256(original).hexdigest())}):
            derived, metadata = checker.derive(original)
            self.assertEqual([i for i, (a, b) in enumerate(zip(original, derived)) if a != b], [0x3fcdb3])
            self.assertEqual(metadata['changed_logical_offsets'], ['3f'])
            self.assertFalse(metadata['authentic_provisioning'])
            self.assertFalse(metadata['firmware_code_changed'])
            changed = bytearray(original)
            changed[0] ^= 1
            with self.assertRaisesRegex(ValueError, 'unchanged acquired'):
                checker.derive(changed)

    def test_changed_checksum_premise_is_not_repaired_speculatively(self):
        original = bytearray(image())
        original[0x3fcdb3] = 0xe0
        with patch.dict(checker.IMAGES, {'501d-full': ('synthetic', hashlib.sha256(original).hexdigest())}):
            with self.assertRaisesRegex(ValueError, 'checksum premise'):
                checker.derive(original)

    def test_release_is_distinct_from_identity_or_graphical_boot_acceptance(self):
        result = checker.check_trace(trace())
        self.assertTrue(result['checksum_accepted'])
        self.assertTrue(result['subsystem_release_observed'])
        self.assertEqual(result['identity_invalid_entries'], 2)
        self.assertFalse(result['authentic_provisioning'])
        self.assertFalse(result['graphical_boot_verified'])
        for bad in (trace().replace('stored=000018e0', 'stored=000018ed'),
                    trace().replace('accepted=00000001', 'accepted=00000000'),
                    trace().replace('name=identity_invalid', 'name=unclassified'),
                    trace().replace('flags=c8', 'flags=88'),
                    trace() + '\nnse5_full_failed_item: item=12 value=12',
                    trace() + '\nnse5_checksum_compare: malformed',
                    trace() + '\nunhandled opcode'):
            with self.subTest(tail=bad[-100:]), self.assertRaises(ValueError):
                checker.check_trace(bad)


if __name__ == '__main__':
    unittest.main()
