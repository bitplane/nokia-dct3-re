import unittest

from tools.nsm1_toolkit_tone_timing_check import check_trace
from tools.nsm1_toolkit_check import check_trace as check_protocol
from tools.sim_toolkit_interactive_check import PLAY_TONE,TONE_COMPLETE_RESPONSE
from tools.test_nsm1_toolkit_check import menu_trace,wire


def trace():
    text=menu_trace().replace('ins=c2 sw=9000','ins=c2 sw=911c')
    text+='\n'+wire('TX',bytes.fromhex('a01200001c'))+'\n'+wire('RX',PLAY_TONE+b'\x90\x00')
    text+='\nnsm1_tone_timing_entry: pc=002bd8de r0=00004190 r1=0000000a caller=00273921 t=86.010000000 observer_writes=0'
    text+='\nnsm1_tone_timing_entry: pc=00275106 r0=00000052 r1=000000fa caller=0027393d t=86.020000000 observer_writes=0'
    text+='\nnsm1_tone_timing_fiq5_ack: data=20 pc=002b95a0 t=93.001000000 observer_writes=0'
    for i,(address,value,mask,pc) in enumerate((
            ('00100514','00010000','ffff0000','0027509c'),
            ('00100510','00100528','ffffffff','002750aa'),
            ('00100518','02000000','ff000000','0027522c'),
            ('00100514','007d0000','ffff0000','002751ac'),
            ('00100510','0010015c','ffffffff','002750ac'),
            ('00100514','00000000','ffff0000','002758fa'),
            ('00100518','03000000','ff000000','00275348'),
            ('00100510','ffffffff','ffffffff','0027537a'),
            ('00100510','00100528','ffffffff','002753a2'),
            ('00100518','01000000','ff000000','00275f9e'),
            ('00100510','ffffffff','ffffffff','00275faa'))):
        when=86.03+i*.001 if i<5 else 93.01+i*.001
        text+=f'\nnsm1_tone_timing_write: address={address} value={value} mask={mask} pc={pc} t={when:.9f} observer_writes=0'
    text+='\n'+wire('TX',bytes.fromhex('a01400000c')+TONE_COMPLETE_RESPONSE)
    text+='\nSIM status ins=14 sw=9000 chv=3/3 puk=10/10 enabled=1'
    text+='\nnsm1_tone_timing_final: record=ffffffff0000050301520000 pc=47a1 idle=0 stop=0 t=93.300000000 observer_writes=0'
    text+='\nnsm1_tone_timing_exit: t=93.500000000 observer_writes=0'
    return text


class ToneTimingTest(unittest.TestCase):
    def test_organic_late_success_is_not_user_cancellation_or_the_92s_control(self):
        text=trace()
        check_trace(text)
        check_protocol(text,tone='late')
        for mode in ('hold','cancel'):
            with self.assertRaises(ValueError):
                check_protocol(text,tone=mode)
        for bad in (text.replace('data=00 ','data=11 '),
                    text+'\nnsm1_toolkit_tone: action=quit down=1'):
            with self.assertRaises(ValueError):
                check_protocol(bad,tone='late')

    def test_expiry_needs_own_timer_schedule_fiq5_consumption_and_clean_exit(self):
        text=trace()
        response=wire('TX',bytes.fromhex('a01400000c')+TONE_COMPLETE_RESPONSE)
        for bad in (text.replace('r1=000000fa','r1=00000001'),
                    text.replace('data=20 pc=002b95a0','data=10 pc=002b95a0'),
                    text.replace('pc=002758fa','pc=00274ffa'),
                    text.replace('t=93.015000000','t=92.015000000'),
                    text.replace('record=ffffffff0000050301520000','record=0010015c007d050302520000'),
                    text.replace('stop=0','stop=1'),
                    text.replace('nsm1_tone_timing_exit:','wrong_exit:'),
                    text+'\nnsm1_tone_timing_exit: malformed',
                    response+'\n'+text.replace(response,'')):
            with self.subTest(tail=bad[-90:]),self.assertRaises(ValueError):
                check_trace(bad)


if __name__=='__main__':
    unittest.main()
