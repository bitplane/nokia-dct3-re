import unittest
from tools.noki6210_fdn_dial_check import check,input_contacts
from tools.noki6210_fdn_record_check import expected_card


def fixture(mode):
    digits='12345' if mode=='listed' else '12346'
    lines=[]
    for second,key in input_contacts(digits):
        lines.append(f'6210_fdn_dial: key={key} state=1 t={second}.000000000')
        if key in ('Send','End'):
            lines.append('6210_call_physical: action='+('send' if key=='Send' else 'end'))
        code={'Send':0x0e,'End':0x0f,'Left Softkey / Menu':0x19}.get(key)
        if code is None:
            code=int(key[-1])
        lines.append(f'6210_keypad_decoded: key={code:02x}')
        if second==12:
            lines.append('SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1')
            if mode!='off':
                for fid in ('6f7e','6f07'):
                    lines += [f'header cla=a0 ins=44 p1=00 p2=00 p3=00 selected={fid}',
                              'SIM status ins=44 sw=9000 chv=3/3 puk=10/10 enabled=1']
            lines.append('LAPDm Location Updating Accept acknowledged nr=1')
            lines += [f'header cla=a0 ins=b2 p1={i:02x} p2=04 p3=20 selected=6f3b' for i in range(1,11)]
        if second==26 and mode!='denied':
            lines += [
                'GSM service establish sapi=0 pd=05 message=24 length=16 data=05247103331981080910101032547698',
                'GSM service downlink kind=5 sapi=0 pd=05 message=21 length=2',
                f'GSM service uplink sapi=0 pd=03 message=05 length=17 data=03450404600200815e04812143f{digits[-1]}150101',
                'GSM service downlink kind=10 sapi=0 pd=03 message=02 length=2',
                'GSM service downlink kind=14 sapi=0 pd=06 message=2e length=8',
                'TX packet type=02 payload=24 data=041202000271012fc1000023000000040000000000000000',
                'GSM service uplink sapi=0 pd=06 message=29 length=3',
                'GSM service downlink kind=11 sapi=0 pd=03 message=01 length=2',
                'GSM service downlink kind=12 sapi=0 pd=03 message=07 length=2',
                'GSM service uplink sapi=0 pd=03 message=0f length=2',
            ]
        if second==38 and mode!='denied':
            lines += [
                'GSM service uplink sapi=0 pd=03 message=25 length=5',
                'GSM service downlink kind=25 sapi=0 pd=03 message=2d length=2',
                'GSM service uplink sapi=0 pd=03 message=2a length=2',
                'LAPDm service Channel Release acknowledged nr=4',
                'TX packet type=02 payload=24 data=041202001117001a60000023000000140000000100000000',
                'RX enqueue type=89 payload=8 data=0000000000000000',
                'PCH no-identity fill',
            ]
        lines.append(f'6210_fdn_dial: key={key} state=0 t={second}.150000000')
    lines.append('6210_fdn_dial: complete=1 firmware_state_forcing=0')
    card=bytearray(expected_card(True))
    if mode=='off':
        card[3575]=1
    return '\n'.join(lines),bytes(card),bytes(card)


class DialPermissionTests(unittest.TestCase):
    def test_listed_unlisted_and_off(self):
        for mode in ('listed','denied','off'):
            with self.subTest(mode=mode):
                check(*fixture(mode),mode)

    def test_denial_requires_no_network_call(self):
        text,initial,final=fixture('denied')
        for extra in ('GSM service establish sapi=0 pd=05 message=24 length=1 data=00',
                      'GSM service uplink sapi=0 pd=03 message=05 length=1 data=00',
                      'GSM service downlink kind=1 sapi=0 pd=06 message=2e length=8',
                      'GSM service uplink sapi=0 pd=03 message=25 length=5'):
            with self.subTest(extra=extra),self.assertRaises(ValueError):
                check(text+'\n'+extra,initial,final,'denied')

    def test_calls_require_real_contact_decode_number_and_release(self):
        text,initial,final=fixture('listed')
        for old,new in (('key=0e','key=0f'),('key=Keypad 5 state=1','key=Keypad 6 state=1'),
                        ('812143f5','812143f6'),('pd=03 message=0f length=2','missing'),
                        ('pd=03 message=2a length=2','missing')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(text.replace(old,new),initial,final,'listed')

    def test_card_and_control_state_are_exact(self):
        for mode in ('listed','denied','off'):
            text,initial,final=fixture(mode)
            for index in (3484,3517,3575,3578,3600):
                changed=bytearray(final)
                changed[index]^=1
                with self.subTest(mode=mode,index=index),self.assertRaises(ValueError):
                    check(text,initial,bytes(changed),mode)
            with self.assertRaises(ValueError):
                check(text,initial[:-1],final,mode)


if __name__=='__main__':
    unittest.main()
