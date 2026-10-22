import unittest

from tools.noki6210_pin2_change_check import check
from tools.noki6210_puk_check import profile


def fixture():
    items=[(7+i,'Keypad '+str(i)) for i in range(1,5)]+[(12,'Left Softkey / Menu')]
    items += [(20,'Left Softkey / Menu')]+[(22+i,'Scroll Down') for i in range(3)]+[(26,'Left Softkey / Menu')]
    items += [(28+i,'Scroll Down') for i in range(5)]+[(34,'Left Softkey / Menu')]
    items += [(36+i,'Scroll Down') for i in range(4)]+[(42,'Left Softkey / Menu')]
    items += [(44+i,'Scroll Down') for i in range(2)]+[(47,'Left Softkey / Menu')]
    for start,digits in ((50,'5678'),(58,'8765'),(66,'8765')):
        items.extend((start+i,'Keypad '+d) for i,d in enumerate(digits))
        items.append((start+4,'Left Softkey / Menu'))
    items += [(76,'End'),(78,'Left Softkey / Menu')]
    lines=[]
    for second,key in items:
        lines.extend([f'6210_pin2_input: key={key} state=1 t={second}.000000000',
                      f'6210_pin2_input: key={key} state=0 t={second}.150000000'])
        if second==12:
            lines.extend(['SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',
                          'LAPDm Location Updating Accept acknowledged nr=1'])
        if second==70:
            lines.extend(['sim_device: header cla=a0 ins=24 p1=00 p2=02 p3=10',
                          'SIM status ins=24 sw=9000 chv=3/3 puk=10/10 enabled=1'])
    lines.append('6210_pin2_input: complete=1 firmware_state_forcing=0')
    card=bytearray(profile(3,True))
    card[3492:3500]=b'8765\xff\xff\xff\xff'
    return '\n'.join(lines),bytes(card)


class Pin2ChangeTests(unittest.TestCase):
    def test_cold_installed_pin2_is_used_to_change_back(self):
        text,_=fixture()
        # Change the actual keyed old/new phases, not the rest of the workflow.
        import re
        def rotate(match):
            digit,state,second,fraction=match.groups()
            time=int(second)
            digits='8765' if 50<=time<=53 else '5678' if 58<=time<=61 or 66<=time<=69 else None
            if digits is None:
                return match[0]
            start=50 if time<=53 else 58 if time<=61 else 66
            return f'key=Keypad {digits[time-start]} state={state} t={second}.{fraction}'
        text=re.sub(r'key=Keypad (\d) state=([01]) t=(\d+)\.(\d+)',rotate,text)
        card=profile(3,True)
        check(text,card,'8765','5678')
        with self.assertRaises(ValueError):
            check(text,card)

    def test_physical_change(self):
        check(*fixture())

    def test_selector_input_or_status_drift_fails(self):
        text,card=fixture()
        for old,new in [('p2=02','p2=01'),('ins=24 sw=9000','ins=24 sw=9804'),
                        ('t=70.000000000','t=71.000000000'),('key=Keypad 8','key=Keypad 9'),
                        ('complete=1','complete=0')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(text.replace(old,new),card)

    def test_complete_card_preserves_other_credentials(self):
        text,original=fixture()
        for index in (0,1604,3484,3492,3500,3516,3517,3518,3520,3569):
            card=bytearray(original)
            card[index]^=1
            with self.subTest(index=index),self.assertRaises(ValueError):
                check(text,bytes(card))


if __name__=='__main__':
    unittest.main()
