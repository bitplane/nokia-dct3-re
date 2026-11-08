import unittest
from tools.make_npe3_record_profile import low_records,make_profile,replace_low_records,assess_runtime
from tools.dct3_rom6_record_codec import decode_record
from tools.nse5_pmm_journal import replay


class Npe3RecordFixtureTest(unittest.TestCase):
    tables=dict(faid_table=bytes.fromhex('b8338004b848851a2dcea128'),
                identity_table=bytes.fromhex('ca635ae719faa64c0d78fc16'),
                lock_table=bytes.fromhex('7bb4d0ef9eb20abe73dad335'))
    chip=bytes.fromhex('00160010')

    def test_explicit_storage_records_decode_to_declared_policy(self):
        records=low_records(bytes.fromhex('e1906dcf'),self.chip,'49015420621001',**self.tables)
        self.assertEqual(len(records),0x110)
        self.assertEqual(records[12:20].hex(),'49015420621001ff')
        identity=decode_record(records[:12],self.chip,self.tables['identity_table'])
        self.assertEqual(identity.hex(),'792949015420621001ff0000')
        self.assertEqual(identity[2:10],records[12:20])
        self.assertEqual(decode_record(records[20:32],self.chip,self.tables['faid_table']).hex(),
                         'f65a0d4609a5f2b900000000')
        for start in range(32,0x110,12):
            self.assertEqual(decode_record(records[start:start+12],self.chip,self.tables['lock_table'])[-2:],
                             bytes.fromhex('54c2'))

    def test_malformed_inputs_and_unpinned_profiles_reject(self):
        for seed,chip,imei in ((bytes(3),self.chip,'49015420621001'),
                               (bytes(4),bytes(3),'49015420621001'),
                               (bytes(4),bytes.fromhex('01000000'),'49015420621001'),
                               (bytes(4),self.chip,'4901542062100x')):
            with self.assertRaises(ValueError):low_records(seed,chip,imei,**self.tables)
        with self.assertRaises(ValueError):make_profile(bytes(10),bytes(10),bytes(10))

    def test_outer_checksums_and_unrelated_storage_are_preserved(self):
        original=bytearray([255])*0x6000
        original[6:12]=b'EEPROM'
        original[0x18:0x1a]=bytes([0,1])
        original[0x20:0x26]=bytes.fromhex('000009c40000')
        original[0x26:0x26+0x9c4]=bytes(at%256 for at in range(0x9c4))
        records=bytes(at%256 for at in range(0x110))
        fixture=replace_low_records(bytes(original),records)
        cache,rows,_=replay(fixture,0x9c4)
        self.assertEqual(cache[:0x110],records)
        self.assertEqual(int.from_bytes(cache[0x11c:0x120],'big'),sum(cache[:0x11c])&65535)
        self.assertEqual(fixture[0x21],sum(cache)&255)
        changed={at for at,(old,new) in enumerate(zip(original,fixture)) if old!=new}
        allowed={0x21}|set(range(0x26,0x136))|set(range(0x142,0x146))
        self.assertTrue(changed<=allowed)
        self.assertEqual(rows,[dict(record=0x20,source=0x26,destination=0,length=0x9c4)])

    def test_initial_validation_does_not_claim_the_complete_family(self):
        states=[(0,0xf0f),(0,0x45b4),(1,0x45b7),(1,0x45bc),(3,0x45bf),(3,0x45e6),(7,0x45e9),(3,0x45e6)]
        text='\n'.join(f'npe3_integrity_flags: sequence={index} t=0.7 value={value:04x} mask=ffff owner={owner:06x} observer_writes=0'
                       for index,(value,owner) in enumerate(states,1))
        result=assess_runtime(text)
        self.assertTrue(result['initial_native_validations_qualified'])
        self.assertFalse(result['complete_record_family_qualified'])
        self.assertEqual(result['later_validation_writes'],[dict(value=3,owner='0045e6')])
        for bad in ('',text.replace('value=0007','value=0003'),text.replace('owner=0045e9','owner=0045b7')):
            with self.assertRaises(ValueError):assess_runtime(bad)
        settled=states[:7]+[(3,0x45e6),(7,0x45e9)]*10
        full='\n'.join(f'npe3_integrity_flags: sequence={index} t=0.7 value={value:04x} mask=ffff owner={owner:06x} observer_writes=0'
                       for index,(value,owner) in enumerate(settled,1))
        with self.assertRaises(ValueError):assess_runtime(full)
        reads='\n'+'\n'.join(f'npe3_record_storage_read: sequence={index} offset={offset:04x} length=24 '
                              f'caller={"00387471" if index==1 else "00387f3f"} t=0.7 observer_writes=0'
                              for index,offset in enumerate(list(range(0x20,0x110,24))+[0x20],1))
        reads+='\nnpe3_record_storage_read_complete: reads=11 cap=16 observer_writes=0'
        self.assertTrue(assess_runtime(full+reads)['complete_record_family_qualified'])
        with self.assertRaises(ValueError):assess_runtime(full+reads.replace('offset=0038','offset=0020'))
        self.assertFalse(assess_runtime(full.rsplit('\n',1)[0])['complete_record_family_qualified'])


if __name__=='__main__':unittest.main()
