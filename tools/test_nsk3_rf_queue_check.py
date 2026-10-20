import unittest
from tools.nsk3_rf_queue_check import check


class QueueTest(unittest.TestCase):
    def test_activity_not_merely_equal_pointers(self):
        lines=[]
        for time,count in zip((5,20,40,45,65,90),(4,4,230,1300,5600,11000)):
            lines.append(f'nsk3_rf_queue_sample: t={time:.6f} producer_writes={count+2} consumer_writes={count} producer_changes={count+1} consumer_changes={count-1} producer=1a00 consumer=1a00 observer_writes=0')
        trace='\n'.join(lines)
        self.assertFalse(check(trace)['physical_rf_semantics_proved'])
        for bad in (trace.replace('observer_writes=0','observer_writes=1',1),
                    '\n'.join(lines[:-1]),trace.replace('consumer=1a00','consumer=1a04'),
                    trace.replace('producer_changes=11001','producer_changes=1')):
            with self.assertRaises(ValueError):check(bad)


if __name__=='__main__':unittest.main()
