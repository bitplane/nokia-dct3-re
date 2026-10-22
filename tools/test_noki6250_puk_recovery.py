import unittest
import re
import io
from unittest.mock import patch
from tools.make_sim_card_profile import make_profile
from tools.noki6250_pin_retry_check import registered_card
from tools.run_noki6250_pin_retry_cold import check_puk_phase
from tools import run_noki6250_pin_retry_cold as runner


def fixture(name):
    blocked=bytearray(make_profile(True)+bytes.fromhex('00f110')+b'\xff'*43)
    blocked[3516]=0
    rotated=bytearray(registered_card())
    rotated[3484:3492]=b'2468\xff\xff\xff\xff'
    if name=='blocked':
        initial,final=make_profile(True),bytes(blocked)
        actions=[]
        for start in (8,16,24):
            actions += [(start+i,'Keypad 9') for i in range(4)]+[(start+4,'Left Softkey / Menu')]
        prefix='6250_pin_retry'
    elif name=='puk':
        initial,final=bytes(blocked),bytes(rotated)
        actions=[(8+i,'Keypad '+str(i+1)) for i in range(8)]+[(16,'Left Softkey / Menu')]
        for group in (0,1):
            actions += [(20+group*8+i,'Keypad '+str(digit)) for i,digit in enumerate((2,4,6,8))]
            actions.append((24+group*8,'Left Softkey / Menu'))
        actions.append((52,'Left Softkey / Menu'))
        prefix='6250_puk_input'
    else:
        initial=final=bytes(rotated)
        actions=[(8+i,'Keypad '+str(digit)) for i,digit in enumerate((2,4,6,8))]
        actions += [(12,'Left Softkey / Menu'),(34,'Left Softkey / Menu')]
        prefix='6250_pin_retry'
    lines=[]
    for second,key in actions:
        lines += [f'{prefix}: key={key} state=1 t={second}.000000000',
                  f'{prefix}: key={key} state=0 t={second}.150000000']
        if name=='blocked' and second in (12,20,28):
            index=(second-12)//8
            lines += ['header cla=a0 ins=20 p1=00 p2=01 p3=08',
                      f'SIM status ins=20 sw={"9840" if index==2 else "9804"} chv={2-index}/3 puk=10/10 enabled=1']
        if name=='puk' and second==32:
            lines += ['header cla=a0 ins=2c p1=00 p2=00 p3=10',
                      'SIM status ins=2c sw=9000 chv=3/3 puk=10/10 enabled=1',
                      'LAPDm Location Updating Accept acknowledged']
        if name=='reauth' and second==12:
            lines += ['header cla=a0 ins=20 p1=00 p2=01 p3=08',
                      'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',
                      'LAPDm Location Updating Accept acknowledged']
    lines.append(prefix+': '+('wrong_only_complete' if name=='blocked' else 'complete')+'=1 firmware_state_forcing=0')
    return '\n'.join(lines),initial,final


class PukRecoveryTest(unittest.TestCase):
    def test_wrong_puk_cli_requires_full_recovery_lifecycle(self):
        with patch('sys.argv',['runner','unused','--wrong-puk-first']), \
                patch('sys.stderr',new_callable=io.StringIO) as output:
            with self.assertRaises(SystemExit) as error:
                runner.main()
        self.assertEqual(error.exception.code,2)
        self.assertIn('--wrong-puk-first requires --puk-recovery',output.getvalue())

    def test_exhaustion_cli_rejects_ambiguous_phase_plans(self):
        for arguments in (['--exhaust-puk'],
                          ['--exhaust-puk','--puk-recovery','--wrong-puk-first']):
            with self.subTest(arguments=arguments),patch('sys.argv',['runner','unused',*arguments]), \
                    patch('sys.stderr',new_callable=io.StringIO) as output:
                with self.assertRaises(SystemExit) as error:
                    runner.main()
            self.assertEqual(error.exception.code,2)
            self.assertIn('--exhaust-puk requires --puk-recovery without --wrong-puk-first',output.getvalue())

    def test_wrong_puk_preserves_pin_and_decrements_only_puk_allowance(self):
        text,initial,_=fixture('puk')
        before,after=text.split('6250_puk_input: key=Left Softkey / Menu state=1 t=16.',1)
        before=re.sub(r'key=Keypad \d','key=Keypad 9',before)
        text=before+'6250_puk_input: key=Left Softkey / Menu state=1 t=16.'+after
        text=text.replace('sw=9000 chv=3/3 puk=10/10','sw=9804 chv=0/3 puk=9/10')
        text=text.replace('LAPDm Location Updating Accept acknowledged','')
        text=re.sub(r'6250_puk_input: key=Left Softkey / Menu state=[01] t=52\.[^\n]*\n','',text)
        text=text.replace('6250_puk_input: complete=1','6250_puk_input: wrong_complete=1')
        failed=bytearray(initial)
        failed[3518]=9
        check_puk_phase(text,initial,bytes(failed),'wrong-puk')
        changed=bytearray(failed)
        changed[3484:3492]=b'2468\xff\xff\xff\xff'
        with self.assertRaises(ValueError):
            check_puk_phase(text,initial,bytes(changed),'wrong-puk')
        with self.assertRaises(ValueError):
            check_puk_phase(text.replace('puk=9/10','puk=10/10'),initial,bytes(failed),'wrong-puk')
        with self.assertRaises(ValueError):
            check_puk_phase(text+'\nLAPDm Location Updating Accept acknowledged',initial,bytes(failed),'wrong-puk')

        for allowance in range(1,11):
            source=bytearray(initial)
            source[3518]=allowance
            target=bytearray(source)
            target[3518]-=1
            outcome='9840' if allowance==1 else '9804'
            trace=text.replace('sw=9804 chv=0/3 puk=9/10',
                               f'sw={outcome} chv=0/3 puk={allowance-1}/10')
            with self.subTest(allowance=allowance):
                check_puk_phase(trace,bytes(source),bytes(target),'wrong-puk',puk_attempts=allowance)
            with self.assertRaises(ValueError):
                check_puk_phase(trace,bytes(source),bytes(source),'wrong-puk',puk_attempts=allowance)

    def test_permanent_cold_rejection_refuses_physical_credential_patterns(self):
        initial=bytearray(make_profile(True)+bytes.fromhex('00f110')+b'\xff'*43)
        initial[3516]=initial[3518]=0
        actions=[(10+i,'Keypad 9') for i in range(8)]+[(18,'Left Softkey / Menu')]
        for group in (0,1):
            actions += [(22+group*8+i,'Keypad '+str(digit)) for i,digit in enumerate((2,4,6,8))]
            actions.append((26+group*8,'Left Softkey / Menu'))
        actions.append((38,'End'))
        lines=[]
        for second,key in actions:
            lines += [f'6250_puk_input: key={key} state=1 t={second}.000000000',
                      f'6250_puk_input: key={key} state=0 t={second}.150000000']
        lines.append('6250_puk_input: permanent_complete=1 firmware_state_forcing=0')
        text='\n'.join(lines)
        check_puk_phase(text,bytes(initial),bytes(initial),'permanent')
        for event in ('SIM status ins=2c sw=9840 chv=0/3 puk=0/10 enabled=1',
                      'SIM status ins=20 sw=9840 chv=0/3 puk=0/10 enabled=1',
                      'LAPDm Location Updating Accept acknowledged','GSM outgoing request'):
            with self.subTest(event=event),self.assertRaises(ValueError):
                check_puk_phase(text+'\n'+event,bytes(initial),bytes(initial),'permanent')
        changed=bytearray(initial)
        changed[3518]=1
        with self.assertRaises(ValueError):
            check_puk_phase(text,bytes(initial),bytes(changed),'permanent')

    def test_recovery_inherits_nine_puk_attempts_and_resets_them(self):
        text,initial,final=fixture('puk')
        failed=bytearray(initial)
        failed[3518]=9
        check_puk_phase(text,bytes(failed),final,'puk',puk_attempts=9)
        with self.assertRaises(ValueError):
            check_puk_phase(text,initial,final,'puk',puk_attempts=9)
        with self.assertRaises(ValueError):
            check_puk_phase(text,bytes(failed),final,'puk')
        changed=bytearray(final)
        changed[3518]=9
        with self.assertRaises(ValueError):
            check_puk_phase(text,bytes(failed),bytes(changed),'puk',puk_attempts=9)

    def test_three_qualified_phase_contracts(self):
        for name in ('blocked','puk','reauth'):
            check_puk_phase(*fixture(name),name)

    def test_complete_card_transitions_are_required(self):
        for name in ('blocked','puk','reauth'):
            text,initial,final=fixture(name)
            for offset in (0,3484,3516,3517,3518,len(final)-1):
                changed=bytearray(final)
                changed[offset]^=1
                with self.subTest(name=name,offset=offset),self.assertRaises(ValueError):
                    check_puk_phase(text,initial,bytes(changed),name)
            with self.assertRaises(ValueError):
                check_puk_phase(text,initial[:-1],final,name)

    def test_wrong_selectors_results_and_contacts_fail(self):
        for name in ('blocked','puk','reauth'):
            text,initial,final=fixture(name)
            changes=[('p2=00 p3=10','p2=02 p3=10')] if name=='puk' else [('p2=01 p3=08','p2=02 p3=08')]
            changes += [('chv=3/3','chv=2/3')] if name!='blocked' else [('chv=0/3','chv=1/3')]
            changes += [('state=0 t=8.','state=1 t=8.'),('complete=1','complete=0')]
            for old,new in changes:
                with self.subTest(name=name,old=old),self.assertRaises(ValueError):
                    check_puk_phase(text.replace(old,new),initial,final,name)
            with self.assertRaises(ValueError):
                check_puk_phase(text+'\nSIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=1',initial,final,name)

    def test_unknown_phase_is_not_reauthentication(self):
        with self.assertRaises(ValueError):
            check_puk_phase(*fixture('reauth'),'unknown')


if __name__=='__main__':unittest.main()
