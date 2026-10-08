import unittest

from tools.noki8890_calendar_check import verify


SEED = '\n'.join((
    '8890_calendar: stage=before scalar=d44b1580',
    'event=second time=23:59:59 day=0',
    'event=second time=00:00:00 day=1 status=33 mask=50',
    'event=read reg=0a data=01',
    'event=counter_write reg=0a data=00',
    '8890_calendar: stage=after scalar=d44c6700',
))
COLD = ('kind=app_write pc=003062cc address=00137420 data=d44c6700 mask=ffffffff\n'
        '8890_clock_nv_result: result=00000001 flags=00\n' +
        '\n'.join(f'8890_calendar_physical: step={index}' for index in range(1, 10)))


class CalendarTest(unittest.TestCase):
    def test_complete_chain(self):
        verify(SEED, COLD)

    def test_every_midnight_link_required(self):
        for line in SEED.splitlines():
            with self.subTest(line=line), self.assertRaises(ValueError):
                verify(SEED.replace(line, ''), COLD)

    def test_wrong_date_or_rejected_record(self):
        for cold in (COLD.replace('d44c6700', 'd44b1580'),
                     COLD.replace('result=00000001', 'result=00000000')):
            with self.subTest(cold=cold), self.assertRaises(ValueError):
                verify(SEED, cold)

    def test_no_cold_time_entry_or_fixture_failure(self):
        for extra in ('8890_clock_physical: key=Keypad 1', '[LUA ERROR] aborted'):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                verify(SEED, COLD + '\n' + extra)

    def test_physical_menu_selection_required(self):
        for cold in (COLD.replace('step=5', 'step=6'),
                     COLD.replace('8890_calendar_physical: step=9', '')):
            with self.subTest(cold=cold), self.assertRaises(ValueError):
                verify(SEED, cold)

    def test_out_of_order_rollover(self):
        lines = SEED.splitlines()
        lines[1], lines[2] = lines[2], lines[1]
        with self.assertRaises(ValueError):
            verify('\n'.join(lines), COLD)


if __name__ == '__main__':
    unittest.main()
