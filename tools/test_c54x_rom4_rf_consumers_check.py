import unittest

from tools.c54x_rom4_rf_consumers_check import FAMILIES, DISPATCH_SIGNATURES, SEQUENCES, check, check_program


def fixture():
    groups = {
        'initial_filter': [(0, 0, 2432), (0, 0x100, 8), (4, 0x104, 18040),
                           (4, 0x105, 48352), (9, 0x105, 16), (12, 0x105, 24192)],
        'unfiltered_history': [(1, 0x105, 32), (2, 0x105, 16), (4, 0x105, 24176),
                               (9, 0x105, 24), (12, 0x105, 36288)],
        'scan_filter': [(1, 0, 8008), (1, 0x100, 8), (1, 0x105, 176)],
        'capture_filter': [(2, 0x104, 40), (2, 0x105, 200)],
    }
    rows = [f'rom4_rf_consumer: xpc=00 pc={address + 2:04x} mode={mode:04x} control={control:04x} '
            f'holdoff={int(name == "unfiltered_history")} count={count} first=1.0 last=29.0'
            for name, addresses in FAMILIES.items() for address in addresses
            for mode, control, count in groups[name]]
    return '\n'.join(rows + [
        'rom4_rf_consumer_summary: reads=648032 buckets=64 physical_rate_claim=0 t=30.016666667',
        'rom4_interface_summary: rf_reads=648032'])


class RfConsumerTests(unittest.TestCase):
    def test_complete_census_has_no_physical_format_claim(self):
        report = check(fixture())
        self.assertEqual(report['families']['scan_filter']['control_clear_reads'], 32064)
        self.assertEqual(report['control_clear_reads'], 114144)
        for key in ('physical_rate_claim', 'iq_lane_claim', 'acquisition_claim'):
            self.assertFalse(report[key])

    def test_missing_duplicate_malformed_and_unclassified_rows_fail(self):
        text = fixture()
        first = text.splitlines()[0]
        for changed in (text.replace(first, ''), text + '\n' + first,
                        text.replace(first, first.replace('count=2432', 'count=0')),
                        text.replace(first, first.replace('count=2432', 'count=2433')),
                        text.replace(first, first.replace('pc=324b', 'pc=324c')),
                        text.replace(first, first.replace('xpc=00', 'xpc=01')),
                        text.replace(first, first.replace('first=1.0', 'first=31.0')),
                        text.replace(first, first.replace('count=2432', 'count=oops'))):
            with self.assertRaises(ValueError):
                check(changed)

    def test_count_control_and_duration_changes_fail(self):
        for old, new in [('count=2432', 'count=2433'), ('control=0104', 'control=0105'),
                         ('holdoff=1', 'holdoff=0'), ('holdoff=0', 'holdoff=1'),
                         ('mode=0002', 'mode=0001'),
                         ('buckets=64', 'buckets=63'), ('rf_reads=648032', 'rf_reads=648031'),
                         ('physical_rate_claim=0', 'physical_rate_claim=1'),
                         ('t=30.016666667', 't=8.0')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check(fixture().replace(old, new))

    def test_resident_readers_are_exhaustive_and_operands_pinned(self):
        image = bytearray(0x20000)
        for name, addresses in FAMILIES.items():
            for address in addresses:
                opcode = 0x74d4 if name == 'unfiltered_history' else 0x74d6
                image[2 * address:2 * address + 4] = opcode.to_bytes(2, 'big') + b'\x00\x27'
        for name in DISPATCH_SIGNATURES:
            address, words = SEQUENCES[name]
            image[2 * address:2 * (address + len(words))] = b''.join(word.to_bytes(2, 'big') for word in words)
        self.assertEqual(len(check_program(image)), 16)
        for name in DISPATCH_SIGNATURES:
            address, words = SEQUENCES[name]
            for index in range(len(words)):
                changed = bytearray(image)
                changed[2 * (address + index)] ^= 1
                with self.subTest(name=name, index=index), self.assertRaises(ValueError):
                    check_program(changed)
        for offset in (0x3249 * 2, 0x3249 * 2 + 1, 0x3249 * 2 + 3):
            changed = bytearray(image)
            changed[offset] ^= 1
            with self.assertRaises(ValueError):
                check_program(changed)
        changed = bytearray(image)
        changed[0x100:0x104] = bytes.fromhex('74d60033')
        with self.assertRaisesRegex(ValueError, 'port-33'):
            check_program(changed)
        image[0x100:0x104] = bytes.fromhex('74d60027')
        with self.assertRaisesRegex(ValueError, 'census changed'):
            check_program(image)


if __name__ == '__main__':
    unittest.main()
