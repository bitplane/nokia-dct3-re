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

    def test_location_accept_allows_i_frame_sequences_not_supervisory_frames(self):
        for control in range(256):
            text = radio_fixture().replace('0300450502', f'03{control:02x}450502')
            with self.subTest(control=control):
                if control & 1:
                    with self.assertRaisesRegex(ValueError, 'Location Updating Accept'):
                        self.verify(text)
                else:
                    self.verify(text)

    def test_authenticated_registration_requires_exchange_and_shifted_acknowledgements(self):
        text = radio_fixture().replace(
            '0300450502', '0322450502')
        start = text.index('RX enqueue type=80 payload=34',
                           text.index('0173490508') + 10)
        exchange = (
            'RX enqueue type=80 payload=34 data=8012000000000013000003004d051200'
            + '11' * 16 + '\n'
            'GSM service uplink sapi=0 pd=05 message=14 length=6 data=055411223344\n')
        text = text[:start] + exchange + text[start:]
        text = text.replace('LAPDm Location Updating Accept acknowledged nr=1',
                            'TX packet type=1b data=0080034101')
        text = text.replace('data=0080032101', 'data=0080034101')
        # Distinguish packet publication from the transport consuming it.
        first = text.index('TX packet type=1b data=0080034101')
        second = text.index('TX packet type=1b', first + 1)
        text = text[:second] + text[second:].replace('TX packet', 'TX consume', 1)
        text = text.replace('LAPDm Channel Release acknowledged nr=2',
                            'LAPDm service Channel Release acknowledged nr=3')
        release = text.index('LAPDm service Channel Release')
        text = text[:release] + text[release:].replace('data=0080034101', 'data=0080036101', 1)
        location = 'sim_device: update-binary fid=6f7e offset=4 length=5'
        text = text.replace(location, '')
        consume_end = text.index('\n', text.index('TX consume type=1b'))
        text = text[:consume_end] + '\n' + location + text[consume_end:]
        self.verify(text)
        for required in (exchange.splitlines()[0], exchange.splitlines()[1],
                         'data=0080036101', location):
            with self.subTest(required=required), self.assertRaises(ValueError):
                self.verify(text.replace(required, 'missing'))

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
