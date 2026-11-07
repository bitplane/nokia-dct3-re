import hashlib
import io
import unittest
from unittest.mock import patch
from tools import rom6_config_contract as contract
from tools.rom6_tuning_contract import KEYS


def fixture():
    prefix=bytearray(contract.PREFIX_WORDS*2)
    for address,value in ((0x3efc,0x1054),(0x3efe,0x082a),(0x3f0f,0x0402),(0x3f10,0x0208)):
        offset=2*(address-contract.PREFIX_BASE); prefix[offset:offset+2]=value.to_bytes(2,'big')
    overlay=bytearray(0x4000); selector=bytes(range(60))
    overlay[0x1990*2:0x19ae*2]=selector
    mcu=bytearray(contract.PREFIX_OFFSET+len(prefix))
    mcu[contract.PREFIX_OFFSET:]=prefix
    mcu[0x1161b0:0x1161b0+60]=selector
    text=[]; writes=[]
    for i,(address,value,pc) in enumerate((('3f0f','0000','000f15'),('3f10','0000','000f15'),('3f0f','0402','0022af'),('3f10','0208','0022af')),1):
        text.append(f'rom6_config_write: sequence={i} address={address} value={value} cpu_pc={pc} mask=ffff observer_writes=0')
    text.append('rom6_config_prefix: base=3e7e words=147 incoming_store=3f10 observer_writes=0')
    for i,key in enumerate(sorted(KEYS),1):
        gsm=key<=124 or key>=975
        cls=4 if gsm else 0; value=0x0402 if gsm else 0x0208; old=0x1054 if gsm else 0x082a
        text.extend((f'rom6_config_class_entry: sequence={i} key={key:010x} observer_writes=0',
            f'rom6_config_class_result: sequence={i} class263b={cls:04x} observer_writes=0',
            f'rom6_config_selector: sequence={i} old_payload={old:04x} match3efc=1054 class263b={cls:04x} observer_writes=0',
            f'rom6_config_select: sequence={i} t={i}.000000000 value={value:04x} word3f0f=0402 word3f10=0208 class263b={cls:04x} cpu_pc=00199f mask=ffff observer_writes=0'))
        text.append(f'rom6_tuning_entry: a={key:010x}')
        normalized=key-1024 if key>=975 else key
        scalar=2*(normalized+4675) if gsm else key+8514
        q,r=divmod(scalar-38,64); packed=0x44000000+q*128+r+38
        writes.append(f'{i}.050000000\t0038bc\t0031\t{packed&65535:04x}\tffff\n'.encode())
        writes.append(f'{i}.050000001\t0038c1\t0032\t{packed>>16:04x}\tffff\n'.encode())
        for port,word in ((0x31,value),(0x32,0x4a80)):
            writes.append(f'{i}.100000000\t001a95\t{port:04x}\t{word:04x}\tffff\n'.encode())
    text.append(f'rom6_config_complete: producers=4 producer_cap=128 selections={len(KEYS)} selection_cap=4096 observer_writes=0')
    return '\n'.join(text),b''.join(writes),bytes(prefix),bytes(mcu),bytes(overlay)


class ConfigContractTest(unittest.TestCase):
    def test_only_qualified_whole_words_filter_candidates(self):
        for word,band,key in ((0x4a800402,'GSM900',1),(0x4a800208,'DCS1800',838)):
            self.assertEqual(contract.observed_configuration_class(word),band)
            self.assertEqual(contract.candidates_for_configuration(0x440048c8,word)[0]['key'],key)
            self.assertEqual(contract.candidates_for_configuration(0x44009044,word),[])
        for word in (0x0402,0x4a800c22,0x4a802018,0x4a801402,0x41001054):
            with self.assertRaises(ValueError): contract.observed_configuration_class(word)

    def test_provenance_and_complete_class_selection_issuance(self):
        text,writes,prefix,mcu,overlay=fixture()
        program=bytes(0x20000)
        spans={(a,b):hashlib.sha256(program[a*2:b*2]).hexdigest() for a,b in contract.SOURCE_SPANS}
        with patch.object(contract,'SOURCE_SPANS',spans):
            result=contract.check(text,io.BytesIO(writes),prefix,mcu,overlay,program)
            self.assertEqual(result['distinct_keys'],548)
            self.assertEqual(result['next_config_matches_tuning'],548)
            self.assertFalse(result['delivered_band_selection_proved'])
            for old,new in (('old_payload=1054','old_payload=082a'),('value=0402','value=0403'),
                            ('sequence=2 ','sequence=1 '),('selections=548','selections=549'),
                            ('cpu_pc=00199f','cpu_pc=0029cd'),('old_payload=1054 ','old='),
                            ('t=1.000000000','t=nan')):
                with self.subTest(old=old),self.assertRaises(ValueError):
                    contract.check(text.replace(old,new,1),io.BytesIO(writes),prefix,mcu,overlay,program)
            for broken in (writes[:-1],writes+writes,writes.replace(b'4a80',b'4a81',1)):
                with self.assertRaises(ValueError): contract.check(text,io.BytesIO(broken),prefix,mcu,overlay,program)
            with self.assertRaises(ValueError): contract.check(text,io.BytesIO(writes),prefix[:-1],mcu,overlay,program)
            with self.assertRaises(ValueError): contract.check(text,io.BytesIO(writes),prefix,mcu,overlay[:-1],program)


if __name__=='__main__': unittest.main()
