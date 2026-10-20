import unittest
from tools.nsm1_analog_check import check_trace
from tools.nsm1_analog_contract import inspect,FRAGMENTS


class AnalogTest(unittest.TestCase):
    def test_instruction_contract_rejects_mutation(self):
        image=bytearray(0xe0b50)
        for address,raw in FRAGMENTS.items():
            data=bytes.fromhex(raw);image[address-0x200000:address-0x200000+len(data)]=data
        self.assertEqual(inspect(image)['source7_channel'],2)
        for address in FRAGMENTS:
            bad=bytearray(image);bad[address-0x200000]^=1
            with self.assertRaises(ValueError):inspect(bad)

    def test_converted_samples_and_counter_before_decrement(self):
        for nominal in (False,True):
            trace='\n'.join(f'nsm1_analog_entry: pc=0028a892 index={i} r0={int(nominal):08x} first={0xdfd if nominal else 0x19dc:04x} second={0xdfd if nominal else 0x19dc:04x} remaining={11-i if nominal else 10:02x} gain=3f8033c4 offset=00000000 t=1.000000 observer_writes=0' for i in range(1,9))
            if nominal:trace+='\nnsm1_analog_entry: pc=00223a68 \nSIM status ins=a4 sw=9f0f\nnsm1_scalar_receive: task=01 value=00000014 \nnsm1_readiness_nibble: value=000f0000 mask=00ff0000 pc=0028120c task=01 '
            check_trace(trace,nominal)
            for bad in (trace.replace('gain=3f8033c4','gain=3f800000'),trace.replace('remaining=0a','remaining=00',1),trace.replace('r0=00000001','r0=00000000') if nominal else trace+'\nnsm1_analog_entry: pc=00223a68 '):
                with self.assertRaises(ValueError):check_trace(bad,nominal)


if __name__=='__main__':unittest.main()
