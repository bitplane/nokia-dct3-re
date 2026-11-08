import unittest
from tools.npe3_rf_config_check import check


class ConfigProvenanceTest(unittest.TestCase):
    def fixture(self):
        prefix=bytearray(294)
        for address,value in ((0x3efc,0x1054),(0x3efe,0x082a),(0x3f0f,0x0502),(0x3f10,0x0308)):
            at=2*(address-0x3e7e);prefix[at:at+2]=value.to_bytes(2,'big')
        prefix=bytes(prefix)
        mcu=b'\xff'*0x2083c+prefix
        lines=[]
        for n,(address,value,owner) in enumerate((('3f0f','0000','000f14'),('3f10','0000','000f14'),('3f0f','0502','0022ad'),('3f10','0308','0022ad')),1):
            lines.append(f'npe3_rf_config_source: sequence={n} t=0.{n} address={address} value={value} mask=ffff owner={owner} observer_writes=0')
        stream=[]
        for n,value in enumerate(('0502','0308'),1):
            lines.append(f'npe3_rf_config_stage: sequence={n} t={n}.000000000 value={value} source0=0502 source1=0308 route=1054 mask=ffff owner=ffffffff observer_writes=0')
            stream.extend((f'{n}.000001000 001a85 0031 {value} ffff',f'{n}.000002000 001a89 0032 4a80 ffff'))
        lines.append('npe3_rf_config_complete: writes=4 write_cap=128 stages=2 stage_cap=8192 observer_writes=0')
        return '\n'.join(lines),prefix,mcu,stream

    def test_own_provenance_not_physical_meaning(self):
        result=check(*self.fixture())
        self.assertEqual(result['staged_and_issued'],2)
        self.assertFalse(result['physical_bit_meaning_proved'])

    def test_reject_masked_alias_missing_write_and_wrong_upload(self):
        text,prefix,mcu,stream=self.fixture()
        for args in ((text.replace('value=0502','value=0402'),prefix,mcu,stream),
                     (text.replace('sequence=4','sequence=5'),prefix,mcu,stream),
                     (text,prefix,mcu[:-1]+b'\xff',stream),
                     (text,prefix,mcu,stream[:-2]),
                     (text,prefix,mcu,[r.replace('001a89','001a88') for r in stream])):
            with self.subTest(args=args[0][-60:]),self.assertRaises(ValueError):check(*args)


if __name__=='__main__':unittest.main()
