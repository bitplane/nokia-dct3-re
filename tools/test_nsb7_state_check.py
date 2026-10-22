"""Executable acceptance checks for exact NSB-7 idle replay."""
import unittest
from unittest.mock import patch

from tools.nsb7_state_check import check
from tools.test_nsb7_outgoing_call_check import fixture as call_fixture


def fixture():
    cpu = ','.join(['00000000'] * 37)
    state = f'pc=0000001c sp=00130df8 ram=87f4614d cpu={cpu} t=27.000000000'
    return '\n'.join([
        'nsb7_state: event=saved ' + state,
        'state_replay: phase=reference event=begin t=27.000000000',
        'RX enqueue type=80 payload=34 data=6000 t=27.1',
        'state_replay: phase=reference event=end t=30.000000000',
        'nsb7_state: event=restored ' + state,
        'state_roundtrip: result=pass scenario=nsb7_idle requested_at=27.000000000 t=27.000000000',
        'state_replay: phase=restored event=begin t=27.000000000',
        'RX enqueue type=80 payload=34 data=6000 t=27.1',
        'state_replay: phase=restored event=end t=30.000000000',
        'nsb7_state_physical: key=Menu state=1',
        'nsb7_key_decode: key=19 observer_writes=0',
        'nsb7_state_physical: key=Menu state=0',
    ])


class StateCheckTest(unittest.TestCase):
    def test_host_incoming_composes_card_and_epoch_checks(self):
        state=fixture().split('nsb7_state_physical:')[0].replace('nsb7_idle','nsb7_incoming_call')
        text='GSM service downlink kind=20 sapi=0 pd=03 message=0f length=2\n'+state
        text+='\nnsb7_call_input: key=End state=1 t=60.000000000'
        with patch('tools.nsb7_pin_services_check.check_authentication') as authentication, patch(
                'tools.nsb7_pin_services_check.check_source') as source, patch(
                'tools.nsb7_host_incoming_call_check.check') as host:
            check(text,incoming_call=True,pin=True,host=True,card=b'actual card')
            authentication.assert_called_once_with(text.split('nsb7_state: event=saved',1)[0])
            source.assert_called_once_with(b'actual card')
            host.assert_called_once_with(text,restored=True)

    def test_host_call_requires_pre_save_authentication(self):
        with self.assertRaises(ValueError):
            check(fixture(),call=True,host=True)

    def test_host_call_composes_restoration_and_host_epoch_checks(self):
        state=fixture().split('nsb7_state_physical:')[0].replace('nsb7_idle','nsb7_call')
        text='GSM service uplink sapi=0 pd=03 message=0f length=2\n'+state
        text+='\nnsb7_outgoing_call_input: key=End state=1 t=66.000000000'
        with patch('tools.nsb7_pin_services_check.check_authentication') as authentication, patch(
                'tools.nsb7_host_outgoing_call_check.check') as host:
            check(text,call=True,pin=True,host=True)
            authentication.assert_called_once_with(text.split('nsb7_state: event=saved',1)[0])
            host.assert_called_once_with(text,restored=True)

    def incoming_fixture(self):
        state = fixture().split('nsb7_state_physical:')[0].replace(
            'nsb7_idle', 'nsb7_incoming_call').replace('27.', '23.').replace('30.', '26.')
        return ('GSM service downlink kind=1 sapi=0 pd=03 message=0f\n' + state +
                '\nnsb7_call_input: key=End state=1 t=27.000000000')

    def test_incoming_restoration(self):
        text = self.incoming_fixture()
        with patch('tools.nsb7_pin_services_check.check_incoming') as child:
            check(text, incoming_call=True, card=b'actual card')
            child.assert_called_once_with(text, b'actual card')

    def test_incoming_requires_card(self):
        with self.assertRaises(ValueError):
            check(self.incoming_fixture(), incoming_call=True)

    def test_incoming_requires_connection_before_save(self):
        with patch('tools.nsb7_pin_services_check.check_incoming'), self.assertRaises(ValueError):
            check(self.incoming_fixture().replace('message=0f', 'message=01'),
                  incoming_call=True, card=b'card')

    def test_incoming_requires_end_after_replay(self):
        text = self.incoming_fixture()
        text = 'nsb7_call_input: key=End state=1\n' + text.split('\nnsb7_call_input:')[0]
        with patch('tools.nsb7_pin_services_check.check_incoming'), self.assertRaises(ValueError):
            check(text, incoming_call=True, card=b'card')

    def test_complete(self):
        check(fixture())

    def test_architecture_mismatch(self):
        text = fixture().replace('event=restored pc=0000001c', 'event=restored pc=00000020')
        with self.assertRaises(ValueError):
            check(text)

    def test_incomplete_cpu(self):
        with self.assertRaises(ValueError):
            check(fixture().replace(',00000000', '', 1))

    def test_packet_replay_mismatch(self):
        with self.assertRaises(ValueError):
            check(fixture().replace('data=6000', 'data=6001', 1))

    def test_missing_post_load_decode(self):
        with self.assertRaises(ValueError):
            check(fixture().replace('key=19 observer_writes=0', 'key=18 observer_writes=0'))

    def test_incomplete_fixture(self):
        with self.assertRaises(ValueError):
            check(fixture() + '\nnsb7_state: FAIL incomplete')

    def test_call_restoration(self):
        state = fixture().split('nsb7_state_physical:')[0].replace(
            'nsb7_idle', 'nsb7_call').replace('27.', '33.').replace('30.', '36.')
        text = call_fixture().replace('nsb7_outgoing_call_input: key=End state=1',
                                      state + 'nsb7_outgoing_call_input: key=End state=1')
        check(text, call=True)

    def test_call_must_be_connected_at_save(self):
        with self.assertRaises(ValueError):
            check(fixture() + '\n' + call_fixture(), call=True)

    def test_active_sms_completion(self):
        cp = 'GSM service uplink sapi=3 pd=09 message=04 length=2 data=8904\n'
        rp = 'GSM service uplink sapi=3 pd=09 message=01 length=5 data=8901020240'
        text = cp + fixture().split('nsb7_state_physical:')[0].replace(
            'RX enqueue type=80 payload=34 data=6000 t=27.1', rp)
        text = text.replace('nsb7_idle', 'nsb7_sms').replace(
            '27.000000000', '19.350000000').replace('30.000000000', '22.350000000')
        text += '\nnsb7_sms_input: key=Menu state=1 t=23.000000000\nnsb7_state_sms: complete'
        check(text, sms=True)

    def test_pin_authentication_is_bounded_before_save(self):
        text = fixture()
        with patch('tools.nsb7_pin_services_check.check_authentication') as authentication:
            check(text, pin=True)
            authentication.assert_called_once_with('')

    def test_pin_requires_real_authentication(self):
        with self.assertRaises(ValueError):
            check(fixture(), pin=True)

    def test_sms_already_completed_before_save(self):
        rp = 'GSM service uplink sapi=3 pd=09 message=01 length=5 data=8901020240\n'
        with self.assertRaises(ValueError):
            check(rp + fixture(), sms=True)


if __name__ == '__main__':
    unittest.main()
