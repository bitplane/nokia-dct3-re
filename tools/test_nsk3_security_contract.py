import struct
import unittest
from tools.nsk3_security_contract import inspect
from tools.nsk3_security_check import check_trace


class SecurityContractTest(unittest.TestCase):
    def test_runtime_predicate_distinguishes_retry_from_match(self):
        trace='\n'.join([
            *[f'nsk3_security_contact: key={key:02x} down=1 column=1' for key in (1,2,3,4,5,0x19)],
            'nsk3_security_submit: pointer=0010ba2c text=3132333435000000 caller=0028a1d7',
            'nsk3_security_gate: match=00000001 retry_pointer=0010c520 retry=ff'])
        check_trace(trace,False)
        positive=trace.replace('retry=ff','retry=00')+'\nnsk3_security_accepted: result=00000001'
        check_trace(positive,True)
        menu=positive+'\nnsk3_security_contact: key=19 down=1 column=1'
        check_trace(menu,True,(0x19,))
        with self.assertRaises(ValueError):check_trace(menu,True)
        for bad in (positive.replace('match=00000001','match=00000000'),
                    positive.replace('3132333435','3132333436'),
                    positive.replace('key=19','key=17'),trace):
            with self.assertRaises(ValueError):check_trace(bad,True)

    def image(self):
        data=bytearray(0x100000)
        data[0xa64bc:0xa64c8]=bytes.fromhex('b510493d78092905db05202b')
        data[0xa63c4:0xa63d8]=bytes.fromhex('b50048792100710121041808497b2200f7fdfabb')
        for offset,value in ((0xa65ac,0x10c51c),(0xa65b4,0x10c520),
                             (0xa65c0,0x70a),(0xa39d8,0x2c1aa0)):
            struct.pack_into('>I',data,offset,value)
        struct.pack_into('>IHH',data,0xc1aa0+56,0x2c1798,97,0)
        for i,record in ((1,(0x701,0x3b0,8)),(9,(0x709,0x3b0,4)),(10,(0x70a,0x3b4,1))):
            struct.pack_into('>IHH',data,0xc1798+i*8,*record)
        return data

    def test_own_retry_contract_and_mutations(self):
        image=self.image()
        result=inspect(image)
        self.assertEqual(result['retry_eeprom'],0x3b4)
        self.assertEqual(result['rejected_count_at_least'],5)
        for offset in (0xa64bc,0xa63c4,0xa65ac,0xa65b4,0xa65c0,0xa39d8,
                       0xc1aa0+56,0xc1798+8,0xc1798+9*8,0xc1798+10*8):
            bad=bytearray(image)
            bad[offset]^=1
            with self.assertRaises(ValueError):inspect(bad)


if __name__=='__main__':unittest.main()
