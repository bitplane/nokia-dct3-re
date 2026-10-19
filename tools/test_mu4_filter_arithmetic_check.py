import unittest

from tools.mu4_filter_arithmetic_check import check, reference_iteration


WORDS = [0,0xfffd,0x3fb6,0x87ea,0xfffd,0x804a,0,0xa9e8,0,0xa9e8,0,
         0x804a,0,0,0x804a,0x87ea,0,0x3fb6,0,0,0x3fb6,0x1000,0x7e48,0xfffd,0xfffd]
ADDRESSES = [0x1318,0x1000,0x131c,0x131d,0x1318,0x131e,0x1319,0x131f,0x1319,
             0x131f,0x1320,0x131e,0x1320,0x1324,0x131e,0x131d,0x1321,0x131c,
             0x1321,0x1325,0x131c,0x131b,0x1325,0x1000,0x1000]


def fixture(*,dma=False,prefix=False):
    lines = ['mu4_filter_scope: first=1.061 last=1.062 cap=50000 first_address=1000 last_address=13ff context=02a2c6..02a2eb boundary=data_bus observer_writes=0 audio_acceptance=0']
    ordinal = 0
    def add(kind,address,word,time,pc):
        nonlocal ordinal
        ordinal += 1
        lines.append(f'mu4_filter_bus: ordinal={ordinal} kind={kind} address={address:04x} '
                     f'word={word:04x} mask=ffff t={time:.9f} context_pc={pc:06x} st1=e105 pmst=202d observer_writes=0')
    if dma:
        add('read',0x1000,0,1.06135,0x2a2eb)
        lines.append('mu4_playback_dma_epoch: address=1000 word=0000 t=1.061350000 channel=3 ownership=active_source_read observer_writes=0')
    if prefix:
        add('read',0x115f,0x1000,1.06135,0x2a2c7)
    stores = {2:(0x1318,0xfffd,0x2a2d0),20:(0x1325,0x7e48,0x2a2e0),
              21:(0x1321,1,0x2a2e1),24:(0x1000,0xfffd,0x2a2ea)}
    for index,(address,word) in enumerate(zip(ADDRESSES,WORDS)):
        time = 1.0614+index/1000000
        add('read',address,word,time,0x2a2d2)
        if index in stores:
            destination,value,pc = stores[index]
            add('write',destination,value,time,pc)
    lines.append(f'mu4_filter_terminal: events={ordinal} closed=1 capped=0 illegal=0 t=2.016666667 observer_writes=0')
    return '\n'.join(lines)


class FilterArithmeticTest(unittest.TestCase):
    def test_negative_floor_and_nonzero_feedback_state(self):
        # -24465 + (-52191 >> 16) = -24466; x4 then negate = 0x00017e48.
        self.assertEqual(reference_iteration(WORDS),[0xfffd,0x7e48,1,0xfffd])
        result = check(fixture(),section_words=1)
        self.assertEqual(result['iterations'],1)
        self.assertEqual(result['nonzero_state_iterations'],1)
        self.assertEqual(result['actual_bus_reads'],25)
        self.assertEqual(result['modeled_forwarded_high_operands'],1)
        self.assertFalse(result['native_audio_validated'])

    def test_neutral_direct_coefficient_and_declared_fractional_mode(self):
        values = [0]*25
        values[1] = values[4] = values[23] = values[24] = 16
        values[21] = 0x1000
        self.assertEqual(reference_iteration(values),[16,0,0,16])
        values[21] = 0x0800
        self.assertEqual(reference_iteration(values),[16,0,0,12])
        self.assertEqual(reference_iteration(values,fractional=True),[16,0,0,16])

    def test_rejects_changed_state_output_geometry_modes_and_coverage(self):
        text = fixture()
        for old,new in [('address=1325 word=7e48','address=1325 word=7e49'),
                        ('kind=write address=1000 word=fffd','kind=write address=1000 word=fffc'),
                        ('address=131d','address=131c'),('st1=e105','st1=e305'),
                        ('pmst=202d','pmst=202c'),('capped=0','capped=1')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(text.replace(old,new),section_words=1)
        with self.assertRaisesRegex(ValueError,'incomplete section'):
            check(text)

    def test_reference_requires_observed_operand_domain(self):
        for values in (WORDS[:-1],[-1]+WORDS[1:],[65536]+WORDS[1:]):
            with self.assertRaises(ValueError):
                reference_iteration(values)

    def test_dma_and_boundary_context_are_counted_separately(self):
        result = check(fixture(dma=True,prefix=True),section_words=1)
        self.assertEqual(result['excluded_actual_dma_reads'],1)
        self.assertEqual(result['boundary_context_reads'],1)
        self.assertEqual(result['actual_bus_reads'],25)


if __name__ == '__main__':
    unittest.main()
