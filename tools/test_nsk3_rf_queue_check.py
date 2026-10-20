import unittest
from tools.nsk3_rf_queue_check import check,check_metric,check_source


class QueueTest(unittest.TestCase):
    def test_source_requires_zero_values_and_whole_queue(self):
        trace='\n'.join(f'nsk3_scan_rf_input: index={i+1} value=0000 pc={pc:04x} t={39+(i+1)/100:.6f} observer_writes=0' for i,pc in enumerate((0x330c,0x3312,0x3318,0x331e)*2))
        trace+='\nnsk3_scan_rf_queue: start=1a00 words=72 data='+'0000'*72+' observer_writes=0'
        self.assertTrue(check_source(trace)['all_zero'])
        for bad in (trace.replace('value=0000','value=0001',1),trace.replace('pc=330c','pc=330a',1),trace.replace('data=0000','data=0001'),trace.replace('words=72','words=71')):
            with self.assertRaises(ValueError):check_source(bad)

    def test_metric_requires_live_code_and_actual_return(self):
        program=bytearray(131072)
        for address,raw in {0x4701:'f0742424',0x47e6:'7712125477131255308a218a20823083',0x330a:'74d60027',0x0ae6:'6ff8219b0d20'}.items():
            fragment=bytes.fromhex(raw);program[address*2:address*2+len(fragment)]=fragment
        trace='\n'.join(f'nsk3_scan_metric: index={i} pc=0ae7 a=0000000000 threshold=0574 producer=1a00 consumer=1a00 t={39+i/100:.6f} observer_writes=0' for i in range(1,9))
        self.assertEqual(check_metric(trace,program)['measurement_returns'],8)
        for bad in (trace.replace('a=0000000000','a=0000000600'),trace.replace('pc=0ae7','pc=0ae6'),trace.replace('threshold=0574','threshold=0000')):
            with self.assertRaises(ValueError):check_metric(bad,program)
        program[0x4701*2]^=1
        with self.assertRaises(ValueError):check_metric(trace,program)

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
