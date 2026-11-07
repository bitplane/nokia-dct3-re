import io
import unittest
from tools.rom6_timed_output_check import check


class TimedOutputTest(unittest.TestCase):
    def fixture(self):
        text='\n'.join(f'rom4_transfer_timer: expiry={i} target=0010 counter=0010 delay=16 t={i}.000000000'
                       for i in (1,2,3))+'\nrom4_interface_summary: transfer_expiries=3'
        rows=[(0,'0004','3a98'),(1,'0030','0100'),(2,'0031','48ce'),
              (3,'0032','4400'),(4,'0004','0020'),(5,'0004','0010'),
              (1_100_000_000,'0001','1000')]
        stream=b''.join(f'{time//1_000_000_000}.{time%1_000_000_000:09d}\t001a99\t{port}\t{value}\tffff\n'.encode()
                        for time,port,value in rows)
        return text,stream

    def test_latest_compare_and_coalesced_ack_are_not_a_fifo(self):
        text,stream=self.fixture()
        result=check(text,io.BytesIO(stream))
        self.assertEqual(result['payload_equal_at_arm_and_edge'],3)
        self.assertEqual(result['consecutive_edge_intervals'],{'with_ack':1,'without_ack':1})
        self.assertFalse(result['physical_latch_timing_proved'])
        self.assertFalse(result['hardware_fifo_proved'])

    def test_payload_changes_are_reported_not_hidden_by_a_snapshot(self):
        text,stream=self.fixture()
        stream+=b'0.500000000\t001a95\t0032\t6000\tffff\n'
        # The chronological stream must remain ordered.
        stream=b''.join(sorted(stream.splitlines(keepends=True)))
        result=check(text,io.BytesIO(stream))
        self.assertEqual(result['payload_equal_at_arm_and_edge'],0)

    def test_bad_target_counter_order_or_capture_is_rejected(self):
        text,stream=self.fixture()
        for old,new in (('target=0010','target=0020'),('counter=0010','counter=0011'),
                        ('expiry=2','expiry=4'),('transfer_expiries=3','transfer_expiries=4'),
                        ('delay=16','delay=0')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(text.replace(old,new,1),io.BytesIO(stream))
        with self.assertRaises(ValueError): check(text,io.BytesIO(stream[:-1]))

    def test_complete_raw_callback_and_pending_latch(self):
        text,stream=self.fixture()
        outputs='\n'.join(f'dsp_compare_output: sequence={i} control=0100 low=48ce high=4400 target=0010 counter=0010 pending_before={pending} t={i}.000000000'
                          for i,pending in ((1,0),(2,0),(3,1)))
        full=outputs+'\nrom4_interface_summary: transfer_expiries=3'
        result=check(full,io.BytesIO(stream),full_boundary=True)
        self.assertTrue(result['complete_callback_capture'])
        self.assertEqual(result['pending_before'],{0:2,1:1})
        for old,new in (('low=48ce','low=48cf'),('pending_before=1','pending_before=0'),
                        ('control=0100','control=0000'),('transfer_expiries=3','transfer_expiries=4')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(full.replace(old,new,1),io.BytesIO(stream),full_boundary=True)


if __name__=='__main__': unittest.main()
