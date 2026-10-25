import unittest
from unittest.mock import patch

from tools import nsm1_factory_reset_check as checker


def trace():
    cells = ((0x19,1,2),(4,2,4),(0x19,1,2),(6,4,4),(0x19,1,2),
             (1,2,2),(2,3,2),(3,4,2),(4,2,4),(5,3,4),(0x19,1,2))
    rows = ['nsm1_security_accepted: result=00000001 observer_writes=0']
    for i,(key,column,mask) in enumerate(cells,1):
        for down in (1,0):
            rows.append(f'nsm1_factory_contact: index={i} key={key:02x} column={column} mask={mask:02x} down={down} t={25+i*.5+(1-down)*.2:.9f} physical_only=1')
    rows.append('nsm1_security_accepted: result=00000001 observer_writes=0')
    rows.extend(f'nsm1_factory_initializer: caller={caller} cycles={i:016X} observer_writes=0'
                for i,caller in enumerate(('29d457','29dc5f'),1))
    return '\n'.join(rows)


class FactoryResetTraceTest(unittest.TestCase):
    def check(self, text):
        # Authentication helpers have their own fixtures; exercise reset ordering here.
        with patch.object(checker,'check_pin'), patch.object(checker,'check_phone'):
            checker.check_trace(text)

    def test_physical_contacts_and_both_initializer_callers_required(self):
        self.check(trace())

    def test_missing_changed_or_duplicate_evidence_is_rejected(self):
        good = trace()
        for bad in (good.replace('key=06','key=04'),
                    good.replace('mask=04','mask=02'),
                    good.replace('caller=29dc5f','caller=29d457'),
                    good.replace('physical_only=1','physical_only=0'),
                    good.replace('t=25.500000000','t=45.500000000'),
                    good+'\nnsm1_factory_contact: malformed',
                    good+'\nnsm1_factory_initializer: malformed',
                    good+'\nnsm1_security_accepted: result=00000001'):
            with self.subTest(tail=bad[-80:]),self.assertRaises(ValueError):
                self.check(bad)


if __name__ == '__main__':
    unittest.main()
