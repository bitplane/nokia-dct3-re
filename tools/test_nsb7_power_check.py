import unittest
from unittest.mock import patch
from tools.nsb7_power_check import check


def fixture():
    events=[(44,'nsb7_power: ready t=44.000000000'),
            (51.442,'ccont_power: event=off t=51.442'),
            (55,'nsb7_radio_capture: reset_cursor t=55.000000000 observer_writes=0'),
            (65.017,'ccont_power: event=wake cause=02 t=65.017'),
            (65.02,'ccont_power: event=cause_read data=33'),
            (77.01,'SIM status ins=20 sw=9000'),
            (78,'nsb7_radio_tx: index=106 type=1b length=28 data=0080013f4905087200f1100001200809101010325476982b2b000000 cursor=3e producer=4d pc=002cb0ec t=78.0 observer_writes=0'),
            (78.01,'GSM service establish sapi=0 pd=05 message=08 length=18 data=05087200f110000120080910101032547698'),
            (78.1,'LAPDm Location Updating Accept acknowledged'),
            (86.01,'nsb7_security_retry: match=1 retries=00'),
            (101.001,'nsb7_power: done t=101.000000000'),
            (1.01,'ccont_analog: event=control_write reg=07 data=00')]
    steps=[(45,'Power',4),(65,'Power',2)]
    for start,digits in ((73,'1234'),(81,'12345')):
        steps += [(start+i,'Keypad '+d,0.2) for i,d in enumerate(digits)]
        steps.append((start+len(digits),'Left Softkey / Menu',0.2))
    steps += [(95,'Left Softkey / Menu',0.2),(98,'End',0.2)]
    for time,key,duration in steps:
        events += [(time,f'nsb7_power_input: key={key} state=1 t={time:.9f}'),
                   (time+duration,f'nsb7_power_input: key={key} state=0 t={time+duration:.9f}')]
        if key!='Power':
            code='19' if key=='Left Softkey / Menu' else '0f' if key=='End' else f'{int(key[-1]):02x}'
            events.append((time+0.005,f'nsb7_key_decode: key={code} observer_writes=0'))
    for time in range(1,102):
        hour,second=divmod(13*3600+47*60+45+time,3600)
        minute,second=divmod(second,60)
        events.append((time,f'ccont_rtc: event=second time={hour:02d}:{minute:02d}:{second:02d} day=0 t={time:.9f}'))
    for index,value in enumerate(bytes.fromhex('a02000010831323334ffffffff')):
        events.append((77.005,f'nsb7_sim_tx: index={index} value={value:02x}'))
    return '\n'.join(text for _,text in sorted(events,key=lambda item:item[0]))


class PowerTest(unittest.TestCase):
    @patch('tools.nsb7_power_check.check_location_update')
    @patch('tools.nsb7_power_check.check_authentication')
    def test_physical_retained_power_contract(self,authentication,registration):
        check(fixture())
        authentication.assert_called_once()
        self.assertEqual(registration.call_args.kwargs['first_ack'],107)

    @patch('tools.nsb7_power_check.check_location_update')
    @patch('tools.nsb7_power_check.check_authentication')
    def test_corruption_rejected(self,authentication,registration):
        text=fixture()
        for bad in (text.replace('event=wake cause=02','event=wake cause=04'),
                    text.replace('time=13:48:45','time=13:48:44'),
                    text.replace('observer_writes=0','observer_writes=1'),
                    text.replace('key=Keypad 4 state=1 t=76.','key=Keypad 3 state=1 t=76.'),
                    text.replace('SIM status ins=20 sw=9000','SIM status ins=20 sw=9804'),
                    text.replace('ccont_power: event=wake',
                                 'dsp_hle: TX packet type=02\nccont_power: event=wake'),
                    text.replace('match=1 retries=00','match=0 retries=01'),
                    text.replace('nsb7_power: done','missing done')):
            with self.subTest(bad=bad),self.assertRaises(ValueError): check(bad)
