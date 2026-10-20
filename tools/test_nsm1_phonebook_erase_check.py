import unittest
from tools.nsm1_phonebook_erase_check import check_erase,KEYS


class EraseTest(unittest.TestCase):
    def test_two_physical_confirmations_and_no_sim_update(self):
        trace='\n'.join(f'nsm1_security_contact: key={key:02x} down={down} t=1 firmware_writes=0' for key in (1,2,3,4,5,0x19)+KEYS for down in (1,0))
        for code in ('3132333435000000','313233343500ffff'):
            trace+=f'\nnsm1_security_submit: text={code} caller=0029faf3\nnsm1_security_gate: match=00000001 retry_pointer=00112824 retry=00\nnsm1_security_accepted: result=00000001'
        trace+='\n'+'\n'.join(f'nsm1_sim_tx: index={i} value={value:02x}' for i,value in enumerate(bytes.fromhex('a0f2000016'),1))
        check_erase(trace)
        for bad in (trace.replace('313233343500ffff','313233343600ffff'),trace.replace('result=00000001','result=00000000',1),trace+'\nSIM status ins=dc sw=9000',trace.replace('value=f2','value=dc')):
            with self.assertRaises(ValueError):check_erase(bad)


if __name__=='__main__':unittest.main()
