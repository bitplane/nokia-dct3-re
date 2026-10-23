import unittest
from tools.nse2_sampling_check import inspect_sampling_controls,inspect_peripheral_routes


class SamplingCheckTest(unittest.TestCase):
    def test_peripheral_routes_preserve_alternate_selector(self):
        import struct
        words=[0]*65536
        for address,mask,target in ((0x35c0,0x1000,0x369a),(0x35c4,0x1c,0x362f),
                (0x35cd,0x100,0x8ec5),(0x35d1,0x200,0x8ed3),(0x35d7,0x100,0xb7dc),
                (0x35db,0x200,0xb787),(0x35df,0x400,0x8e89),(0x35e3,0x80,0x371d),
                (0x35e7,2,0x3621),(0x35eb,1,0x362d),(0x35ef,0x20,0x3764),(0x35f3,0x40,0x3764)):
            words[address:address+4]=[0xf130,mask,0xf84c,target]
        words[0x35c8:0x35cd]=[0x61f8,0x06f3,0x0400,0xf830,0x35d7]
        self.assertEqual(len(inspect_peripheral_routes(struct.pack('>65536H',*words))['routes']),12)
        for address in (0x35c1,0x35ca,0x35d0):
            changed=words.copy()
            changed[address]^=1
            with self.assertRaises(ValueError):
                inspect_peripheral_routes(struct.pack('>65536H',*changed))

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
