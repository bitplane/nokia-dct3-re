import unittest

from tools.arm_architecture_snapshot_check import verify_roundtrip


class SnapshotCheckTest(unittest.TestCase):
    def capture(self):
        cpu=','.join(['00000000']*37)
        row=f'pc=00000000 sp=00000000 ram=12345678 cpu={cpu} t=34.000000000'
        return '\n'.join(('state: event=saved '+row, 'state: event=restored '+row,
            'state_roundtrip: result=pass scenario=unit requested_at=34.000000000 t=34.000000000'))

    def verify(self,text):
        verify_roundtrip(text,prefix='state',scenario='unit')

    def test_complete_snapshot(self):
        self.verify(self.capture())

    def test_mutations_and_missing_inventory(self):
        text=self.capture()
        for old,new in [('event=restored pc=00000000','event=restored pc=00000001'),
                        ('ram=12345678','ram=12345679'),
                        ('cpu='+','.join(['00000000']*37),'cpu='+','.join(['00000000']*36)),
                        ('requested_at=34.','requested_at=35.'),
                        ('event=restored','event=saved')]:
            mutated=text.replace(old,new,1)
            with self.subTest(old=old),self.assertRaises(ValueError):
                self.verify(mutated)
        for suffix in ('\nstate: FAIL incomplete','\nstate: event=restored malformed'):
            with self.subTest(suffix=suffix),self.assertRaises(ValueError):
                self.verify(text+suffix)
