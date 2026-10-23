import hashlib
import unittest
from unittest.mock import patch

from tools.nse2_sim_write_state_check import check_write_state, RECORD


def fixture():
    before=b'\xff'*3570
    after=RECORD+before[32:]
    snapshot='t=27.585003000 pc=00291a1c sp=0010f7a0 ram=7ee135db cpu='+','.join(['00000000']*37)
    snapshot+=' card=m_ins:dc,m_tx_len:0,m_tx_expected:20,m_receiving_body:1,m_selected_file:6f3a'
    snapshot+=' simi=m_rx_count:1,m_rx_ready:0,m_iir:0,m_tx_ready_pending:1,m_uart_tx_count:0 adn='+'ff'*32
    rows=['nse2_phonebook_contact: event=16 down=0 t=27.570000000 physical_only=1',
          'nse2_sim_state: requested=1 sent=0 t=27.585000000',
          'sim_device: header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3a t=27.58500000',
          'nse2_sim_state: phase=saved '+snapshot,
          'sim_device: update fid=6f3a record=1 length=32 t=27.59000000',
          'SIM status ins=dc sw=9000',
          'nse2_sim_state: committed=reference adn='+RECORD.hex(),
          'nse2_sim_state: phase=restored '+snapshot,
          'sim_device: update fid=6f3a record=1 length=32 t=27.59000000',
          'SIM status ins=dc sw=9000',
          'nse2_sim_state: committed=restored adn='+RECORD.hex(),
          'nse2_sim_state: completed=1 reference=35 restored=35 sent_before_save=0']
    events=[('0.001000000','rx',0xdc)]
    events += [(f'{.002+i*.000001:.9f}','tx',value) for i,value in enumerate(RECORD)]
    events += [('0.004000000','rx',0x90),('0.004100000','rx',0)]
    wire='\n'.join(f'{phase}:{i}:{time}:{kind}:{value:02x}' for phase in ('reference','restored')
                   for i,(time,kind,value) in enumerate(events,1))
    return '\n'.join(rows),wire,before,after


class SimWriteStateTest(unittest.TestCase):
    def test_pending_apdu_restore_wire_and_record(self):
        with patch('tools.nse2_sim_write_state_check.PHONEBOOK_SAVED_FRAME',hashlib.sha256(b'saved').hexdigest()):
            result=check_write_state(*fixture(),b'saved',b'saved')
            self.assertEqual(result['wire_events'],35)
            self.assertFalse(result['stable_long_run'])

    def test_rejects_missing_rollback_bad_protocol_or_unrelated_storage(self):
        text,wire,before,after=fixture()
        bad_texts=[text.replace('sent=0','sent=1'),text.replace('m_rx_ready:0','m_rx_ready:1'),
                   text.replace('m_receiving_body:1','m_receiving_body:0'),
                   text.replace('event=16 down=0 t=27.570000000','event=16 down=0 t=27.650000000'),
                   text.replace('sw=9000','sw=9804',1),text.replace('reference=35','reference=34'),
                   text.replace('phase=restored','phase=missing')]
        with patch('tools.nse2_sim_write_state_check.PHONEBOOK_SAVED_FRAME',hashlib.sha256(b'saved').hexdigest()):
            for bad in bad_texts:
                with self.subTest(bad=bad[-100:]),self.assertRaises(ValueError):
                    check_write_state(bad,wire,before,after,b'saved',b'saved')
            for bad in (wire.replace(':tx:41',':tx:42',1),wire.replace('restored:2:','restored:1:'),
                        wire.replace('0.001000000','0.600000000'),wire+'\ninvalid'):
                with self.assertRaises(ValueError):
                    check_write_state(text,bad,before,after,b'saved',b'saved')
            with self.assertRaises(ValueError):
                check_write_state(text,wire,before,after[:-1]+b'\x00',b'saved',b'saved')


if __name__=='__main__':
    unittest.main()
