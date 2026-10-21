"""Authenticated LU bytes, denial control and complete fresh/cold card states."""
import unittest
from unittest.mock import patch

from tools.nsb7_pin_registration_check import check_registered, check_denied, check_cards, request, LOCI
from tools.nsb7_pin_check import serialized_profile
from tools.make_sim_card_profile import make_profile
from tools.sim_security_trace_check import ATTEMPTS_OFFSET
from tools.test_nsb7_location_update_check import trace as location_trace
from tools.test_nsb7_pin_check import fixture as pin_trace


def registered_trace(cold=False):
    data = request(cold)
    rows = ['SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',
            'GSM service establish sapi=0 pd=05 message=08 length=18 data='+data.hex()]
    exchange = location_trace().replace('index=45 ', 'index=48 ').replace(
        'index=44 ', 'index=47 ').replace('index=43 ', 'index=46 ')
    exchange = exchange.replace(request(False).hex(), data.hex())
    common = '0412020200000016600002580000000001297000'
    for index in range(1, 58):
        if 46 <= index <= 48:
            continue
        kind, body = ('00', '')
        if index == 45:
            kind, body = '1b', '0080013f49'+data.hex()+'2b2b000000'
        elif index == 49:
            kind, body = '46', '3210321000010000'
        elif index in (50, 51):
            kind, body = '02', common
        elif index == 52:
            kind, body = '57', '03050000'
        if index == 49:
            rows.append(exchange.replace(' t=11', ' cursor=00 producer=01 pc=002cb0ec t=11.0 observer_writes=0').replace(
                ' t=12', ' cursor=00 producer=01 pc=002cb0ec t=12.0 observer_writes=0'))
        rows.append(f'nsb7_radio_tx: index={index} type={kind} length={len(body)//2} data={body} cursor=00 producer=01 pc=002cb0ec t=15.0 observer_writes=0')
    wire = b'' if cold else bytes.fromhex('a0d600040500f1100001a0d6000a0100')
    rows += [f'nsb7_sim_tx: index={i} value={byte:02x}' for i, byte in enumerate(wire, 1)]
    rows.append('nsb7_pin_registration: complete')
    return '\n'.join(rows)


class PinRegistrationTest(unittest.TestCase):
    @patch('tools.nsb7_pin_registration_check.check_phone')
    @patch('tools.nsb7_pin_registration_check.check_pin')
    def test_fresh_and_cold_payloads(self, pin, phone):
        check_registered(registered_trace())
        check_registered(registered_trace(True), cold=True)

    @patch('tools.nsb7_pin_registration_check.check_phone')
    @patch('tools.nsb7_pin_registration_check.check_pin')
    def test_cold_must_use_retained_lai(self, pin, phone):
        with self.assertRaises(ValueError):
            check_registered(registered_trace(), cold=True)

    @patch('tools.nsb7_pin_registration_check.check_phone')
    @patch('tools.nsb7_pin_registration_check.check_pin')
    def test_ack_payload_still_strict(self, pin, phone):
        with self.assertRaises(ValueError):
            check_registered(registered_trace().replace('0080034101', '0080032101'))

    @patch('tools.nsb7_pin_registration_check.check_phone')
    @patch('tools.nsb7_pin_registration_check.check_pin')
    def test_verification_must_precede_registration(self, pin, phone):
        text = registered_trace().replace('SIM status ins=20 sw=9000', 'missing verification')
        with self.assertRaises(ValueError):
            check_registered(text+'\nSIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1')

    @patch('tools.nsb7_pin_registration_check.check_phone')
    @patch('tools.nsb7_pin_registration_check.check_pin')
    def test_retained_card_has_no_redundant_writes(self, pin, phone):
        text = registered_trace(True)+'\nnsb7_sim_tx: index=1 value=a0\nnsb7_sim_tx: index=2 value=d6'
        with self.assertRaises(ValueError):
            check_registered(text, cold=True)

    def test_wrong_pin_control(self):
        text = pin_trace(False)+'\nccont_rtc: event=second t=45.000000000'
        check_denied(text)
        with self.assertRaises(ValueError):
            check_denied(text+'\nGSM service establish sapi=0 pd=05 message=08')

    def test_complete_cards(self):
        wrong = bytearray(serialized_profile())
        wrong[ATTEMPTS_OFFSET] = 2
        registered = bytearray(serialized_profile())
        registered[1600:1611] = LOCI
        initial = make_profile(True)
        check_cards(initial, bytes(wrong), initial, bytes(registered), bytes(registered), bytes(registered))
        registered[ATTEMPTS_OFFSET] = 2
        with self.assertRaises(ValueError):
            check_cards(initial, bytes(wrong), initial, bytes(registered), bytes(registered), bytes(registered))


if __name__ == '__main__':
    unittest.main()
