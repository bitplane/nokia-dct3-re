import hashlib
import unittest
from unittest.mock import patch
from tools.make_sim_card_profile import make_profile
from tools.nse2_pin_state_check import check_pin_state,check_puk_state
from tools.sim_security_trace_check import ATTEMPTS_OFFSET,CHV_OFFSET


def fixture(last_retry=False):
    initial=1 if last_retry else 3
    remaining=initial-1
    before=bytearray(make_profile(True).ljust(3570,b'\xff'))
    before[ATTEMPTS_OFFSET]=initial
    after=bytearray(before)
    after[ATTEMPTS_OFFSET]=remaining
    snapshot='t=7.663201154 pc=00291a1c sp=0010f7a0 ram=3d516c96 cpu='+','.join(['00000000']*37)
    snapshot+=' card=m_ins:20,m_tx_len:0,m_tx_expected:8,m_receiving_body:1,m_selected_file:6fad'
    snapshot+=f' simi=m_rx_count:1,m_rx_ready:0,m_iir:0,m_tx_ready_pending:1,m_uart_tx_count:0 record={initial:02x}03 partial='
    status=f'SIM status ins=20 sw={"9840" if last_retry else "9804"} chv={remaining}/3 puk=10/10 enabled=1'
    release='nse2_pin_contact: event=10 down=0 t=7.750000000 physical_only=1'
    text='\n'.join(['nse2_sim_state: phase=saved '+snapshot,status,release,
                    f'nse2_sim_state: committed=reference record={remaining:02x}03',
                    'nse2_sim_state: phase=restored '+snapshot,
                    'nse2_pin_contact_replay: restored=1 physical_only=1',status,release,
                    f'nse2_sim_state: committed=restored record={remaining:02x}03',
                    'nse2_sim_state: completed=1 reference=11 restored=11 sent_before_save=0'])
    events=[('rx',0x20)]+[('tx',v) for v in b'1111\xff\xff\xff\xff']+[('rx',0x98),('rx',0x40 if last_retry else 4)]
    wire='\n'.join(f'{phase}:{i}:{i*.0001:.9f}:{kind}:{value:02x}'
                   for phase in ('reference','restored') for i,(kind,value) in enumerate(events,1))
    return text,wire,before,after


class PinStateTest(unittest.TestCase):
    def test_committed_pin_restores_pending_status_without_executing_again(self):
        text,_,before,after=fixture()
        status='SIM status ins=20 sw=9804 chv=2/3 puk=10/10 enabled=1'
        text=status+'\n'+text.replace(status+'\n','')
        text=text.replace('m_tx_expected:8,m_receiving_body:1',
                          'm_tx_expected:0,m_receiving_body:0')
        text=text.replace('m_rx_count:1','m_rx_count:2')
        text=text.replace('record=0303','record=0203')
        text=text.replace('partial=','partial=31313131ffffffff')
        text=text.replace('reference=11 restored=11 sent_before_save=0',
                          'reference=2 restored=2 sent_before_save=8')
        wire='\n'.join(f'{phase}:{i}:{i*.0001:.9f}:rx:{value:02x}'
                       for phase in ('reference','restored') for i,value in enumerate((0x98,4),1))
        with patch('tools.nse2_pin_state_check.CODE_ERROR_FRAME',hashlib.sha256(b'error').hexdigest()):
            self.assertTrue(check_pin_state(text,wire,before,after,b'error',b'error',status_pending=True)['status_pending'])
            for bad in (text.replace('m_rx_count:2','m_rx_count:0'),
                        text.replace('record=0203','record=0303'),
                        text+'\n'+status,
                        text.replace(status+'\n','').replace('nse2_sim_state: committed=reference',
                                                            status+'\nnse2_sim_state: committed=reference')):
                with self.assertRaises(ValueError):
                    check_pin_state(bad,wire,before,after,b'error',b'error',status_pending=True)
            with self.assertRaises(ValueError):
                check_pin_state(text,wire,before,after,b'error',b'error',buffered_body=True,status_pending=True)

    def test_buffered_pin_body_restores_controller_not_card_body(self):
        text,_,before,after=fixture()
        text=text.replace('m_rx_count:1,m_rx_ready:0,m_iir:0,m_tx_ready_pending:1,m_uart_tx_count:0',
                          'm_rx_count:0,m_rx_ready:0,m_iir:0,m_tx_ready_pending:0,m_uart_tx_count:5')
        text=text.replace('partial=','partial=31313131ff')
        text=text.replace('reference=11 restored=11 sent_before_save=0',
                          'reference=5 restored=5 sent_before_save=5')
        events=[('tx',0xff)]*3+[('rx',0x98),('rx',4)]
        wire='\n'.join(f'{phase}:{i}:{i*.0001:.9f}:{kind}:{value:02x}'
                       for phase in ('reference','restored') for i,(kind,value) in enumerate(events,1))
        with patch('tools.nse2_pin_state_check.CODE_ERROR_FRAME',hashlib.sha256(b'error').hexdigest()):
            result=check_pin_state(text,wire,before,after,b'error',b'error',buffered_body=True)
            self.assertEqual(result['buffered_body_bytes'],5)
            for bad in (text.replace('m_uart_tx_count:5','m_uart_tx_count:4'),
                        text.replace('m_tx_len:0','m_tx_len:5'),
                        text.replace('partial=31313131ff','partial=31313131fe')):
                with self.assertRaises(ValueError):
                    check_pin_state(bad,wire,before,after,b'error',b'error',buffered_body=True)
            with self.assertRaises(ValueError):
                check_pin_state(text,wire,before,after,b'error',b'error')

    def test_puk_recovery_restores_old_credentials_before_commit(self):
        before=bytearray(make_profile(True).ljust(3570,b'\xff'))
        before[ATTEMPTS_OFFSET]=0
        after=bytearray(before)
        after[ATTEMPTS_OFFSET]=3
        after[CHV_OFFSET:CHV_OFFSET+8]=b'2468\xff\xff\xff\xff'
        old='0003'+before[CHV_OFFSET:CHV_OFFSET+16].hex()
        new='0303'+after[CHV_OFFSET:CHV_OFFSET+16].hex()
        snapshot='t=18.265665615 pc=00291a1c sp=0010f7a0 ram=12345678 cpu='+','.join(['00000000']*37)
        snapshot+=' card=m_ins:2c,m_tx_len:0,m_tx_expected:10,m_receiving_body:1,m_selected_file:6fad'
        snapshot+=' simi=m_rx_count:1,m_rx_ready:0,m_iir:0,m_tx_ready_pending:1,m_uart_tx_count:0 record='+old+' partial='
        status='SIM status ins=2c sw=9000 chv=3/3 puk=10/10 enabled=1'
        release='nse2_puk_contact: event=38 down=0 t=18.350000000 physical_only=1'
        text='\n'.join(['nse2_sim_state: phase=saved '+snapshot,status,release,
                        'nse2_sim_state: committed=reference record='+new,
                        'nse2_sim_state: phase=restored '+snapshot,
                        'nse2_puk_contact_replay: restored=1 physical_only=1',status,release,
                        'nse2_sim_state: committed=restored record='+new,
                        'nse2_sim_state: completed=1 reference=19 restored=19 sent_before_save=0'])
        events=[('rx',0x2c)]+[('tx',v) for v in b'123456782468\xff\xff\xff\xff']+[('rx',0x90),('rx',0)]
        wire='\n'.join(f'{phase}:{i}:{i*.0001:.9f}:{kind}:{value:02x}'
                       for phase in ('reference','restored') for i,(kind,value) in enumerate(events,1))
        with patch('tools.nse2_pin_state_check.SECURITY_FRAME',hashlib.sha256(b'security').hexdigest()):
            result=check_puk_state(text,wire,before,after,b'security',b'security')
            self.assertEqual(result['replacement_pin'],'2468')
            for bad in (text.replace('record='+old,'record='+new),text.replace('puk=10/10','puk=9/10'),
                        text.replace('m_tx_expected:10','m_tx_expected:8'),
                        text.replace(status,status+'\n'+status,1),
                        text.replace(release,'',1),
                        text.replace('nse2_puk_contact_replay: restored=1 physical_only=1','')):
                with self.assertRaises(ValueError):
                    check_puk_state(bad,wire,before,after,b'security',b'security')
            for bad_wire in (
                    wire.replace('restored:1:0.000100000:rx:2c',
                                 'restored:1:0.000101000:rx:2c'),
                    wire.replace('restored:2:0.000200000:tx:31',
                                 'restored:2:0.000200000:tx:32')):
                with self.assertRaises(ValueError):
                    check_puk_state(text,bad_wire,before,after,b'security',b'security')
            changed=after.copy()
            changed[CHV_OFFSET+8]^=1
            with self.assertRaises(ValueError):
                check_puk_state(text,wire,before,changed,b'security',b'security')

    def test_last_retry_blocks_once_and_rejects_wrong_branch(self):
        text,wire,before,after=fixture(last_retry=True)
        with patch('tools.nse2_pin_state_check.PIN_BLOCKED_FRAME',hashlib.sha256(b'blocked').hexdigest()):
            result=check_pin_state(text,wire,before,after,b'blocked',b'blocked',last_retry=True)
            self.assertTrue(result['pin_blocked'])
            self.assertEqual(result['pin_attempts_remaining'],0)
            with self.assertRaises(ValueError):
                check_pin_state(text,wire,before,after,b'blocked',b'blocked')
            for bad in (text.replace('sw=9840','sw=9804'),text.replace('record=0103','record=0003')):
                with self.assertRaises(ValueError):
                    check_pin_state(bad,wire,before,after,b'blocked',b'blocked',last_retry=True)

    def test_pending_pin_replay(self):
        with patch('tools.nse2_pin_state_check.CODE_ERROR_FRAME',hashlib.sha256(b'error').hexdigest()):
            result=check_pin_state(*fixture(),b'error',b'error')
            self.assertEqual(result['pin_attempts_remaining'],2)
            self.assertEqual(result['wire_events'],11)

    def test_rejects_missing_rollback_duplicate_retry_and_wire_drift(self):
        text,wire,before,after=fixture()
        with patch('tools.nse2_pin_state_check.CODE_ERROR_FRAME',hashlib.sha256(b'error').hexdigest()):
            for bad in (text.replace('record=0303','record=0203'),
                        text.replace('m_tx_expected:8','m_tx_expected:0'),
                        text.replace('chv=2/3','chv=1/3'),
                        text.replace('physical_only=1','physical_only=0')):
                with self.assertRaises(ValueError):
                    check_pin_state(bad,wire,before,after,b'error',b'error')
            wrong=bytearray(after)
            wrong[ATTEMPTS_OFFSET]=1
            for bad_card in (wrong,after[:-1]):
                with self.assertRaises(ValueError):
                    check_pin_state(text,wire,before,bad_card,b'error',b'error')
            with self.assertRaises(ValueError):
                check_pin_state(text,wire.replace('restored:1:0.000100000:rx:20','restored:1:0.000100000:rx:21'),before,after,b'error',b'error')


if __name__=='__main__':
    unittest.main()
