import unittest
from unittest.mock import patch
from tools.rom6_idle_contract import SIGNATURES,static_contract,check


class NativeIdleContractTest(unittest.TestCase):
    def program(self):
        program=bytearray(131072);drom=bytearray(131072)
        for address,encoded in SIGNATURES.items():
            data=bytes.fromhex(encoded);program[address*2:address*2+len(data)]=data
        drom[0xb08b*2:0xb08c*2]=b'\x35\xc5'
        return bytes(program),bytes(drom)

    def fixture(self):
        fill='1506210001f0'+'2b'*17
        first='602a0000056a00230000'+fill
        second='602a00000570002e0001'+fill
        context='mask=ffff selector=0000 primary=0000,0568 secondary=0000,0561 flags=0212,000f mode=0010 status=0000 receiver=0023,0000 observer_writes=0'
        text=('npe3_idle_format: sequence=1 t=6.389960000 low=0568 owner=004f27 '+context+'\n'
              'npe3_idle_format: sequence=2 t=6.389961000 low=056a owner=004f28 '+context+'\n'
              'npe3_idle_format: sequence=3 t=7.000000000 low=0570 owner=004f2c '+context.replace('0568','0570').replace('flags=0212,000f','flags=0212,0000')+'\n'
              f'npe3_idle_publish_gate: sequence=1 t=6.389970000 result=0001 mask=ffff read=00005168 mode=0010 controls=0201,0933 payload={first} observer_writes=0\n'
              f'npe3_idle_publish_gate: sequence=2 t=7.000010000 result=0000 mask=ffff read=00005168 mode=0010 controls=0201,0973 payload={second} observer_writes=0\n'
              'npe3_idle_scalar: sequence=1 t=3.710000000 value=011b mask=ffff read=000035c5 observer_writes=0\n'
              'npe3_idle_observer_complete: formats=3 format_cap=128 scalars=1 scalar_cap=64 gates=2 gate_cap=256 window_end=24 observer_writes=0')
        packets=[dict(type=128,payload=first,time='6.389980000')]
        commands=[dict(kind=0x4a,time=3.7,body=b'\x01\x1b')]
        return text,packets,commands

    def checked(self,text=None,packets=None,commands=None):
        original,native,mcu=self.fixture()
        with patch('tools.rom6_idle_contract.check_rx_publications',return_value=dict(packets=native if packets is None else packets)), \
                patch('tools.rom6_idle_contract.mcu_packets',return_value=mcu if commands is None else commands):
            return check(original if text is None else text,*self.program())

    def test_static_dispatch_scalar_readers_and_conditional_formatter(self):
        program,drom=self.program()
        self.assertEqual(static_contract(program,drom)['literal_reference_sites'],6)
        for address in SIGNATURES:
            bad=bytearray(program);bad[address*2]^=1
            with self.assertRaises(ValueError):static_contract(bytes(bad),drom)
        bad=bytearray(program);bad[0x4200:0x4202]=b'\x05\x78'
        with self.assertRaises(ValueError):static_contract(bytes(bad),drom)
        with self.assertRaises(ValueError):static_contract(program,bytes(131072))

    def test_actual_adjustment_filtering_publication_and_scalar_join(self):
        report=self.checked()
        self.assertEqual(report['frame_adjustments'],1)
        self.assertEqual(report['filtered_valid_fill'],1)
        self.assertEqual(report['scalar_commands'],1)
        self.assertFalse(report['source_frame_binding_qualified'])
        self.assertFalse(report['indirect_writer_closure'])

    def test_missing_mutated_or_truncated_witnesses_reject(self):
        text,packets,commands=self.fixture()
        for old,new in (('low=056a','low=0569'),('selector=0000','selector=0001'),
                        ('flags=0212,000f','flags=0012,000f'),('read=00005168','read=00005169'),
                        ('controls=0201,0973','controls=0201,0933'),('value=011b','value=011c'),
                        ('formats=3','formats=129'),('window_end=24','window_end=60'),
                        (' t=6.389960000',''),('flags=0212,000f','flags=0212'),
                        ('sequence=2','sequence=4'),('observer_writes=0','observer_writes=1')):
            with self.subTest(old=old),self.assertRaises(ValueError):self.checked(text.replace(old,new))
        for native in ([],packets*2,[dict(packets[0],payload=packets[0]['payload'].replace('150621','140621'))]):
            with self.assertRaises(ValueError):self.checked(packets=native)
        for mcu in ([],[dict(commands[0],body=b'\x02\x1b')],[dict(commands[0],time=4.0)]):
            with self.assertRaises(ValueError):self.checked(commands=mcu)


if __name__=='__main__':unittest.main()
