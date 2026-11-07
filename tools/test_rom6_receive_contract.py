import unittest
from unittest.mock import patch

from tools.rom6_receive_contract import check,check_program


class ReceiveContractTest(unittest.TestCase):
    def fixture(self):
        lines=[]
        for index in range(1024):
            stamp=535_343_582+(index//32)*4_615_385+(index%32)*200
            seconds,fraction=divmod(stamp,1_000_000_000)
            pc=(0x18a9,0x18af,0x18b5,0x18bb)[index%4]
            lines.append(f'rom6_receive_word: sequence={index+1} t={seconds}.{fraction:09d} value=0000 mask=ffff cpu_pc={pc:06x} pmst=07ac imr=035f ifr=0020 observer_writes=0')
        lines.append('rom6_receive_complete: reads=1245280 captured=1024 cap=1024 observer_writes=0')
        return '\n'.join(lines),[b'0.535312000\t003f18\t0009\t3a98\tffff\n',b'0.535320571\t003f75\t0003\t0000\tffff\n']

    def checked(self,text,stream):
        with patch('tools.rom6_receive_contract.check_program'):
            return check(text,stream,b'',b'')

    def test_baseline_scope(self):
        result=self.checked(*self.fixture())
        self.assertEqual(result['groups'],32)
        self.assertFalse(result['physical_interrupt_owner_proved'])

    def test_reject_missing_malformed_or_reordered_read(self):
        text,stream=self.fixture()
        for changed in (text.replace('sequence=1024 ','sequence=1025 '),
                        text.replace('cpu_pc=0018a9','cpu_pc=0018aa',1),
                        text.replace('captured=1024','captured=1023'),
                        text+'\nrom6_receive_word: malformed'):
            with self.subTest(changed=changed[-80:]),self.assertRaises(ValueError):
                self.checked(changed,stream)

    def test_no_implicit_active_receiver(self):
        text,stream=self.fixture()
        with self.assertRaisesRegex(ValueError,'active P03/P09'):
            self.checked(text,[stream[0],stream[1].replace(b'0000\tffff',b'0105\tffff')])

    def test_program_provenance_is_mandatory(self):
        for program in (b'',bytes(0x20000)):
            with self.assertRaises(ValueError): check_program(program,b'')


if __name__=='__main__': unittest.main()
