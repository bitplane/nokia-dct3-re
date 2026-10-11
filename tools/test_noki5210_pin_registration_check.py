import unittest
import contextlib
import io
from unittest.mock import patch
from tools.make_sim_card_profile import make_profile
from tools.noki5210_pin_registration_check import verify, verify_late_boundary, verify_coherent_late, verify_pin_sms, verify_pin_idle_state, verify_pin_outgoing_sms


def complete():
    return '\n'.join([
        *(f'5210_pin_physical: key=Keypad {digit}' for digit in '1234'),
        '5210_pin_physical: key=Left Softkey / Menu',
        'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',
        'LAPDm Location Updating Accept acknowledged nr=1',
    ])


class PinRegistrationTest(unittest.TestCase):
    def test_outgoing_sms_requires_exact_submit_and_physical_composition(self):
        menu = '5210_pin_physical: key=Left Softkey / Menu'
        text = '\n'.join(('SIM status ins=20 sw=9000',) + (menu,) * 3 + (
            '5210_pin_physical: key=Keypad 2',) + (menu,) * 2 + tuple(
            '5210_pin_physical: key=Keypad ' + digit for digit in '5551234') + (
            menu, 'GSM service establish sapi=0 pd=05 message=24 length=16',
            'TX packet type=1b data=00800d3f01',
            'GSM service uplink sapi=3 pd=09 message=01 length=27 data=390118000100069121436587090d11010781551532f40000a70161',
            'gsm_sms_submit: cp=39 rp=01 smsc=1234567890 destination=5551234 alphabet=0 user_length=1 outcome=0 status_report=0',
            'GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2',
            'GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5',
            'GSM service uplink sapi=3 pd=09 message=04 length=2 data=3904',
            'LAPDm service Channel Release acknowledged nr=2',
        ))
        verify_pin_outgoing_sms(text)
        rejected = text.replace('downlink kind=18 sapi=3 pd=09 message=01 length=5',
                                'downlink kind=19 sapi=3 pd=09 message=01 length=7')
        verify_pin_outgoing_sms(rejected, rejected=True)
        with self.assertRaises(ValueError):
            verify_pin_outgoing_sms(text, rejected=True)
        with self.assertRaises(ValueError):
            verify_pin_outgoing_sms(rejected)
        with self.assertRaises(ValueError):
            verify_pin_outgoing_sms(rejected + '\nGSM service downlink kind=18 sapi=3', rejected=True)
        silent = text.replace('GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5', '')
        silent = silent.replace('status_report=0', 'status_report=0 t=50.000000')
        silent += '\nTX packet type=1b data=0080015301 t=120.000000'
        silent += ('\nRX enqueue type=80 data=80' + '00' * 9 + '017301' +
                   '\nTX packet type=02 radio_phase=release_channel_change' +
                   '\nPCH no-identity fill')
        verify_pin_outgoing_sms(silent, silent=True)
        local_silent = silent.replace('outcome=0', 'outcome=3')
        verify_pin_outgoing_sms(local_silent, silent=True, local_silence=True)
        with self.assertRaises(ValueError):
            verify_pin_outgoing_sms(local_silent, silent=True)
        with self.assertRaises(ValueError):
            verify_pin_outgoing_sms(silent.replace('t=120.000000', 't=55.000000'), silent=True)
        with self.assertRaises(ValueError):
            verify_pin_outgoing_sms(silent + '\nGSM service downlink kind=19 sapi=3', silent=True)
        with self.assertRaises(ValueError):
            verify_pin_outgoing_sms(silent + '\nGSM service downlink kind=17 sapi=3 pd=09 message=04 length=2', silent=True)
        for old, new in (('a70161', 'a70162'), ('user_length=1', 'user_length=2'),
                         ('key=Keypad 5', 'key=Keypad 6'), ('downlink kind=18', 'downlink kind=19')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify_pin_outgoing_sms(text.replace(old, new))
        with self.assertRaisesRegex(ValueError, 'expected one'):
            verify_pin_outgoing_sms(text + '\ngsm_sms_submit: duplicate')

    def test_checker_rejects_mixed_scenarios_before_reading_evidence(self):
        from tools.noki5210_pin_registration_check import main
        with patch('sys.argv', ['checker', 'missing-run', '--outgoing-call', '--idle-state']), \
                contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as raised:
            main()
        self.assertEqual(raised.exception.code, 2)

    def test_pin_idle_requires_exact_restoration_and_continuation(self):
        text = '\n'.join((
            'LAPDm Location Updating Accept acknowledged nr=1',
            '5210_pin_state: event=saved pc=00200000 sp=00140000 ram=12345678 t=31.000000000',
            '5210_pin_state: event=restored pc=00200000 sp=00140000 ram=12345678 t=31.000000000',
            '5210_pin_state: result=pass',
            '5210_pin_state_physical: key=Left Softkey / Menu',
            '5210_pin_state_physical: key=End',
            '5210_pin_state: completed=true',
        ))
        verify_pin_idle_state(text)
        call_text = text.replace('5210_pin_state_physical: key=Left Softkey / Menu', '')
        verify_pin_idle_state(call_text, call=True)
        sms_text = text.replace('5210_pin_state_physical: key=End',
                                '5210_pin_state_physical: key=Left Softkey / Menu')
        verify_pin_idle_state(sms_text, sms=True)
        with self.assertRaises(ValueError):
            verify_pin_idle_state(sms_text)
        with self.assertRaises(ValueError):
            verify_pin_idle_state(call_text)
        for old, new in (
            ('restored pc=00200000', 'restored pc=00200002'),
            ('ram=12345678 t=31.000000000\n5210_pin_state: result',
             'ram=12345679 t=31.000000000\n5210_pin_state: result'),
            ('key=End', 'key=missing'), ('completed=true', 'completed=false'),
        ):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify_pin_idle_state(text.replace(old, new))

    def test_pin_sms_closure_precedes_physical_read(self):
        text = '\n'.join((
            'sim_device: update fid=6f3c record=1 length=176',
            'GSM service uplink sapi=3 pd=09 message=04 length=2 data=8904',
            'GSM service uplink sapi=3 pd=09 message=01 length=5 data=8901020240',
            'GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2',
            'LAPDm service Channel Release acknowledged nr=3',
            '5210_pin_physical: key=Left Softkey / Menu',
            '5210_pin_physical: key=Left Softkey / Menu',
            'sim_device: update fid=6f3c record=1 length=176',
        ))
        with patch('tools.noki5210_pin_registration_check.verify_coherent_late') as coherent, \
                patch('tools.radio_incoming_sms_trace_check.verify') as sms:
            verify_pin_sms(text, b'card')
            coherent.assert_called_once_with(text, b'card')
            sms.assert_called_once_with(text, b'card', 'nsm5', read=True)
            for token in ('data=8901020240', 'downlink kind=17', 'acknowledged nr=3'):
                with self.subTest(token=token), self.assertRaises(ValueError):
                    verify_pin_sms(text.replace(token, 'missing'), b'card')
            state_text = text.replace('5210_pin_physical:', '5210_pin_state_physical:').replace(
                '5210_pin_state_physical: key=Left Softkey / Menu',
                '5210_pin_state: event=saved\n5210_pin_state: event=restored\n'
                '5210_pin_state_physical: key=Left Softkey / Menu', 1)
            verify_pin_sms(state_text, b'card', state=True)
            with self.assertRaises(ValueError):
                verify_pin_sms(state_text.replace('event=restored', 'event=missing'), b'card', state=True)

    def test_coherent_late_requires_own_measured_carrier(self):
        text = '\n'.join((
            'TX packet type=57 payload=4 words=3 radio_phase=serving_bcch data=03050000',
            'RX enqueue type=8b payload=166 producer=0a4 data=0010005600c4',
            complete(),
            '5210_readiness_root: final current=00000000 queued=00000000 previous=00000000 context=00000000',
        ))
        with patch('tools.noki5210_pin_registration_check.registration') as registration:
            verify_coherent_late(text, make_profile(pin_enabled=True))
            registration.assert_called_once_with(text, 'nsm5', nsm5_pin=True, preserved=False)
            registration.reset_mock()
            verify_coherent_late(text, make_profile(pin_enabled=True), preserved=True)
            registration.assert_called_once_with(text, 'nsm5', nsm5_pin=True, preserved=True)
        with self.assertRaisesRegex(ValueError, 'serving-carrier'):
            verify_coherent_late(text.replace('0010005600c4', '0010000100c4'), make_profile(pin_enabled=True))

    def late_trace(self):
        prefix = '\n'.join((
            'TX packet type=57 payload=4 words=3 radio_phase=serving_bcch data=03050000',
            'RX enqueue type=8b payload=166',
            '5210_measurement_entry: pc=0037019c',
            '5210_measurement_entry: pc=002e336c',
            '5210_measurement_entry: pc=002bd782',
            '5210_request_entry: pc=002e1f5e',
            '5210_request_entry: pc=002bd3d8 r0=00000005',
            '5210_request_entry: pc=002bd424',
            '5210_readiness_selector: argument=00000000 lr=00234f43 state=10 eligible=5 current=00149440 queued=00000000 t=10.209',
        ))
        return prefix + '\n' + complete().replace(
            'LAPDm Location Updating Accept acknowledged nr=1', '\n'.join((
                '5210_readiness_selector: argument=00000003 lr=00232069 state=6 eligible=5 current=00149440 queued=00149e90 t=21.025',
                '5210_readiness_root: final current=00149440 queued=00149e90 previous=00000000 context=00148380 writes=28 selections=2',
                '5210_pin_fixture: stopped_at=45.000000000')))

    def test_late_boundary(self):
        verify_late_boundary(self.late_trace(), make_profile(pin_enabled=True))

    def test_late_boundary_requires_consumption_and_full_window(self):
        for original, replacement in (
            ('pc=002bd782', 'pc=002bd784'),
            ('RX enqueue type=8b payload=166', 'RX missing'),
            ('pc=002bd3d8 r0=00000005', 'pc=002bd3d8 r0=00000004'),
            ('stopped_at=45.000000000', 'stopped_at=21.000000000'),
        ):
            with self.subTest(original=original), self.assertRaises(ValueError):
                verify_late_boundary(self.late_trace().replace(original, replacement), make_profile(pin_enabled=True))

    def test_late_boundary_rejects_new_radio_progress(self):
        with self.assertRaisesRegex(ValueError, 'boundary changed'):
            verify_late_boundary(self.late_trace() + '\nradio_phase=random_access', make_profile(pin_enabled=True))

    def test_late_request_ownership_rejects_missing_or_changed_evidence(self):
        for original, replacement in (
            ('selections=2', 'selections=3'), ('writes=28', 'writes=81'),
            ('argument=00000003', 'argument=00000000'),
            ('state=6 eligible=5', 'state=6 eligible=0'),
            ('final current=00149440 queued=00149e90', 'final current=00149440 queued=00000000'),
        ):
            with self.subTest(original=original), self.assertRaises(ValueError):
                verify_late_boundary(self.late_trace().replace(original, replacement), make_profile(pin_enabled=True))

    def test_complete(self):
        with patch('tools.noki5210_pin_registration_check.registration') as registration:
            verify(complete(), make_profile(pin_enabled=True))
        registration.assert_called_once_with(complete(), 'nsm5')

    def test_bad_pin_or_missing_physical_key(self):
        for text in (complete().replace('sw=9000', 'sw=9804'),
                     complete().replace('key=Keypad 3', 'key=Keypad 5'),
                     complete() + '\n[LUA ERROR]'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                verify(text, make_profile(pin_enabled=True))

    def test_registration_must_follow_pin(self):
        text = complete()
        registration = 'LAPDm Location Updating Accept acknowledged nr=1'
        text = registration + '\n' + text.replace(registration, '')
        with self.assertRaises(ValueError):
            verify(text, make_profile(pin_enabled=True))

    def test_unlocked_card_cannot_pass(self):
        with self.assertRaises(ValueError):
            verify(complete(), make_profile(pin_enabled=False))


if __name__ == '__main__':
    unittest.main()
