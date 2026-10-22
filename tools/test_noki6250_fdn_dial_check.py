import unittest
from pathlib import Path
from unittest.mock import patch
from tools.noki6250_fdn_dial_check import actions,check,check_frames,expected_card
from tools.noki6250_call_check import verify_outgoing


START="""GSM service establish sapi=0 pd=05 message=24 length=16 data=05247103331981080910101032547698
GSM service downlink kind=5 sapi=0 pd=05 message=21 length=2
GSM service uplink sapi=0 pd=03 message=05 length=17 data=03450404600200815e04812143f5150101
GSM service downlink kind=12 sapi=0 pd=03 message=02 length=2
GSM service downlink kind=15 sapi=0 pd=06 message=2e length=8
TX packet type=02 payload=20 radio_phase=traffic_channel_change data=041202000271012fc10000130000000400000000
GSM service uplink sapi=0 pd=06 message=29 length=3
GSM service downlink kind=13 sapi=0 pd=03 message=01 length=2
GSM service downlink kind=14 sapi=0 pd=03 message=07 length=2
GSM service uplink sapi=0 pd=03 message=0f length=2"""
END="""GSM service uplink sapi=0 pd=03 message=25 length=5 data=036502e090
GSM service downlink kind=20 sapi=0 pd=03 message=2d length=2
GSM service uplink sapi=0 pd=03 message=2a length=2
LAPDm service Channel Release acknowledged nr=4
TX packet type=02 payload=20 radio_phase=release_channel_change data=041202001117001a600000130000001400000001
RX enqueue type=89 payload=8 data=0000000000000000
6250_channel_confirmation: body=00 input=0409 expected=00 pending=00
RX enqueue type=80 payload=34 data=600000000a4d000100001506210001f0"""


def fixture(mode):
    lines=[]
    for second,key in actions(mode):
        lines += [f'6250_fdn_dial: key={key} state=1 t={second}.000000000',
                  f'6250_fdn_dial: key={key} state=0 t={second}.150000000']
        if second==12:
            lines += ['header cla=a0 ins=20 p1=00 p2=01 p3=08',
                      'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1']
            if mode!='off':
                for fid in ('6f7e','6f07'):
                    lines += [f'header cla=a0 ins=44 p1=00 p2=00 p3=00 selected={fid}',
                              'SIM status ins=44 sw=9000 chv=3/3 puk=10/10 enabled=1']
            lines += [f'header cla=a0 ins=b2 p1={index:02x} p2=04 p3=20 selected=6f3b' for index in range(1,11)]
            lines.append('LAPDm Location Updating Accept acknowledged')
        if mode!='denied' and second==26:
            lines.append(START.replace('2143f5','2143f6') if mode=='off' else START)
        if mode!='denied' and second==38:lines.append(END)
    lines.append('6250_fdn_dial: complete=1 firmware_state_forcing=0')
    card=expected_card(mode)
    return '\n'.join(lines),card,card


class FdnDialTest(unittest.TestCase):
    def test_listed_denied_and_physically_off_controls(self):
        for mode in ('listed','denied','off'):
            with self.subTest(mode=mode):check(*fixture(mode),mode)

    def test_permission_requires_real_called_digits_and_complete_release(self):
        text,initial,final=fixture('listed')
        for before,after in (('2143f5','2143f6'),('message=0f','message=00'),
                              ('6250_channel_confirmation:','removed_confirmation:'),
                              ('001300000004','000100000004'),('complete=1','complete=0')):
            with self.subTest(before=before),self.assertRaises(ValueError):
                check(text.replace(before,after),initial,final,'listed')
        with self.assertRaises(ValueError):verify_outgoing(text,'12345',configured_carrier=True)

    def test_denial_fails_if_any_call_setup_leaks(self):
        text,initial,final=fixture('denied')
        for extra in (START, 'GSM service uplink sapi=0 pd=03 message=25 length=5'):
            with self.subTest(extra=extra),self.assertRaises(ValueError):
                check(text+'\n'+extra,initial,final,'denied')

    def test_dialing_preserves_full_card_and_does_not_change_credentials(self):
        for mode in ('listed','denied','off'):
            text,initial,final=fixture(mode)
            for offset in (0,1604,3484,3492,3516,3517,3518,3519,3575,3578,3600,len(final)-1):
                changed=bytearray(final)
                changed[offset]^=1
                with self.subTest(mode=mode,offset=offset),self.assertRaises(ValueError):
                    check(text,initial,bytes(changed),mode)
            with self.assertRaises(ValueError):
                check(text+'\nheader cla=a0 ins=20 p1=00 p2=02 p3=08',initial,final,mode)
        text,initial,final=fixture('listed')
        with self.assertRaises(ValueError):
            check(text.replace('key=Send state=0 t=26.','key=Send state=1 t=26.'),initial,final,'listed')

    def test_three_own_frames_per_control(self):
        for mode in ('listed','denied','off'):
            with patch('tools.noki6250_fdn_dial_check.check_rgb_frame') as renderer:
                check_frames(Path('/capture'),mode)
            self.assertEqual(len(renderer.call_args_list),3)
        with self.assertRaises(ValueError):actions('unknown')


if __name__=='__main__':unittest.main()
