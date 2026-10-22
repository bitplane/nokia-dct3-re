import unittest
import re

from tools.noki6210_puk_check import check_blocked,check_unblocked,check_reauthenticated,check_wrong_puk,check_permanent,check_permanent_retry,profile


def keys(prefix,items):
    return ''.join(f'{prefix}: key={key} state={state} t={second}.{fraction}\n'
                   for second,key in items for state,fraction in ((1,'000000000'),(0,'150000000')))


def blocked():
    text=''
    for attempt,start in enumerate((8,16,24)):
        text+=keys('6210_pin_retry_input',[(start+i,'Keypad 9') for i in range(4)]+
                   [(start+4,'Left Softkey / Menu')])
        text+=f'SIM status ins=20 sw={"9840" if attempt==2 else "9804"} chv={2-attempt}/3 puk=10/10 enabled=1\n'
    return text+'6210_pin_retry_input: wrong_complete=1 firmware_state_forcing=0\n'


def recovered():
    items=[(7+i,'Keypad '+str(i)) for i in range(1,9)]+[(16,'Left Softkey / Menu')]
    for start in (20,28):
        items.extend((start+i,'Keypad '+str(d)) for i,d in enumerate((2,4,6,8)))
        items.append((start+4,'Left Softkey / Menu'))
    return (keys('6210_puk_input',items)+
            'SIM status ins=2c sw=9000 chv=3/3 puk=10/10 enabled=1\n'
            'LAPDm Location Updating Accept acknowledged nr=1\n'+
            keys('6210_puk_input',[(52,'Left Softkey / Menu')])+
            '6210_puk_input: complete=1 firmware_state_forcing=0\n')


def reauthenticated():
    return (keys('6210_puk_input',[(8+i,'Keypad '+str(d)) for i,d in enumerate((2,4,6,8))]+
                 [(12,'Left Softkey / Menu')])+
            'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1\n'
            'LAPDm Location Updating Accept acknowledged nr=1\n'+
            keys('6210_puk_input',[(34,'Left Softkey / Menu')])+
            '6210_puk_input: reauthenticated=1 firmware_state_forcing=0\n')


class PukTests(unittest.TestCase):
    def test_permanent_physical_input_is_not_credential_recovery(self):
        text=recovered()
        text=re.sub(r'SIM status ins=2c[^\n]*\n','',text)
        text=text.replace('LAPDm Location Updating Accept acknowledged nr=1\n','')
        text=text.replace(keys('6210_puk_input',[(52,'Left Softkey / Menu')]),keys('6210_puk_input',[(36,'End')]))
        text=text.replace('complete=1','permanent_complete=1')
        card=profile(puk_attempts=0)
        check_permanent_retry(text,card,card)
        for extra in ('\nSIM status ins=2c sw=9840 chv=0/3 puk=0/10 enabled=1',
                      '\nGSM service establish sapi=0 pd=05 message=24 ',
                      '\nGSM service uplink sapi=0 pd=03 message=05 '):
            with self.subTest(extra=extra),self.assertRaises(ValueError):
                check_permanent_retry(text+extra,card,card)
        with self.assertRaises(ValueError):
            check_permanent_retry(text.replace('key=End','key=Send'),card,card)

    def test_all_ten_wrong_puk_transitions_and_permanent_rejection(self):
        text=recovered()
        text=re.sub(r'key=Keypad [1-8]( state=[01] t=(?:8|9|1[0-5])\.)',r'key=Keypad 9\1',text)
        text=text.replace('LAPDm Location Updating Accept acknowledged nr=1\n','')
        text=re.sub(r'6210_puk_input: key=Left Softkey / Menu state=[01] t=52\.[0-9]+\n','',text)
        text=text.replace('complete=1','wrong_complete=1')
        for attempt in range(1,11):
            status=f'ins=2c sw={"9840" if attempt==10 else "9804"} chv=0/3 puk={10-attempt}/10'
            current=text.replace('ins=2c sw=9000 chv=3/3 puk=10/10',status)
            check_wrong_puk(current,profile(puk_attempts=11-attempt),profile(puk_attempts=10-attempt),attempt)
            with self.subTest(attempt=attempt),self.assertRaises(ValueError):
                check_wrong_puk(current,profile(puk_attempts=11-attempt),profile(puk_attempts=11-attempt),attempt)
        permanent='6210_puk_permanent: complete=1 credential_input=0 firmware_state_forcing=0'
        card=profile(puk_attempts=0)
        check_permanent(permanent,card,card)
        for extra in ('\nSIM status ins=2c sw=9840 chv=0/3 puk=0/10 enabled=1',
                      '\nLAPDm Location Updating Accept acknowledged nr=1'):
            with self.assertRaises(ValueError):
                check_permanent(permanent+extra,card,card)
        with self.assertRaises(ValueError):
            check_permanent(permanent,card,profile(puk_attempts=1))

    def test_wrong_puk_preserves_credentials_and_cold_recovery(self):
        text=recovered()
        text=re.sub(r'key=Keypad [1-8]( state=[01] t=(?:8|9|1[0-5])\.)',r'key=Keypad 9\1',text)
        text=text.replace('ins=2c sw=9000 chv=3/3 puk=10/10','ins=2c sw=9804 chv=0/3 puk=9/10')
        text=text.replace('LAPDm Location Updating Accept acknowledged nr=1\n','')
        text=re.sub(r'6210_puk_input: key=Left Softkey / Menu state=[01] t=52\.[0-9]+\n','',text)
        text=text.replace('complete=1','wrong_complete=1')
        failed=profile(puk_attempts=9)
        check_wrong_puk(text,profile(),failed)
        check_unblocked(recovered(),failed,profile(3,True,b'2468\xff\xff\xff\xff'),puk_attempts=9)
        for old,new in [('puk=9/10','puk=8/10'),('chv=0/3','chv=3/3'),
                        ('sw=9804','sw=9000'),('t=32.000000000','t=33.000000000')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                check_wrong_puk(text.replace(old,new),profile(),failed)
        for index in (0,3484,3492,3500,3516,3518,3520,3569):
            bad=bytearray(failed)
            bad[index]^=1
            with self.subTest(index=index),self.assertRaises(ValueError):
                check_wrong_puk(text,profile(),bytes(bad))

    def test_complete_card_transitions(self):
        check_blocked(blocked(),profile())
        final=profile(3,True,b'2468\xff\xff\xff\xff')
        check_unblocked(recovered(),profile(),final)
        check_reauthenticated(reauthenticated(),final,final)

    def test_missing_attempt_or_physical_input_fails(self):
        for old,new in [('sw=9840','sw=9804'),('chv=1/3','chv=2/3'),
                        ('key=Keypad 9','key=Keypad 8'),('wrong_complete=1','wrong_complete=0')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                check_blocked(blocked().replace(old,new),profile())

    def test_puk_requires_confirmation_and_real_unblock(self):
        final=profile(3,True,b'2468\xff\xff\xff\xff')
        for old,new in [('t=32.000000000','t=33.000000000'),('ins=2c','ins=20'),
                        ('sw=9000','sw=9804'),('chv=3/3','chv=2/3'),
                        ('Location Updating Accept acknowledged','missing'),('complete=1','complete=0')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                check_unblocked(recovered().replace(old,new),profile(),final)

    def test_whole_card_and_rotated_pin_are_protected(self):
        final=profile(3,True,b'2468\xff\xff\xff\xff')
        for index in (0,1604,1700,3484,3492,3500,3516,3517,3518,3520,3569):
            bad=bytearray(final)
            bad[index]^=1
            with self.subTest(index=index),self.assertRaises(ValueError):
                check_unblocked(recovered(),profile(),bytes(bad))
            with self.subTest(cold_index=index),self.assertRaises(ValueError):
                check_reauthenticated(reauthenticated(),bytes(bad),bytes(bad))
        with self.assertRaises(ValueError):
            check_reauthenticated(reauthenticated().replace('key=Keypad 2','key=Keypad 1'),final,final)


if __name__=='__main__':
    unittest.main()
