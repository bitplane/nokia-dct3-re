import unittest
from tools.nse2_sampling_check import inspect_sampling_controls,inspect_peripheral_routes,inspect_peripheral_consumers,inspect_word_port_activity,inspect_word_control_contract


class SamplingCheckTest(unittest.TestCase):
    def test_word_host_command_uses_data_space_and_counter_consumers(self):
        import struct
        from tools.nse2_sampling_check import inspect_word_host_command
        program=bytearray(0x20000)
        data=bytearray(0x20000)
        for address,encoded in {
            0x398d:'f000b00f4a1188117710000110818a11f4e2',
            0x8e4b:'12f806f5f150fffffa4d8e690af806b3',
            0x8e6e:'6bf806c400016bf806b30001',
            0x9185:'13f806c40bf806f6f84b9191',
        }.items():
            fragment=bytes.fromhex(encoded)
            program[address*2:address*2+len(fragment)]=fragment
        struct.pack_into('>H',data,(0xb00f+0x4c)*2,0x3b92)
        self.assertEqual(inspect_word_host_command(program,data)['command'],'4c')
        with self.assertRaises(ValueError):
            inspect_word_host_command(program,program)
        struct.pack_into('>H',data,(0xb00f+0x4b)*2,0x3b92)
        with self.assertRaises(ValueError):
            inspect_word_host_command(program,data)
        with self.assertRaises(ValueError):
            inspect_word_host_command(program,data[:-2])

    def test_word_configuration_and_vector_output_contract(self):
        from tools.nse2_sampling_check import inspect_word_configuration_contract
        program=bytearray(0x20000)
        fragments={
            0x9232:'f07423e877120060771306f3771406f4',
            0x9295:'74820036118261840300f330fe01ff30f3400180f030007ef2a08082f49575820036',
            0x92a6:'1084f030079e8083f0749009',
            0x34da:'74f8000b0036f3302000f84d34f1',
            0x34e8:'75f8000b0034',0x34f4:'75f800080034',0xffd0:'f2733416',
            0x3bb1:'81f806f410d2f130ff00f77863f8000b015bf77c81f806f6',
            0x3bc8:'80f806f5f07439004a12f07492328a12f0733924',
        }
        for address,encoded in fragments.items():
            data=bytes.fromhex(encoded)
            program[address*2:address*2+len(data)]=data
        result=inspect_word_configuration_contract(program)
        self.assertEqual(result['state_bits_to_control'],'007e')
        self.assertFalse(result['completion_source_proved'])
        for address in fragments:
            changed=program.copy()
            changed[address*2]^=1
            with self.assertRaises(ValueError):
                inspect_word_configuration_contract(changed)

    def test_word_control_acknowledges_after_transmitter_call(self):
        program=bytearray(0x20000)
        for a,v in ((0x8da5,'76820700f49575820001ec0ef4957482000269820700f49575820002'),
                (0x90ad,'698204006882fcfff49575820002'),
                (0x8ec5,'f07423f6f0749162f0200100f49575f800080001'),
                (0x90e9,'10f806c5fe447712006074f800080036f1301000fc4d'),
                (0x9129,'748200366882ffe16982801cf49575820036'),
                (0x913c,'771200607482003661824000f820913e68827ffff49575820036')):
            data=bytes.fromhex(v)
            program[a*2:a*2+len(data)]=data
        result=inspect_word_control_contract(program)
        self.assertTrue(result['acknowledgement_after_transmitter_call'])
        self.assertFalse(result['baud_and_framing_proved'])
        for a in (0x8da6,0x90ae,0x8ec8,0x90f2,0x9130,0x9141):
            changed=program.copy()
            changed[a*2]^=1
            with self.assertRaises(ValueError):
                inspect_word_control_contract(changed)

    def test_pending_word_transmitter_requires_complete_activity(self):
        import struct
        data=bytearray(0x20000)
        struct.pack_into('>6H',data,0xb3d1*2,0x9185,0x9199,0x91b0,0x91be,0x91fb,0x9200)
        rows=[]
        for p,v,pc,n,t in (('36','0080','8d98',1,.249),('36','0080','90bc',2,.2491),('34','1eff','920d',1,.3425)):
            rows.append(f'nse2_word_port_write: port={p} value={v} mask=ffff pc={pc} n={n} t={t} observer_writes=0')
        for p,r,w,v in (('34',0,1,'1eff'),('35',0,0,'0000'),('36',4,2,'0080')):
            rows.append(f'nse2_word_port_summary: port={p} reads={r} writes={w} last={v} t=30.0 observer_writes=0')
        rows+=['nse2_word_tx_state: state=0001 flags=0001 pending=0009 pointer=0701 t=0.3425 observer_writes=0',
               'nse2_word_terminal: state=0002 flags=0001 pending=0009 pointer=0701 t=30.0 observer_writes=0']
        text='\n'.join(rows)
        self.assertEqual(inspect_word_port_activity(data,text)['pending'],9)
        for bad in (text.replace('pending=0009','pending=0000'),text.replace('port=34 reads=0 writes=1','port=34 reads=0 writes=2'),text.replace('value=1eff','value=0000')):
            with self.assertRaises(ValueError):
                inspect_word_port_activity(data,bad)
        data[0xb3d3*2]^=1
        with self.assertRaises(ValueError):
            inspect_word_port_activity(data,text)

    def test_raw_port_reader_is_a_delayed_call_operand(self):
        from tools.c54x_rom4_port_census import census
        program=bytearray(0x20000)
        for address,encoded in ((0x71ee,'f27474fa771a000a'),
                (0x8ec5,'f07423f6f0749162f0200100'),(0x8ed8,'74f8000800358813f1200200'),
                (0xb792,'74f80008003576f806b20000'),(0xb81f,'4d82f49575f800080034f07335f7'),
                (0x917b,'10f806c6f000b3d1881174f8000800361181f5e2')):
            data=bytes.fromhex(encoded)
            program[address*2:address*2+len(data)]=data
        self.assertIn(0x71ef,census(program)[('R',0x0a)])
        result=inspect_peripheral_consumers(program)
        self.assertFalse(result['sampling_control_link_proved'])
        self.assertEqual(result['rejected_raw_port_read']['candidate'],'71ef')
        program[0x71ee*2]^=1
        with self.assertRaises(ValueError):
            inspect_peripheral_consumers(program)

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
