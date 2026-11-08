import unittest
from tools.npe3_bcch_receive_check import SI3,check,check_broadcast,expected_fire_words
from tools.gsm_xcch_signal import broadcast_octets,broadcast_si_index


class BcchReceiveCheckTest(unittest.TestCase):
    def fixture(self,broadcast=False,frames=None,combined_ccch=False):
        text,writes=[],[]
        cursor=0x80
        frames=frames or ((719,770,821,872) if broadcast else (719,770,821))
        for index,frame in enumerate(frames,1):
            octets=broadcast_octets(broadcast_si_index(frame)) if broadcast else SI3
            if combined_ccch and octets[2]==0x1b:
                octets=octets[:10]+b'\x41'+octets[11:]
            words=expected_fire_words(octets)
            stamp=3.1+index*0.2
            values=[(0x3c,3),(0x3c,5)]+[(0x39,word) for word in words]+[(0x3c,0xfffc),(0x3c,2),(0x38,5)]
            for port,value in values:
                writes.append(f'{stamp:.9f}\t003a50\t{port:04x}\t{value:04x}\tffff')
            text.append(f'npe3_bcch_fire: sequence={index} t={stamp+0.0001:.9f} status=0025 frame={frame:04x} words='+','.join(f'{word:04x}' for word in words)+' read_owner=003ae7 observer_writes=0')
            body=bytes.fromhex('502a0000')+frame.to_bytes(2,'big')+bytes.fromhex('00230000')+octets
            # Odd-byte padding is deliberately not SI3 data.
            padded=body+b'\xa5'
            packet=[0x2180]+[int.from_bytes(padded[at:at+2],'big') for at in range(0,len(padded),2)]
            end=cursor+len(packet)
            text.append(f'rom6_measure_rx_commit: sequence={index} t={stamp+0.0002:.9f} previous={cursor:04x} producer={end:04x} consumer={cursor:04x} words='+','.join(f'{word:04x}' for word in packet)+' mask=ffff cpu_pc=002d08 observer_writes=0')
            cursor=end
        text.append(f'rom6_measure_rx_complete: commits={len(frames)} cap=64 observer_writes=0')
        if broadcast:
            for handler in ('003b9fd0','003b9ed4','003ba0c0','003ba060'):
                text.append('plmn_si3_parse: pc='+handler+' hit=1 ')
            text.append('plmn_si3_parse: pc=002d3e3c hit=4 r0=00000001 state=0002 flags=0f ')
        return '\n'.join(text),'\n'.join(writes)

    def test_three_independent_blocks_with_padding(self):
        report=check(*self.fixture())
        self.assertEqual(report['blocks'],3)
        self.assertFalse(report['camping_qualified'])

    def test_combined_si3_is_checked_as_external_content_not_accepted_as_either_layout(self):
        fixture=self.fixture(True,combined_ccch=True)
        self.assertTrue(check_broadcast(*fixture,combined_ccch=True)['native_complete_broadcast_qualified'])
        with self.assertRaises(ValueError):check_broadcast(*fixture)
        with self.assertRaises(ValueError):check_broadcast(*self.fixture(True),combined_ccch=True)

    def test_full_broadcast_needs_all_consumers_and_acquisition_return(self):
        text,writes=self.fixture(True)
        report=check_broadcast(text,writes)
        self.assertEqual(report['si_indices'],[2,3,0,1])
        self.assertFalse(report['camping_qualified'])
        for old,new in [('pc=003ba060','pc=003ba062'),('flags=0f','flags=0d'),
                        ('r0=00000001','r0=00000000'),('frame=0368','frame=0369')]:
            with self.subTest(old=old),self.assertRaises(ValueError): check_broadcast(text.replace(old,new,1),writes)
        with self.assertRaises(ValueError): check_broadcast(*self.fixture())

    def test_explicit_dynamic_cycle_checks_every_successful_block_until_all_types(self):
        text,writes=self.fixture(True,(872,923,974,1025,1229))
        report=check_broadcast(text,writes,dynamic_frames=True)
        self.assertEqual(report['si_indices'],[1,2,3,1,0])
        self.assertEqual(report['blocks'],5)
        with self.assertRaises(ValueError):check_broadcast(text,writes)
        with self.assertRaises(ValueError):check_broadcast(text.replace('frame=0368','frame=0369'),writes,dynamic_frames=True)
        with self.assertRaises(ValueError):check_broadcast(*self.fixture(True,(770,821,872)),dynamic_frames=True)

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
