import hashlib
import unittest
from unittest.mock import patch

from tools.nse2_sim_write_state_check import check_write_state, RECORD
from tools.sim_sms_draft import DRAFT, SMS_OFFSET


def fixture(sms=False):
    record=DRAFT if sms else RECORD
    partial=60 if sms else 0
    fid='6f3c' if sms else '6f3a'
    offset=SMS_OFFSET if sms else 0
    free=b'\x00'+b'\xff'*175 if sms else b'\xff'*32
    before=bytearray(b'\xff'*3570)
    before[offset:offset+len(record)]=free
    after=bytearray(before)
    after[offset:offset+len(record)]=record
    time='25.934003000' if sms else '27.585003000'
    request='25.934000000' if sms else '27.585000000'
    header='25.92700000' if sms else '27.58500000'
    commit='25.94000000' if sms else '27.59000000'
    snapshot=f't={time} pc=00291a1c sp=0010f7a0 ram=7ee135db cpu='+','.join(['00000000']*37)
    snapshot+=f' card=m_ins:dc,m_tx_len:{partial:x},m_tx_expected:{len(record):x},m_receiving_body:1,m_selected_file:{fid}'
    snapshot+=f' simi=m_rx_count:{0 if sms else 1},m_rx_ready:0,m_iir:0,m_tx_ready_pending:1,m_uart_tx_count:0 record={free.hex()} partial={record[:partial].hex()}'
    count=118 if sms else 35
    rows=[('nse2_sms_contact: event=22 down=0 t=25.883333333 physical_only=1' if sms else
           'nse2_phonebook_contact: event=16 down=0 t=27.570000000 physical_only=1'),
          f'nse2_sim_state: requested=1 sent={partial} t={request}',
          f'sim_device: header cla=a0 ins=dc p1=01 p2=04 p3={len(record):02x} selected={fid} t={header}',
          'nse2_sim_state: phase=saved '+snapshot,
          f'sim_device: update fid={fid} record=1 length={len(record)} t={commit}',
          'SIM status ins=dc sw=9000',
          'nse2_sim_state: committed=reference record='+record.hex(),
          'nse2_sim_state: phase=restored '+snapshot,
          f'sim_device: update fid={fid} record=1 length={len(record)} t={commit}',
          'SIM status ins=dc sw=9000',
          'nse2_sim_state: committed=restored record='+record.hex(),
          f'nse2_sim_state: completed=1 reference={count} restored={count} sent_before_save={partial}']
    events=[] if sms else [('0.001000000','rx',0xdc)]
    events += [(f'{.002+i*.000001:.9f}','tx',value) for i,value in enumerate(record[partial:])]
    events += [('0.004000000','rx',0x90),('0.004100000','rx',0)]
    wire='\n'.join(f'{phase}:{i}:{time}:{kind}:{value:02x}' for phase in ('reference','restored')
                   for i,(time,kind,value) in enumerate(events,1))
    return '\n'.join(rows),wire,before,after


class SimWriteStateTest(unittest.TestCase):
    def test_partial_sms_restores_body_and_free_record(self):
        text,wire,before,after=fixture(sms=True)
        with patch('tools.nse2_sim_write_state_check.SMS_SAVED_FRAME',hashlib.sha256(b'saved').hexdigest()):
            result=check_write_state(text,wire,before,after,b'saved',b'saved',sms=True)
            self.assertEqual(result['wire_events'],118)
            self.assertEqual(result['partial_body_bytes'],60)
            for bad in (text.replace('m_tx_len:3c','m_tx_len:40'),
                        text.replace('partial=0702','partial=0602'),
                        text.replace('m_rx_count:0','m_rx_count:1'),
                        text.replace('sent=60','sent=0')):
                with self.assertRaises(ValueError):
                    check_write_state(bad,wire,before,after,b'saved',b'saved',sms=True)
            with self.assertRaises(ValueError):
                check_write_state(text,wire,before,after,b'saved',b'saved')

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
