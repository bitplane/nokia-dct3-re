import unittest

from tools.noki8210_phase_check import verify


def fixture(late=False):
    if late:
        return ('8210_cell_trace: caller=0021bb93 text=PH_9000 t=45.8\n'
                '8210_phase_terminal: stores=0 dispatches=0 gates=0 exits=0 publications=0 '
                'cap=512 t=46.016667 root=00138038 phase=00000000 state=000d observer_writes=0')
    sequence = [('store',0,0),('dispatch',0,0),('zero_gate',0,0),('publish',0,0),
                ('store',9,10),('dispatch',10,0),('zero_gate',10,10),('nonzero_exit',10,10)]
    counts,lines = {},[]
    for cycle,(site,phase,r0) in enumerate(sequence,1):
        counts[site] = counts.get(site,0)+1
        state = 15 if cycle <= 4 else 7
        lines.append(f'8210_phase_context: site={site} ordinal={counts[site]} root=00138038 '
                     f'phase={phase:08x} state={state:04x} input=1802 r0={r0:08x} cycles={cycle:x} observer_writes=0')
    lines.append('8210_phase_terminal: stores=2 dispatches=2 gates=2 exits=1 publications=1 '
                 'cap=512 t=46.016667 root=00138038 phase=00000001 state=0007 observer_writes=0')
    return '\n'.join(lines)


class PhaseObservationTest(unittest.TestCase):
    def test_paired_presence_and_bounded_absence(self):
        early = verify(fixture(),late=False)
        late = verify(fixture(True),late=True)
        self.assertEqual(early['counts']['publish'],1)
        self.assertEqual(late['counts'],{})
        self.assertFalse(early['recovery_validated'])
        self.assertEqual(late['terminal_phase'],'00000000')

    def test_rejects_bad_horizon_counters_or_semantics(self):
        for old,new in [('t=46.016667','t=45'), ('cap=512','cap=2'),
                        ('dispatches=2','dispatches=142'), ('ordinal=1','ordinal=73'),
                        ('root=00138038','root=00137f58'), ('input=1802','input=03ec'),
                        ('phase=00000000','phase=00000001'), ('cycles=2','cycles=1'),
                        ('observer_writes=0','observer_writes=1')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify(fixture().replace(old,new),late=False)

    def test_late_result_is_falsifiable(self):
        for old,new in [('phase=00000000','phase=00000001'), ('state=000d','state=000f'),
                        ('stores=0','stores=1'), ('t=45.8','t=10.0')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify(fixture(True).replace(old,new),late=True)
        with self.assertRaises(ValueError):
            verify(fixture(),late=True)


if __name__ == '__main__':
    unittest.main()
