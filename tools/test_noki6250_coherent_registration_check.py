import unittest
from unittest.mock import patch
from tools import noki6250_coherent_registration_check as check
from tools.radio_registration_trace_check import verify as registration
from tools.test_radio_registration_trace_check import NHM5_GOOD


def radio_fixture():
    text = NHM5_GOOD.replace('0058', '0013').replace(
        'data=040000000000001a600000130000000f00000000',
        'data=041202000000001a600000130000000f00000000')
    return ('RX enqueue type=80 payload=14 data=4012000005140013000048005900\n'
            'TX packet type=02 radio_phase=candidate_channel_change data=041202000000005050000013\n'
            + text + '\ngsm_call_adapter: network registered=1 arfcn=19\n')


def storage():
    data = bytearray(1611)
    data[1604:1609] = bytes.fromhex('00f1100001')
    return bytes(data)


class CoherentRegistrationTest(unittest.TestCase):
    def test_local_peer_still_requires_handset_registration_and_storage(self):
        text = radio_fixture().replace('gsm_call_adapter: network registered=1 arfcn=19', '')
        with patch.object(check, 'check_uploads'), patch.object(check, 'check_initial_fixture'):
            check.verify(text, storage(), require_host=False)
            with self.assertRaises(ValueError):
                check.verify(text, storage())
            with self.assertRaises(ValueError):
                check.verify(text, bytes(1611), require_host=False)

    def test_preserved_scope_keeps_carrier_and_upload_requirements(self):
        text = 'gsm_call_adapter: network registered=1 arfcn=19\n'
        with patch.object(check, 'check_uploads') as uploads, \
                patch.object(check, 'check_initial_fixture') as fixture, \
                patch.object(check, 'check_registration') as radio:
            check.verify(text, storage(), preserved=True)
            uploads.assert_called_once_with(text, runtime=True)
            fixture.assert_called_once_with(text)
            radio.assert_called_once_with(text, 'nhm3', preserved=True, configured_carrier=True)

    def verify(self, text, card=None):
        with patch.object(check, 'check_uploads') as uploads, patch.object(check, 'check_initial_fixture') as fixture:
            check.verify(text, storage() if card is None else card)
            uploads.assert_called_once_with(text, runtime=True)
            fixture.assert_called_once_with(text)

    def test_exact_sch_candidate_release_and_host_carrier(self):
        self.verify(radio_fixture())

    def test_legacy_or_other_carrier_not_promoted(self):
        for old, new in (('arfcn=19', 'arfcn=1'), ('arfcn=19', 'arfcn=190'),
                         ('0013000048', '0013000000'),
                         ('data=041202000000005050000013', 'data=040000000000005050000013'),
                         ('data=041202000000001a60000013', 'data=040000000000001a60000013')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                self.verify(radio_fixture().replace(old, new))

    def test_persistent_location_required(self):
        for card in (b'', bytes(1611), storage()[:1610] + b'\x01'):
            with self.subTest(card=card), self.assertRaises(ValueError):
                self.verify(radio_fixture(), card)

    def test_other_product_cannot_inherit_configured_contract(self):
        with self.assertRaisesRegex(ValueError, 'NHM-3'):
            registration(radio_fixture(), 'nhm5', configured_carrier=True)
