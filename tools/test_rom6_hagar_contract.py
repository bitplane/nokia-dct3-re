import unittest
from tools.rom6_hagar_contract import check


class HagarReplayTest(unittest.TestCase):
    def fixture(self):
        commands=[(0x082a,0x4100,1,0x082a,0,0,0,0,0),
                  (0x0208,0x4a80,5,0x082a,0,0x0208,0,0,0),
                  (0x48c8,0x4400,7,0x082a,0x48c8,0x0208,1,838,1870400000)]
        lines=[]
        for sequence,(low,high,seen,route,pll,configuration,candidate,key,frequency) in enumerate(commands,1):
            time=f'1.{sequence:09d}'
            lines.append(f'hagar_command: sequence={sequence} control=0100 word={high:04x}{low:04x} seen={seen} route={route:04x} pll={pll:04x} configuration={configuration:04x} candidate={candidate} key={key} frequency={frequency} rf_ready=0 t={time}')
            lines.append(f'dsp_compare_output: sequence={sequence} control=0100 low={low:04x} high={high:04x} target=0100 counter=0100 pending_before=0 t={time}')
        return '\n'.join(lines)

    def test_exact_receipts(self):
        result=check(self.fixture())
        self.assertEqual(result['candidate_keys'],[838])
        self.assertFalse(result['rf_readiness'])

    def test_reject_candidate_fallback_or_raw_source_change(self):
        for old,new in [('key=838','key=1'),('word=440048c8','word=440048ce'),
                        ('configuration=0208','configuration=2018'),('rf_ready=0','rf_ready=1')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(self.fixture().replace(old,new))

    def test_reject_missing_and_overflow(self):
        text=self.fixture()
        for changed in ('',text+'\nhagar_command: overflow=1',
                        text.replace('hagar_command: sequence=3','hagar_command: sequence=4')):
            with self.assertRaises(ValueError): check(changed)


if __name__=='__main__': unittest.main()
