import unittest

from tools.make_sim_card_profile import make_profile
from tools.noki8850_host_ussd_check import verify, verify_outgoing, check_restoration


class HostUssdTest(unittest.TestCase):
    def capture(self):
        text = '\n'.join((
            '8850_pin_physical: key=Menu', 'SIM status ins=20 sw=9000',
            'LAPDm Location Updating Accept acknowledged',
            'gsm_call_adapter: network registered=1 arfcn=1',
            'gsm_call_adapter: incoming ussd id=1 result=accepted t=30.010000',
            'gsm_call_adapter: incoming ussd state id=1 epoch=1 phase=queued',
            'gsm_ss: network_initiated operation=notify transaction=0b invoke=1 dcs=0f',
            'gsm_ss: network_initiated handset_response message=3a length=8 data=8b3a05a203020101',
            'LAPDm service Channel Release acknowledged',
            'gsm_call_adapter: incoming ussd state id=1 epoch=1 phase=delivered',
            *[f'8850_host_ussd_input: key={key} state={state} t={time}.{fraction}'
              for key, time in (('Menu', 35), ('End', 38), ('Menu', 41))
              for state, fraction in ((1, '000000000'), (0, '200000000'))],
            '8850_host_ussd: complete',
        ))
        card = bytearray(make_profile(True) + bytes.fromhex('00f110') + b'\xff' * 43)
        card[1604:1609] = bytes.fromhex('00f1100001')
        card[1610] = 0
        return text, bytes(card)

    def test_complete_authenticated_notification(self):
        verify(*self.capture())

    def test_authentication_order_identity_and_contacts(self):
        text, card = self.capture()
        for before, after in (
            ('SIM status ins=20 sw=9000', 'SIM status ins=20 sw=9804'),
            ('t=30.010000', 't=29.010000'),
            ('epoch=1 phase=delivered', 'epoch=2 phase=delivered'),
            ('key=End state=0', 'key=Menu state=0'),
            ('8850_host_ussd: complete', '[LUA ERROR]'),
        ):
            with self.subTest(before=before), self.assertRaises(ValueError):
                verify(text.replace(before, after), card)

    def test_duplicate_operation_and_unrelated_storage_change(self):
        text, card = self.capture()
        with self.assertRaises(ValueError):
            verify(text + '\nnetwork_initiated operation=notify transaction=0b', card)
        for altered in (card[:-1], bytes((card[0] ^ 1,)) + card[1:], card + b'\xff'):
            with self.assertRaises(ValueError):
                verify(text, altered)
        with self.assertRaises(ValueError):
            verify(text + '\nsim_device: update-binary fid=6f7e', card)
        with self.assertRaises(ValueError):
            verify(text + '\nsim_device: header cla=a0 ins=24 p1=00', card)


class OutgoingHostUssdTest(unittest.TestCase):
    def capture(self):
        from tools.noki8850_ussd_check import KEYS
        _, card = HostUssdTest().capture()
        lines = [
            'SIM status ins=20 sw=9000',
            'LAPDm Location Updating Accept acknowledged',
            *['8850_ussd_physical: key=' + key for key in KEYS],
            'gsm_ss: request=ussd_host id=1 transaction=00 dcs=0f packed_length=5',
            'gsm_call_adapter: ussd request id=1 epoch=1 dcs=0f packed_length=5',
            'gsm_call_adapter: ussd response id=2 outcome=0 result=rejected',
            'gsm_call_adapter: ussd response id=1 outcome=0 result=accepted',
            'gsm_call_adapter: ussd state id=1 epoch=1 phase=accepted',
            'LAPDm service Channel Release acknowledged',
            'gsm_call_adapter: ussd state id=1 epoch=1 phase=ended',
            '8850_ussd_physical: key=Back',
            '8850_host_ussd_recovery: Menu press',
            '8850_host_ussd_recovery: Menu release',
        ]
        return '\n'.join(lines), card

    def test_correlated_host_response_and_recovery(self):
        verify_outgoing(*self.capture())

    def test_missing_rejection_stale_epoch_duplicate_and_recovery_fail(self):
        text, card = self.capture()
        for before, after in (
            ('id=2 outcome=0 result=rejected', 'id=2 outcome=0 result=accepted'),
            ('epoch=1 phase=ended', 'epoch=2 phase=ended'),
            ('key=Keypad #', 'key=Keypad 9'),
            ('8850_ussd_physical: key=Back', ''),
            ('8850_host_ussd_recovery: Menu release', ''),
        ):
            with self.subTest(before=before), self.assertRaises(ValueError):
                verify_outgoing(text.replace(before, after), card)
        with self.assertRaises(ValueError):
            verify_outgoing(text + '\ngsm_call_adapter: ussd request id=1 epoch=1', card)
        with self.assertRaises(ValueError):
            verify_outgoing(text + '\n[LUA ERROR]', card)

    def test_storage_changes_or_post_request_security_commands_fail(self):
        text, card = self.capture()
        with self.assertRaises(ValueError):
            verify_outgoing(text, card[:-1])
        with self.assertRaises(ValueError):
            verify_outgoing(text + '\nsim_device: header cla=a0 ins=2c', card)


class PendingHostUssdTest(unittest.TestCase):
    def capture(self):
        text, card = OutgoingHostUssdTest().capture()
        cpu = ','.join(['00000000'] * 37)
        snapshot = f'pc=00000000 sp=00000000 ram=12345678 cpu={cpu} t=31.800000000'
        initial = 'gsm_call_adapter: ussd request id=1 epoch=1 dcs=0f packed_length=5'
        sequence = '\n'.join((initial,
            '8850_ussd_state: event=saved ' + snapshot,
            '8850_ussd_state: event=restored ' + snapshot,
            'state_roundtrip: result=pass scenario=nsm2_ussd requested_at=31.800000000 t=31.800000000',
            initial.replace('epoch=1', 'epoch=2'),
            'gsm_call_adapter: ussd response id=1 outcome=0 result=rejected'))
        text = text.replace(initial, sequence)
        text = text.replace('epoch=1 phase=', 'epoch=2 phase=')
        return text, card

    def test_exact_pending_restore_and_new_epoch_response(self):
        verify_outgoing(*self.capture(), restored=True)

    def test_banked_register_count_and_snapshot_consistency(self):
        text, _ = self.capture()
        for bad in (
            text.replace('cpu=' + ','.join(['00000000'] * 37),
                         'cpu=' + ','.join(['00000000'] * 36)),
            text.replace('pc=00000000', 'pc=00000001'),
            text.replace('event=restored pc=00000000 sp=00000000 ram=12345678',
                         'event=restored pc=00000000 sp=00000000 ram=12345679'),
            text.replace('requested_at=31.800000000', 'requested_at=32.800000000'),
            text + '\n8850_ussd_state: event=saved malformed',
        ):
            with self.assertRaises(ValueError):
                check_restoration(bad)

    def test_early_host_answer_and_missing_old_epoch_rejection(self):
        text, card = self.capture()
        early = text.replace('8850_ussd_state: event=saved',
                             'ussd response id=1 outcome=0 result=accepted\n8850_ussd_state: event=saved')
        with self.assertRaises(ValueError):
            check_restoration(early)
        with self.assertRaises(ValueError):
            verify_outgoing(text.replace('id=1 outcome=0 result=rejected',
                                         'id=3 outcome=0 result=rejected'), card, restored=True)
