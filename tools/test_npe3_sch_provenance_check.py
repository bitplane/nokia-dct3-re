import unittest

from tools.npe3_sch_provenance_check import check
from tools.rom6_adc_lab_check import reference_word


class SchProvenanceTest(unittest.TestCase):
    def fixture(self):
        lines = ['mfi_rx_epoch: first_tick=42243384 divider=12 physical_clock_claim=0']
        for index in range(1024):
            sequence = index + 1 if index < 64 else index - 63
            tick = 41586924 + index * 12 if index < 64 else 42243384 + (index - 64) * 12
            value = reference_word(sequence, tick, True, 12, True, True)
            lines.append(f'npe3_sch_raw: ordinal={index+1} t={tick/13000000+.00001:.9f} value={value:04x} source_tick={tick} capture_sequence={sequence} owner=002592 observer_writes=0')
        owners = (0x794a, 0x794b, 0x794f, 0x7950, 0x7955, 0x7956, 0x7958, 0x7959)
        for index in range(192):
            address = 0xd8 + index
            ar3 = (address & ~1) + 2
            lines.append(f'npe3_sch_writer: address={address:04x} value=0001 t=3.250450000 owner={owners[index%8]:06x} ar2={ar3+2:04x} ar3={ar3:04x} observer_writes=0')
        lines.append('npe3_sch_provenance: raw_total=1024 raw_cap=1024 writers=192 begin=3 end=3.4 aperture_t=3.250482022 observer_writes=0')
        lines.append('npe3_sch_aperture: t=3.250482022 owner=0076ae pointer=00d8 current=0000,0029 next=0000,002a samples='+','.join(['0001']*192)+' observer_writes=0')
        lines.append('npe3_sch_filter_input: t=3.250410000 source=0082 samples='+','.join(['0001']*196)+' coefficients=df85,6d1b,df85 history=0000,0000,02f8,01ba st1=c100 observer_writes=0')
        return '\n'.join(lines)

    def test_partial_sch_is_not_synchronization(self):
        report = check(self.fixture())
        self.assertEqual(report['missing_sch_prefix_complex_samples'], 141)
        self.assertEqual(report['source_frame'], 704)
        self.assertFalse(report['sch_qualified'])

    def test_reject_changed_source_order_builder_and_accounting(self):
        text = self.fixture()
        for old, new in [('value=0000', 'value=0001'), ('source_tick=42243384', 'source_tick=42243385'),
                         ('capture_sequence=960', 'capture_sequence=959'), ('owner=00794a', 'owner=007949'),
                         ('ar2=00dc', 'ar2=00dd'), ('address=00d8 value=0001', 'address=00d8 value=0002'),
                         ('writers=192', 'writers=191'), ('raw_total=1024', 'raw_total=1025'),
                         ('st1=c100', 'st1=c101'), ('current=0000,0029', 'current=0000,0028')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check(text.replace(old, new, 1))


if __name__ == '__main__':
    unittest.main()
