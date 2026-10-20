import unittest

from tools.nsk3_program_census_check import check_census


class ProgramCensusTest(unittest.TestCase):
    def setUp(self):
        regions=((0x1ca4,0x1cff,92),(0x1d00,0x1d93,148),
                 (0x216a,0x21ff,150),(0x2200,0x2285,134),
                 (0x23c4,0x23ff,60),(0x2400,0x24ff,256),
                 (0x2500,0x25ff,125),(0x2600,0x26ff,256),
                 (0x2700,0x27ff,256),(0x2800,0x28ff,256),
                 (0x2900,0x29ff,256),(0x2a00,0x2aff,256),
                 (0x2b00,0x2b2a,43))
        self.trace=''
        for lo,hi,count in regions:
            self.trace+=f'nsk3_program_write_page: page={lo>>8:02x} writes={count} unique={count} min={lo:04x} max={hi:04x} nonzero={count} visible={count} observer_writes=0\n'
        self.trace+='nsk3_program_write_page: page=ff writes=2 unique=1 min=ff87 max=ff87 nonzero=1 visible=0 observer_writes=0\n'
        self.trace+='nsk3_program_write_census: writes=2290 unique=2289 words_at_or_above_3000=1 t=1.000000000 observer_writes=0\n'
        for word,pc in (('0004','0f12'),('0100','4a0d')):
            self.trace+=f'nsk3_program_high_write: address=ff87 data={word} mask=ffff pc={pc}\n'
        for index,pc in enumerate(('0f15','0f6a','4a11'),1):
            self.trace+=f'nsk3_program_identity_read: index={index} value=0004 pc={pc}\n'

    def test_complete_census(self):
        self.assertEqual(check_census(self.trace)['retained_upload_words'],2288)

    def test_rejects_unretained_code_or_mutable_rom(self):
        for trace in (self.trace.replace('visible=92','visible=0'),
                      self.trace.replace('nonzero=1 visible=0','nonzero=1 visible=1'),
                      self.trace.replace('index=3 value=0004','index=3 value=0100')):
            with self.assertRaises(ValueError):check_census(trace)

    def test_rejects_incomplete_extra_or_observer_events(self):
        for trace in (self.trace.replace('page=2b','page=2c'),
                      self.trace.replace('unique=2289','unique=2288'),
                      self.trace.replace('address=ff87 data=0100','address=ff88 data=0100'),
                      self.trace+'nsk3_program_identity_read: index=4 value=0004 pc=408d\n'):
            with self.assertRaises(ValueError):check_census(trace)


if __name__=='__main__':unittest.main()
