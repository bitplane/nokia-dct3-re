import unittest
from tools.npe3_bcch_receive_check import SI3,check,expected_fire_words


class BcchReceiveCheckTest(unittest.TestCase):
    def fixture(self):
        text,writes=[],[]
        cursor=0x80
        words=expected_fire_words()
        for index,frame in enumerate((719,770,821),1):
            stamp=3.1+index*0.2
            values=[(0x3c,3),(0x3c,5)]+[(0x39,word) for word in words]+[(0x3c,0xfffc),(0x3c,2),(0x38,5)]
            for port,value in values:
                writes.append(f'{stamp:.9f}\t003a50\t{port:04x}\t{value:04x}\tffff')
            text.append(f'npe3_bcch_fire: sequence={index} t={stamp+0.0001:.9f} status=0025 frame={frame:04x} words='+','.join(f'{word:04x}' for word in words)+' read_owner=003ae7 observer_writes=0')
            body=bytes.fromhex('502a0000')+frame.to_bytes(2,'big')+bytes.fromhex('00230000')+SI3
            # Odd-byte padding is deliberately not SI3 data.
            padded=body+b'\xa5'
            packet=[0x2180]+[int.from_bytes(padded[at:at+2],'big') for at in range(0,len(padded),2)]
            end=cursor+len(packet)
            text.append(f'rom6_measure_rx_commit: sequence={index} t={stamp+0.0002:.9f} previous={cursor:04x} producer={end:04x} consumer={cursor:04x} words='+','.join(f'{word:04x}' for word in packet)+' mask=ffff cpu_pc=002d08 observer_writes=0')
            cursor=end
        text.append('rom6_measure_rx_complete: commits=3 cap=64 observer_writes=0')
        return '\n'.join(text),'\n'.join(writes)

    def test_three_independent_blocks_with_padding(self):
        report=check(*self.fixture())
        self.assertEqual(report['blocks'],3)
        self.assertFalse(report['camping_qualified'])

    def test_every_fire_bit_is_load_bearing(self):
        text,writes=self.fixture()
        rows=writes.splitlines()
        for bit in range(224):
            changed=rows.copy()
            fields=changed[2+bit//16].split('\t')
            fields[3]=f'{int(fields[3],16)^(1<<(bit%16)):04x}'
            changed[2+bit//16]='\t'.join(fields)
            with self.subTest(bit=bit),self.assertRaises(ValueError): check(text,'\n'.join(changed))

    def test_failure_missing_read_bad_frame_and_changed_payload_reject(self):
        text,writes=self.fixture()
        for old,new in [('status=0025','status=0005'),('read_owner=003ae7','read_owner=003ae0'),
                        ('frame=02cf','frame=02ce'),('4906,1b00','4907,1b00')]:
            self.assertIn(old,text)
            with self.subTest(old=old),self.assertRaises(ValueError): check(text.replace(old,new,1),writes)


if __name__=='__main__': unittest.main()
