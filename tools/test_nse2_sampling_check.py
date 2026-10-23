import unittest
from tools.nse2_sampling_check import inspect_sampling_controls


class SamplingCheckTest(unittest.TestCase):
    def fixture(self):
        program=bytearray(0x20000)
        for address,encoded in ((0x447e,'491109f82193f84a448501f800eef49575f8000b000a'),
                                (0x448b,'f1203a98f49575f8000b000b'),(0x4494,'75f800110009')):
            data=bytes.fromhex(encoded)
            program[address*2:address*2+len(data)]=data
        rows=[]
        for n in range(1,13):
            for port,value,pc in (('0a','028f','4489'),('0b','3a98','4491'),('09','0332','4497')):
                rows.append(f'nse2_sampling_control: port={port} value={value} mask=ffff n={n} pc={pc} mode=000c t=5.0 observer_writes=0')
        for port,count,last in (('09',20,'0332'),('0a',37,'028f'),('0b',37,'3a98')):
            rows.append(f'nse2_sampling_summary: port={port} writes={count} last={last} t=30.0 observer_writes=0')
        return program,'\n'.join(rows)

    def test_sampling_prefix_and_full_counts(self):
        program,text=self.fixture()
        self.assertFalse(inspect_sampling_controls(program,text)['physical_semantics_proved'])
        for broken in (text.replace('writes=37','writes=38',1),
                       text.replace('n=12','n=13',1),text.replace('value=028f','value=0290',1),
                       text.replace('port=09 writes=20','port=09 writes=0')):
            with self.assertRaises(ValueError):
                inspect_sampling_controls(program,broken)
        program[0x447e*2]^=1
        with self.assertRaises(ValueError):
            inspect_sampling_controls(program,text)


if __name__=='__main__':
    unittest.main()
