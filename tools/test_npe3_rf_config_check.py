import unittest
import hashlib
from unittest.mock import patch
from tools.npe3_rf_config_check import check,check_formatter


class ConfigProvenanceTest(unittest.TestCase):
    def fixture(self):
        prefix=bytearray(294)
        for address,value in ((0x3efc,0x1054),(0x3efe,0x082a),(0x3f0f,0x0502),(0x3f10,0x0308),
                              (0x3e7f,4675),(0x3e80,4450),(0x3ed4,8514),(0x3ed5,8039)):
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

    def test_direct_rx_buffered_rx_and_tx_are_distinct(self):
        _,prefix,_,_=self.fixture()
        program=bytes(0x20000)
        digest=hashlib.sha256(program[0x388c*2:0x3938*2]).hexdigest()
        lines=[];stream=[]
        for n,entry,key,pointer,scalar in ((1,'388e',35,0x31,9420),
                                          (2,'388e',838,0x2602,9352),
                                          (3,'389e',35,0x2604,8970)):
            lines.append(f'npe3_rf_formatter: sequence={n} entry={entry} t={n}.000000000 a={key:010x} ar2={pointer:04x} flags=0004 class=03f9 mode=0080 addend0=1243 addend1=1162 observer_writes=0')
            q,r=divmod(scalar-38,64);word=0x44000000+q*128+r+38
            if pointer==0x31:
                stream.extend((f'{n}.000001 0038bc 0031 {word&65535:04x} ffff',f'{n}.000002 0038c1 0032 4400 ffff'))
            else:
                for address,value in ((pointer,word&65535),(pointer+1,word>>16)):
                    lines.append(f'npe3_rf_formatter_store: sequence={n} t={n}.000003000 address={address:04x} value={value:04x} mask=ffff owner=0038c7 observer_writes=0')
        lines.append('npe3_rf_formatter_complete: entries=3 cap=8192 observer_writes=0')
        text='\n'.join(lines)
        with patch('tools.npe3_rf_config_check.FORMATTER_SHA256',digest):
            result=check_formatter(text,program,prefix,stream)
            self.assertEqual((result['rx_entries'],result['tx_entries']),(2,1))
            for changed in (text.replace('entry=389e','entry=388e'),text.replace('value=45ca','value=494c'),
                            text.replace('address=2605','address=2606'),text.replace('sequence=3 t=3.000003000','sequence=3 t=2.000003000')):
                with self.assertRaises(ValueError):check_formatter(changed,program,prefix,stream)


if __name__=='__main__':unittest.main()
