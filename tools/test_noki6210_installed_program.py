import unittest
from pathlib import Path
from tools.noki6210_installed_program import assess, catalogue, verify_catalogue_reads, BLOCKS


class InstalledProgramTest(unittest.TestCase):
    def test_rejects_other_image(self):
        with self.assertRaisesRegex(ValueError, 'acquired NPE-3'):
            assess(b'')

    def test_source_and_capture_contract(self):
        path = Path(__file__).resolve().parents[1] / 'roms/noki6210/6210_556c.fls'
        if not path.exists():
            self.skipTest('own acquired image unavailable')
        image = path.read_bytes()
        captures = {name: image[offset:offset + count * 2]
                    for name, (offset, _, count) in BLOCKS.items()}
        result = assess(image, captures)
        self.assertEqual(result['pairs_examined'], 211)
        self.assertEqual(result['return_nop_pairs'], 143)
        self.assertEqual(len(result['branch_pairs']), 64)
        self.assertEqual(result['unowned_branch_pairs'], 58)
        self.assertEqual(len(result['other_pairs']), 4)
        records = result['catalogue_candidate']
        self.assertEqual(len(records), 28)
        self.assertEqual(records[0]['source_offset'], BLOCKS['bootstrap'][0])
        self.assertEqual(records[15]['source_offset'], BLOCKS['fragment'][0])
        self.assertEqual(records[20]['source_offset'], BLOCKS['loader2'][0])
        self.assertEqual(records[27]['words'], 223)
        lines = []
        def row(name, address, value, pc, r0=0, r1=0):
            return (f'npe3_catalogue_{name}: address={address:08x} data={value:08x} '
                    f'mask=ffffffff pc={pc:08x} r0={r0:08x} r1={r1:08x} '
                    'r2=00000000 t=0.100000000')
        for i, record in enumerate(records):
            lines.append(row('flash', 0x515994 + i * 4, record['descriptor'],
                             0x200194, 0x515994 + i * 4, 0x170004 + i * 4))
        for address in (0x5159a0, 0x5159c0, 0x5159e0, 0x515a00):
            lines.append(row('flash', address, 0x220000, 0x426c5c).replace(
                'mask=ffffffff', 'mask=ffff0000'))
        for index, pc in ((27, 0x426be8), (0, 0x426e1a), (20, 0x427396),
                          (1, 0x427396), (21, 0x420356)):
            lines.append(row('ram', 0x170004 + index * 4, records[index]['descriptor'], pc))
        lines += ['npe3_catalogue_flash: total=32', 'npe3_catalogue_ram: total=5']
        log = '\n'.join(lines)
        self.assertEqual(verify_catalogue_reads(image, log)['copied_pointers'], 28)
        with self.assertRaisesRegex(ValueError, 'startup copy'):
            verify_catalogue_reads(image, '\n'.join(lines[1:]))
        with self.assertRaisesRegex(ValueError, 'selection sequence'):
            verify_catalogue_reads(image, log.replace('pc=00420356', 'pc=00427396'))
        with self.assertRaisesRegex(ValueError, 'totals'):
            verify_catalogue_reads(image, log.replace('total=5', 'total=6'))
        malformed = bytearray(image)
        malformed[0x315994:0x315998] = b'\xff' * 4
        with self.assertRaisesRegex(ValueError, 'pointer outside'):
            catalogue(malformed)
        malformed = bytearray(image)
        malformed[0x1eab8 + 4:0x1eab8 + 6] = b'\xff' * 2
        with self.assertRaisesRegex(ValueError, 'overlaps'):
            catalogue(malformed)
        captures['loader2'] = captures['loader2'][:-2]
        with self.assertRaisesRegex(ValueError, 'loader2 capture differs'):
            assess(image, captures)
