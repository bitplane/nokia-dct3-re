import unittest
from tools.nsk3_call_input_check import contacts


class PhysicalCallInputTest(unittest.TestCase):
    def trace(self):
        lines=['nsk3_security_accepted: result=00000001 cycles=10 observer_writes=0']
        for key,down,time in (('0e',1,22),('0e',0,22.2),('0f',1,30),('0f',0,30.2)):
            lines.append(f'nsk3_radio_contact: key={key} down={down} t={time} firmware_writes=0')
        lines.extend(f'nsk3_logical_key: site=002ac2fc key={key} cycles=20 observer_writes=0' for key in ('0e','0f'))
        return '\n'.join(lines)

    def test_contacts_and_control(self):
        contacts(self.trace(),True)
        contacts('nsk3_security_accepted: result=00000001 cycles=10 observer_writes=0',False)

    def test_bad_contact_or_missing_decode(self):
        for old,new in (('t=22.2','t=29.2'),('key=0f','key=10'),
                        ('site=002ac2fc','site=002ac2fe'),('result=00000001','result=00000000')):
            with self.subTest(old=old),self.assertRaises(ValueError):contacts(self.trace().replace(old,new),True)
        with self.assertRaises(ValueError):contacts(self.trace(),False)


if __name__=='__main__':unittest.main()
