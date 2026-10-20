import unittest

from tools.nsk1_pin_request_roundtrip import check_transition,check_menu_contacts,SEQUENCE
from tools.nsk1_puk_roundtrip import exhaustion_profile
from tools.sim_security_trace_check import ENABLED_OFFSET


class PinRequestTest(unittest.TestCase):
    def test_only_enabled_flag_can_change(self):
        original=exhaustion_profile()
        disabled=bytearray(original)
        disabled[ENABLED_OFFSET]=0
        check_transition(original,disabled,False)
        check_transition(disabled,original,True)
        for offset in (0,3484,3516,3569):
            altered=bytearray(disabled)
            altered[offset]^=1
            with self.assertRaises(ValueError):
                check_transition(original,altered,False)
        for source in (b'',original[:3520],disabled):
            with self.assertRaises(ValueError):
                check_transition(source,disabled,False)

    def test_contacts_are_complete_ordered_and_phase_specific(self):
        for enabled in (False,True):
            sequence=SEQUENCE[1:] if enabled else SEQUENCE
            trace='\n'.join(f'nsk1_pin_request_contact: index={i} key={name} down={down} t=30.0 physical_only=1'
                            for i,name in enumerate(sequence,1) for down in (1,0))
            check_menu_contacts(trace,enabled)
            for bad in (trace.replace('key=one','key=two'),trace+'\nnsk1_pin_request_contact: malformed',trace.split('\n',1)[1]):
                with self.assertRaises(ValueError):
                    check_menu_contacts(bad,enabled)


if __name__ == '__main__':
    unittest.main()
