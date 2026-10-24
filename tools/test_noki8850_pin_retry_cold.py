import unittest
from unittest.mock import patch
from pathlib import Path
import tempfile
from PIL import Image

from tools.make_sim_card_profile import make_profile
from tools.run_noki8850_pin_retry_cold import check_card_phase,check_wrong_frame,storage_hashes,check_puk_phase


def failed_card():
    card=bytearray(make_profile(True)+bytes.fromhex('00f110')+b'\xff'*43)
    card[3516]=2
    return bytes(card)


def wrong_trace():
    lines=[]
    for second,key in [(8+i,'Keypad 9') for i in range(4)]+[(12,'Menu')]:
        lines += [f'8850_pin_wrong: key={key} state=1 t={second}.000000000',
                  f'8850_pin_wrong: key={key} state=0 t={second}.150000000']
    lines += ['sim_device: header cla=a0 ins=20 p1=00 p2=01 p3=08',
              'SIM status ins=20 sw=9804 chv=2/3 puk=10/10 enabled=1',
              '8850_pin_wrong: complete=1 firmware_state_forcing=0']
    return '\n'.join(lines)


def puk_rejection_trace(allowance=10,permanent=False):
    items=[(8+i,'Keypad 9') for i in range(8)]+[(16,'Menu')]
    for group in (0,1):
        items += [(20+group*8+i,'Keypad '+str(digit)) for i,digit in enumerate((2,4,6,8))]
        items += [(24+group*8,'Menu')]
    if permanent:items.append((38,'End'))
    lines=[]
    for second,key in items:
        lines += [f'8850_puk_input: key={key} state=1 t={second}.000000000',
                  f'8850_puk_input: key={key} state=0 t={second}.150000000']
    if not permanent:
        status='9840' if allowance==1 else '9804'
        lines += ['sim_device: header cla=a0 ins=2c p1=00 p2=00 p3=10',
                  f'SIM status ins=2c sw={status} chv=0/3 puk={allowance-1}/10 enabled=1']
    marker='permanent_complete' if permanent else 'wrong_complete'
    lines += [f'8850_puk_input: {marker}=1 firmware_state_forcing=0']
    return '\n'.join(lines)


class ColdPinTest(unittest.TestCase):
    def test_each_puk_allowance_has_exact_rejection_and_final_block_status(self):
        for allowance in range(10,0,-1):
            initial=bytearray(failed_card())
            initial[3516]=0
            initial[3518]=allowance
            final=bytearray(initial)
            final[3518]-=1
            with self.subTest(allowance=allowance):
                check_puk_phase(puk_rejection_trace(allowance),bytes(initial),bytes(final),
                                'wrong-puk',puk_attempts=allowance)
                with self.assertRaises(ValueError):
                    check_puk_phase(puk_rejection_trace(allowance),bytes(initial),bytes(initial),
                                    'wrong-puk',puk_attempts=allowance)

    def test_permanent_cold_input_cannot_change_card_or_issue_credentials(self):
        card=bytearray(failed_card())
        card[3516]=card[3518]=0
        trace=puk_rejection_trace(0,True)
        check_puk_phase(trace,bytes(card),bytes(card),'permanent',puk_attempts=0)
        for extra in ('SIM status ins=2c sw=9840 chv=0/3 puk=0/10 enabled=1',
                      'SIM status ins=20 sw=9840 chv=0/3 puk=0/10 enabled=1',
                      'LAPDm Location Updating Accept acknowledged','GSM outgoing request'):
            with self.subTest(extra=extra),self.assertRaises(ValueError):
                check_puk_phase(trace+'\n'+extra,bytes(card),bytes(card),'permanent',puk_attempts=0)
        restored=bytearray(card)
        restored[3518]=10
        with self.assertRaises(ValueError):
            check_puk_phase(trace,bytes(card),bytes(restored),'permanent',puk_attempts=0)

    def test_wrong_puk_consumes_one_allowance_without_installing_new_pin(self):
        blocked=bytearray(failed_card())
        blocked[3516]=0
        rejected=bytearray(blocked)
        rejected[3518]=9
        items=[(8+i,'Keypad 9') for i in range(8)]+[(16,'Menu')]
        for group in (0,1):
            items += [(20+group*8+i,'Keypad '+str(digit)) for i,digit in enumerate((2,4,6,8))]
            items += [(24+group*8,'Menu')]
        lines=[]
        for second,key in items:
            lines += [f'8850_puk_input: key={key} state=1 t={second}.000000000',
                      f'8850_puk_input: key={key} state=0 t={second}.150000000']
        lines += ['sim_device: header cla=a0 ins=2c p1=00 p2=00 p3=10',
                  'SIM status ins=2c sw=9804 chv=0/3 puk=9/10 enabled=1',
                  '8850_puk_input: wrong_complete=1 firmware_state_forcing=0']
        trace='\n'.join(lines)
        check_puk_phase(trace,bytes(blocked),bytes(rejected),'wrong-puk')
        for offset in (0,1604,3484,3516,3517,3518,3520):
            bad=bytearray(rejected)
            bad[offset]^=1
            with self.subTest(offset=offset),self.assertRaises(ValueError):
                check_puk_phase(trace,bytes(blocked),bytes(bad),'wrong-puk')
        for bad in (trace.replace('sw=9804','sw=9000'),
                    trace+'\nLAPDm Location Updating Accept acknowledged',
                    trace.replace('p2=00 p3=10','p2=01 p3=10')):
            with self.assertRaises(ValueError):check_puk_phase(bad,bytes(blocked),bytes(rejected),'wrong-puk')

    def test_exhaustion_requires_three_rejections_and_zero_persistent_pin_retries(self):
        lines=[]
        for start,status,allowance in ((8,'9804',2),(16,'9804',1),(24,'9840',0)):
            for second,key in [(start+i,'Keypad 9') for i in range(4)]+[(start+4,'Menu')]:
                lines += [f'8850_pin_wrong: key={key} state=1 t={second}.000000000',
                          f'8850_pin_wrong: key={key} state=0 t={second}.150000000']
            lines += ['sim_device: header cla=a0 ins=20 p1=00 p2=01 p3=08',
                      f'SIM status ins=20 sw={status} chv={allowance}/3 puk=10/10 enabled=1']
        lines += ['8850_pin_wrong: complete=1 firmware_state_forcing=0']
        trace='\n'.join(lines)
        blocked=bytearray(failed_card())
        blocked[3516]=0
        check_puk_phase(trace,make_profile(True),bytes(blocked),'blocked')
        for bad in (trace.replace('sw=9840','sw=9804'),
                    trace+'\nLAPDm Location Updating Accept acknowledged',
                    trace.replace('p2=01','p2=02')):
            with self.assertRaises(ValueError):check_puk_phase(bad,make_profile(True),bytes(blocked),'blocked')
        with self.assertRaises(ValueError):check_puk_phase(trace,make_profile(True),failed_card(),'blocked')

    def test_new_pin_cold_authentication_rejects_donor_or_old_credential_state(self):
        rotated=bytearray(failed_card())
        rotated[3516]=3
        rotated[3484:3492]=b'2468\xff\xff\xff\xff'
        rotated[1604:1609]=bytes.fromhex('00f1100001')
        rotated[1610]=0
        trace=('8850_pin_physical: key=Menu\n'
               'sim_device: header cla=a0 ins=20 p1=00 p2=01 p3=08\n'
               'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1\n'
               'LAPDm Location Updating Accept acknowledged')
        with patch('tools.run_noki8850_pin_retry_cold.check_correlated_inputs',return_value=[]),\
             patch('tools.run_noki8850_pin_retry_cold.registration') as registered:
            check_puk_phase(trace,bytes(rotated),bytes(rotated),'reauth')
            registered.assert_called_once_with(trace,'nsm2',preserved=True)
            with self.assertRaises(ValueError):check_puk_phase(trace,failed_card(),bytes(rotated),'reauth')
            rotated[3484]=ord('1')
            with self.assertRaises(ValueError):check_puk_phase(trace,bytes(rotated),bytes(rotated),'reauth')

    def test_missing_wrong_frame_and_wrong_pixels_fail(self):
        with tempfile.TemporaryDirectory() as temporary:
            path=Path(temporary)/'error.png'
            with self.assertRaises(OSError):check_wrong_frame(path)
            for size in ((84,48),(96,60)):
                Image.new('L',size).save(path)
                with self.assertRaises(ValueError):check_wrong_frame(path)

    def test_complete_storage_hashes_detect_non_card_changes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary)
            (root/'card').write_bytes(b'card')
            first=storage_hashes(root)
            (root/'flash').write_bytes(b'flash')
            second=storage_hashes(root)
            self.assertNotEqual(first,second)
            (root/'flash').write_bytes(b'changed')
            self.assertNotEqual(second,storage_hashes(root))

    def test_one_rejection_changes_only_retry_allowance(self):
        check_card_phase(wrong_trace(),make_profile(True),failed_card())

    def test_unrelated_persistent_change_and_extra_attempt_fail(self):
        for offset in (0,1604,3484,3500,3517,3520,3560):
            card=bytearray(failed_card())
            card[offset]^=1
            with self.subTest(offset=offset),self.assertRaises(ValueError):
                check_card_phase(wrong_trace(),make_profile(True),bytes(card))
        with self.assertRaises(ValueError):
            check_card_phase(wrong_trace()+'\nSIM status ins=20 sw=9804 chv=1/3 puk=10/10 enabled=1',
                             make_profile(True),failed_card())

    def test_wrong_pin_cannot_register_or_use_pin2(self):
        for trace in (wrong_trace()+'\nLAPDm Location Updating Accept acknowledged',
                      wrong_trace().replace('p2=01','p2=02'),
                      wrong_trace().replace('state=1 t=12.','state=1 t=13.')):
            with self.subTest(trace=trace[-100:]),self.assertRaises(ValueError):
                check_card_phase(trace,make_profile(True),failed_card())

    def test_cold_recovery_requires_failed_input_and_full_restored_card(self):
        card=bytearray(failed_card())
        card[3516]=3
        card[1604:1609]=bytes.fromhex('00f1100001')
        card[1610]=0
        trace=('sim_device: header cla=a0 ins=20 p1=00 p2=01 p3=08\n'
               'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1')
        with patch('tools.run_noki8850_pin_retry_cold.check_pin_inputs') as inputs,\
             patch('tools.run_noki8850_pin_retry_cold.registration') as registered:
            check_card_phase(trace,failed_card(),bytes(card),True)
            inputs.assert_called_once_with(trace)
            registered.assert_called_once_with(trace,'nsm2')
            with self.assertRaises(ValueError):
                check_card_phase(trace,make_profile(True),bytes(card),True)
            card[3516]=2
            with self.assertRaises(ValueError):
                check_card_phase(trace,failed_card(),bytes(card),True)


if __name__=='__main__':unittest.main()
