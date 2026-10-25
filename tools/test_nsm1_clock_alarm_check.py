from unittest.mock import patch
import unittest
from pathlib import Path
import json
import tempfile

from tools import nsm1_clock_alarm_check as checker


def fixture(control=False):
    rows=['read-binary fid=6fae offset=0 length=1 first=02\n']
    contacts=checker.CONTACTS+([] if control else [(105,'Left Softkey',0x19)])
    for time,name,key in contacts:
        for down in (1,0):
            rows.append(f'nsm1_clock_contact: name={name} key={key:02x} down={down} t={time+(0 if down else .2):.9f} physical_only=1\n')
    for reg,data,time in (('0b','22',38.9),('0c','8c',38.91),('0b','23',59.03),('0c','0c',59.04)):
        rows.append(f'ccont_rtc: event=alarm_write reg={reg} data={data} armed=1 t={time:.9f}\n')
    rows += ['ccont_rtc: event=second time=12:35:00 day=0 status=b3 mask=50 t=98.000000000\n',
             'ccont_rtc: event=status_ack data=a0 old=b3 t=98.000247154\n',
             'nsm1_clock_native: pc=47a1 stop=0 t=111.500000000 observer_writes=0\n']
    times=[98.1+i*.185 for i in range(20)]+(list(range(106,112)) if control else [])
    rows += [f'buzzer: enabled=1 divider=5285 frequency=2459 volume=3 t={t:.6f}\n' for t in times]
    return ''.join(rows)


class ClockAlarmTest(unittest.TestCase):
    def test_matched_pair_rejects_different_or_missing_executable_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            run=Path(directory)/'stop'; run.mkdir()
            control=Path(directory)/'control'; control.mkdir()
            (run/'invocation.json').write_text(json.dumps({'mame_sha256':'a'*64}))
            for identity in ('a'*64,'b'*64,None,'invalid'):
                (control/'invocation.json').write_text(json.dumps({'mame_sha256':identity}))
                with patch.object(checker,'check',return_value={}):
                    if identity=='a'*64:
                        self.assertTrue(checker.check_pair(Path('seed'),run,control)['matched_no_stop_control'])
                    else:
                        with self.assertRaises(ValueError):
                            checker.check_pair(Path('seed'),run,control)

    def check(self,text,control=False):
        # Authentication and APDU parsers have independent fixtures; this bank
        # isolates contact/RTC/buzzer ordering and deliberately malformed data.
        with patch.object(checker,'check_pin'),patch.object(checker,'check_phone'),\
                patch.object(checker,'wire',return_value=b'wire'),\
                patch.object(checker,'apdus',return_value=[(0xa4,)]):
            return checker.check_trace(text,control=control)

    def test_physical_stop_and_matched_no_stop_are_distinct(self):
        self.assertTrue(self.check(fixture())['physical_stop'])
        self.assertFalse(self.check(fixture(True),True)['physical_stop'])
        with self.assertRaises(ValueError):
            self.check(fixture(),True)
        with self.assertRaises(ValueError):
            self.check(fixture(True))

    def test_missing_ack_wrong_latch_and_extra_contact_fail(self):
        good=fixture()
        for bad in (good.replace('data=a0 old=b3','data=20 old=b3'),
                    good.replace('reg=0c data=8c','reg=0c data=0c'),
                    good.replace('reg=0b data=23','reg=0b data=24'),
                    good.replace('t=105.000000000','t=106.000000000'),
                    good.replace('stop=0','stop=1'),
                    good+'nsm1_clock_contact: malformed\n',
                    good+'LUA ERROR',good+'nsm1_restart_request: reason=68'):
            with self.subTest(tail=bad[-80:]),self.assertRaises(ValueError):
                self.check(bad)

    def test_continued_ringing_after_stop_or_silent_control_fail(self):
        pulse='buzzer: enabled=1 divider=5285 frequency=2459 volume=3 t=110.000000\n'
        with self.assertRaises(ValueError):
            self.check(fixture()+pulse)
        silent=fixture(True).split('t=106.000000')[0]
        with self.assertRaises(ValueError):
            self.check(silent,True)


if __name__=='__main__':
    unittest.main()
