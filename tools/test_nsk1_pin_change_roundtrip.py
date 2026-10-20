import unittest
from tools.nsk1_pin_change_roundtrip import check_change,check_change_contacts,MENU
from tools.nsk1_puk_roundtrip import exhaustion_profile
from tools.sim_security_trace_check import CHV_OFFSET


class PinChangeTest(unittest.TestCase):
    def test_only_pin_changes(self):
        original=exhaustion_profile()
        changed=bytearray(original)
        changed[CHV_OFFSET:CHV_OFFSET+4]=b'4321'
        check_change(original,changed)
        for offset in (0,CHV_OFFSET,3516,3517,3520,3569):
            bad=bytearray(changed)
            bad[offset]^=1
            with self.assertRaises(ValueError):check_change(original,bad)

    def test_all_physical_phases_required(self):
        phases={'menu':MENU,'old':['one','two','three','four','menu'],
                'new':['four','three','two','one','menu'],'confirm':['four','three','two','one','menu']}
        trace='\n'.join(f'nsk1_pin_change_contact: phase={phase} index={i} key={key} down={edge} t=60.0 physical_only=1'
                        for phase,keys in phases.items() for i,key in enumerate(keys,1) for edge in (1,0))
        check_change_contacts(trace)
        for bad in (trace.replace('phase=confirm','phase=new'),trace.split('\n',1)[1],trace+'\nnsk1_pin_change_contact: malformed'):
            with self.assertRaises(ValueError):check_change_contacts(bad)


if __name__ == '__main__':unittest.main()
