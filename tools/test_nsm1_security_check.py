import unittest
from pathlib import Path
from tools.nsm1_security_check import check_trace
from tools.nsm1_security_contract import inspect


class SecurityTest(unittest.TestCase):
    def test_physical_matching_code_is_not_acceptance(self):
        base='\n'.join(f'nsm1_security_contact: key={key:02x} down={down} t=1 firmware_writes=0' for key in (1,2,3,4,5,0x19,0x19) for down in (1,0))
        submit='nsm1_security_submit: text=3132333435000000 caller=0029faf3'
        gate='nsm1_security_gate: match=00000001 retry_pointer=00112824 retry='
        accepted=base+'\n'+submit+'\n'+gate+'00\nnsm1_security_accepted: result=00000001'
        rejected=base+'\n'+submit+'\n'+gate+'2b\nnsm1_security_submit: text=0032333435000000 caller=0029faf3\nnsm1_security_gate: match=00000000 retry_pointer=00112824 retry=2b'
        check_trace(accepted,True);check_trace(rejected,False)
        for bad in (accepted.replace('retry=00','retry=2b'),accepted.replace('key=05','key=06'),accepted.replace('result=00000001','result=00000000')):
            with self.assertRaises(ValueError):check_trace(bad,True)
        with self.assertRaises(ValueError):check_trace(rejected,True)

    def test_own_initializer_and_retry_record_when_available(self):
        source=Path(__file__).resolve().parents[1]/'roms/research/nsm1-v523/6150-v523-ppm-c.fls'
        if not source.exists():self.skipTest('acquired NSM-1 input not present')
        image=source.read_bytes()
        self.assertEqual(inspect(image)['eeprom_offset'],0x3d0)
        for address in (0x2bcd96,0x2bcdd0,0x29d6d4,0x2dde44):
            bad=bytearray(image);bad[address-0x200000]^=1
            with self.assertRaises(ValueError):inspect(bad)


if __name__=='__main__':unittest.main()
