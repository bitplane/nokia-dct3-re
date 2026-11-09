import unittest
from unittest.mock import patch

from tools.rom6_receive_contract import (check,check_program,position_reference,check_programming,
    check_short_program,capture_start_reference,check_capture_programming,check_capture_program,
    check_stop_polls)


class ReceiveContractTest(unittest.TestCase):
    def fixture(self):
        lines=['rom6_receive_coefficients: data=3ebd words=8 values=0387,11a3,2ad6,4000,4000,2ad6,11a3,0387 observer_writes=0']
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

    def test_position_subtracts_32_and_adds_raw_modulus_once(self):
        for ar1,length,expected in ((0,4999,4967),(31,4999,4998),(32,4999,0),
                                    (0,1350,1318),(31,1350,1349),(0x332,1350,0x312),
                                    (0x1368,4999,0x1348),(0xffff,1350,0xffdf)):
            with self.subTest(ar1=ar1,length=length):
                self.assertEqual(position_reference(ar1,length),expected)

    def test_capture_start_uses_one_raw_modulus_addition(self):
        for ar1,lead,length,expected in ((0,163,4999,4836),(1000,163,4999,837),
                                       (163,163,1350,0),(0,163,1350,1187),
                                       (0,1500,1350,0xff6a)):
            with self.subTest(ar1=ar1,lead=lead,length=length):
                self.assertEqual(capture_start_reference(ar1,lead,length),expected)

    def test_stop_poll_masks_status_and_subtracts_raw_length_once(self):
        for counter,length,expected in ((12,4999,44),(4980,4999,13),
                                        (1318,1350,0),(0x3fff,1350,15065)):
            stop=dict(sequence='1',port='000b',cpu_pc='0015d0',t='1.000000003',
                      tail='0020',length=f'{length:04x}',value=f'{expected:04x}')
            text=(f'rom6_capture_stop_poll: sequence=1 first={counter:04x} first_pc=003ef0 '
                  f'first_t=1.000000001 first_mask=ffff last={counter|0xc000:04x} '
                  'last_pc=003ef0 last_t=1.000000002 last_mask=ffff observer_writes=0')
            self.assertEqual(check_stop_polls(text,[stop]),1)
            with self.assertRaises(ValueError):
                check_stop_polls(text.replace('last_t=1.000000002','last_t=1.000000003'),[stop])
            with self.assertRaises(ValueError):
                check_stop_polls('',[stop])

    def test_capture_census_rejects_unknown_writers_and_bad_operands(self):
        lines=[]
        stream=[]
        for index in range(1,550):
            stamp=f'1.{index:09d}'
            port,pc,value=(('000a','003e24','12e4'),('000b','0015d0','002c'),
                           ('000a','003f27','3a98'),('000a','003edc','3a98'),
                           ('000b','003e2c','3a98'),('000b','003f38','0000'),
                           ('0011','003fe5','01fe'))[(index-1)%7]
            a='000000002c' if pc=='0015d0' else '0000000000'
            lines.append(f'rom6_capture_write: sequence={index} t={stamp} port={port} value={value} ar1=0000 a={a} b=00000012e4 lead=00a3 tail=0020 length=1387 cpu_pc={pc} mask=ffff observer_writes=0')
            if pc=='0015d0':
                lines.append(f'rom6_capture_stop_poll: sequence={index} first=000c first_pc=003eea first_t=0.{index:09d} first_mask=ffff last=400c last_pc=003ef0 last_t=0.{index+1:09d} last_mask=ffff observer_writes=0')
            stream.append(f'{stamp}\t{pc}\t{port}\t{value}\tffff\n'.encode())
        lines.append('rom6_capture_complete: writes=549 cap=32768 observer_writes=0')
        text='\n'.join(lines)
        with patch('tools.rom6_receive_contract.check_capture_program'):
            result=check_capture_programming(text,stream,b'',b'',b'')
            self.assertEqual(result['writes'],549)
            self.assertEqual(len(result['writers']),7)
            self.assertFalse(result['physical_capture_edges_qualified'])
            self.assertTrue(result['stop_poll_value_qualified'])
            for old,new in (('value=12e4','value=12e5'),('lead=00a3','lead=00a4'),
                            ('cpu_pc=003e24','cpu_pc=003e23'),('mask=ffff','mask=00ff'),
                            ('writes=549','writes=550'),('sequence=1 ','sequence=2 '),
                            ('a=000000002c','a=000000002d'),
                            ('value=3a98','value=3a99'),('value=01fe','value=01ff'),
                            ('first=000c','first=000d'),('last=400c','last=400d'),
                            ('last_pc=003ef0','last_pc=003eea'),
                            ('last_mask=ffff','last_mask=00ff')):
                with self.subTest(old=old),self.assertRaises(ValueError):
                    check_capture_programming(text.replace(old,new,1),stream,b'',b'',b'')
            with self.assertRaises(ValueError):
                check_capture_programming(text,stream[:-1],b'',b'',b'')
        with self.assertRaises(ValueError): check_capture_program(bytes(0x20000),b'',bytes(0x20000))

    def test_complete_position_programming_and_mutations(self):
        lines=[]
        stream=[]
        for index in range(1,550):
            position=index+32
            first=f'1.{index*2:09d}'
            second=f'1.{index*2+1:09d}'
            lines.append(f'rom6_receive_position: sequence={index} t={first} value={index:04x} ar1={position:04x} length=0546 cpu_pc=00140d mask=ffff observer_writes=0')
            lines.append(f'rom6_receive_programming: sequence={index} t={second} value={index:04x} ar1={position:04x} position1844={index:04x} length=0546 cpu_pc=00198c mask=ffff observer_writes=0')
            stream.append(f'{second}\t00198c\t0009\t{index:04x}\tffff\n'.encode())
        lines.append('rom6_receive_programming_complete: positions=549 programming=549 cap=16384 observer_writes=0')
        text='\n'.join(lines)
        with patch('tools.rom6_receive_contract.check_short_program'):
            result=check_programming(text,stream,b'',b'')
            self.assertEqual(result['p09_writes'],549)
            for old,new in [('cpu_pc=00140d','cpu_pc=00140c'),('ar1=0021','ar1=0022'),
                            ('position1844=0001','position1844=0002'),
                            ('mask=ffff','mask=00ff'),('positions=549','positions=550')]:
                with self.subTest(old=old),self.assertRaises(ValueError):
                    check_programming(text.replace(old,new,1),stream,b'',b'')
            with self.assertRaises(ValueError): check_programming(text,stream[:-1],b'',b'')
        with self.assertRaises(ValueError): check_short_program(bytes(0x4000),b'')


if __name__=='__main__': unittest.main()
