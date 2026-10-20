import unittest
from tools.nsk3_liveness_check import FRAGMENTS,check_static,check_failure


class LivenessTest(unittest.TestCase):
    def test_own_static_anchors(self):
        image=bytearray(0x100000)
        for address,encoded in FRAGMENTS.items():
            data=bytes.fromhex(encoded)
            image[address-0x200000:address-0x200000+len(data)]=data
        check_static(image)
        for address in FRAGMENTS:
            bad=bytearray(image)
            bad[address-0x200000]^=1
            with self.assertRaises(ValueError):check_static(bad)

    def test_expected_failure_is_not_stable_acceptance(self):
        trace='\n'.join([
            *[f'nsk3_liveness_window: counter={count:04x} pointer=0010c5a4 cycles={31*(i+1)*13_000_000:016x} observer_writes=0' for i,count in enumerate((1,1,0))],
            f'nsk3_liveness_policy: argument=00000001 caller=002ab50d cycles={93*13_000_000+20:016x} observer_writes=0',
            'nsk3_reset_request: command=05 pc=002ac696 t=93.967473692 reason=68',
            'nsk3_restart_entry: reason=00000068 caller=0023d83b'])
        self.assertFalse(check_failure(trace)['stable_boot'])
        for bad in (trace.replace('counter=0000','counter=0001'),trace.replace('reason=68','reason=04'),
                    trace.replace('caller=002ab50d','caller=002ab50f'),
                    trace.replace('t=93.967473692','t=50.0'),trace+'\nunhandled opcode'):
            with self.assertRaises(ValueError):check_failure(bad)


if __name__=='__main__':unittest.main()
