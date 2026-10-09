import unittest

from tools.noki8890_calendar_check import SCALARS, verify, verify_restore


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


def restoration_fixture():
    cpu = ','.join(['00000001'] * 37)
    lines = []
    for event, time, date in (('saved', 80, 'd44b1580'), ('reference', 100, 'd44c6700'),
                              ('restored', 80, 'd44b1580'), ('replayed', 100, 'd44c6700')):
        lines.append(f'8890_calendar_restore: event={event} t={time:.9f} '
                     f'pc=0000001c sp=00137b6c ram=12345678 cpu={cpu} date={date}')
        if event in ('saved', 'restored'):
            lines.extend(SEED.splitlines()[2:5])
    lines.append('8890_calendar_restore: result=pass elapsed=20 native_speech=0')
    return '\n'.join(lines)


class CalendarRestorationTest(unittest.TestCase):
    def test_exact_replay(self):
        verify_restore(restoration_fixture())

    def test_each_state_observation_required(self):
        text = restoration_fixture()
        for line in text.splitlines():
            with self.subTest(line=line), self.assertRaises(ValueError):
                verify_restore(text.replace(line, '', 1))

    def test_changed_architecture_rejected(self):
        text = restoration_fixture()
        for old, new in (('ram=12345678', 'ram=12345679'),
                         ('pc=0000001c', 'pc=00000020'),
                         ('cpu=00000001', 'cpu=00000002'),
                         ('date=d44b1580', 'date=d44c6700')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify_restore(text.replace(old, new, 1))

    def test_incomplete_architecture_rejected(self):
        with self.assertRaises(ValueError):
            verify_restore(restoration_fixture().replace('cpu=' + ','.join(['00000001'] * 37),
                                                        'cpu=00000001'))

    def test_duplicate_or_runtime_failure_rejected(self):
        text = restoration_fixture()
        for extra in (text.splitlines()[0], '[LUA ERROR] failed',
                      '8890_calendar_restore: FAIL incomplete'):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                verify_restore(text + '\n' + extra)


class CalendarTest(unittest.TestCase):
    def test_complete_chain(self):
        verify(SEED, COLD)

    def test_each_boundary_has_exact_one_day_and_cold_identity(self):
        for boundary, (before, after) in SCALARS.items():
            seed = SEED.replace('d44b1580', before).replace('d44c6700', after)
            cold = COLD.replace('d44c6700', after)
            with self.subTest(boundary=boundary):
                self.assertEqual(int(after, 16) - int(before, 16), 86400)
                verify(seed, cold, boundary=boundary)
                with self.assertRaises(ValueError):
                    verify(seed, cold.replace(after, before), boundary=boundary)

    def test_leap_day_does_not_accept_ordinary_date(self):
        for boundary in ('leap-day', 'year-end', 'non-leap'):
            with self.subTest(boundary=boundary), self.assertRaises(ValueError):
                verify(SEED, COLD, boundary=boundary)
        with self.assertRaises(ValueError):
            verify(SEED, COLD, boundary='unknown')

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
