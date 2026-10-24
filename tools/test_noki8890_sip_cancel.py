import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from PIL import Image

from tools import run_noki8890_sip_cancel as check


class CleanupFramesTest(unittest.TestCase):
    def test_unanswered_release_requires_own_band_after_restore(self):
        for pcs in (False, True):
            carrier = '0258' if pcs else '003c'
            text = ('registered\nsip_state: saved\nsip_state: restored\n'
                f'TX packet type=02 payload=20 data=041202001117001a6000{carrier}0000001400000001\n'
                'RX enqueue type=89 payload=8 data=0000000000000000\n'
                'LAPDm service Channel Release acknowledged\n'
                'RX enqueue type=80 payload=34 data=60' + '00' * 9 + '1506210001f0\n')
            with patch('tools.noki8890_registration_check.verify') as registration:
                check.check_unanswered_restoration_radio(text, pcs1900=pcs)
                registration.assert_called_once_with('registered\n', pcs1900=pcs,
                                                     configured_gsm900=not pcs)
                for bad in (text.replace(carrier, '0013'),
                            text.replace('sip_state: restored', ''),
                            text.replace('type=89', 'type=88'),
                            text.replace('sip_state: restored\n', '') + 'sip_state: restored\n'):
                    with self.subTest(pcs=pcs), self.assertRaises(ValueError):
                        check.check_unanswered_restoration_radio(bad, pcs1900=pcs)

    def test_connected_restore_requires_own_carrier_and_postload_release(self):
        for outgoing in (False, True):
            connected = 'gsm_call_adapter: ' + ('' if outgoing else 'incoming ') + 'state id=1 epoch=1 phase=connected\n'
            text = ('TX packet type=02 payload=20 data=041202000271012fc100003c0000000400000000\n' +
                    connected + 'gsm_call_adapter: media direction=uplink id=1 sequence=0 good=1\n'
                    'gsm_call_adapter: media direction=downlink id=1 sequence=0 result=accepted\n'
                    'sip_state: saved\nsip_state: restored\n'
                    'TX packet type=02 payload=20 data=041202001117001a6000003c0000001400000001\n'
                    'RX enqueue type=89 payload=8 data=0000000000000000\n'
                    'RX enqueue type=80 payload=34 data=60' + '00' * 9 + '1506210001f0\n')
            for pcs in (False, True):
                own = text.replace('003c', '0258') if pcs else text
                with patch('tools.noki8890_registration_check.verify') as registration:
                    check.check_connected_restoration_radio(own, outgoing=outgoing, pcs1900=pcs)
                    registration.assert_called_once_with(own.split('sip_state: saved')[0],
                        pcs1900=pcs, configured_gsm900=not pcs)
                    for invalid in (own.replace('0258' if pcs else '003c', '0013'),
                                    own.replace('sip_state: restored', ''),
                                    own.replace('sip_state: saved', ''),
                                    own.replace('direction=uplink', 'direction=missing'),
                                    own.replace('good=1', 'good=0'),
                                    own.replace('gsm_call_adapter: media direction=uplink id=1 sequence=0 good=1\n', '') +
                                        'gsm_call_adapter: media direction=uplink id=1 sequence=0 good=1\n',
                                    own.replace('result=accepted', 'result=rejected'),
                                    own.replace('phase=connected', 'phase=alerting'),
                                    own.replace('type=89', 'type=88')):
                        with self.subTest(outgoing=outgoing, pcs=pcs, invalid=invalid), self.assertRaises(ValueError):
                            check.check_connected_restoration_radio(invalid, outgoing=outgoing, pcs1900=pcs)
                with patch('tools.noki8890_registration_check.verify', side_effect=ValueError('unregistered')):
                    with self.assertRaisesRegex(ValueError, 'unregistered'):
                        check.check_connected_restoration_radio(own, outgoing=outgoing, pcs1900=pcs)

    def test_alerting_restore_requires_exact_architecture_and_order(self):
        state = 'pc=00412345 sp=00170000 ram=12345678 t=52.000000000'
        text = ('incoming state id=1 epoch=1 phase=alerting\n'
                '8890_state: event=saved ' + state + '\nsip_state: saved\n'
                '8890_state: event=restored ' + state + '\nsip_state: restored\n'
                'state_roundtrip: result=pass scenario=8890_incoming_alerting\n'
                '8890_sip_cancel: physical Exit\n')
        values = list(range(37))
        values[13], values[15] = 0x170000, 0x412345
        registers = ','.join(f'{value:08x}' for value in values)
        text += ('8890_state_cpu: event=saved cpu=' + registers + '\n'
                 '8890_state_cpu: event=restored cpu=' + registers + '\n')
        check.check_alerting_restoration(text)
        for invalid in (text.replace('event=restored pc=00412345', 'event=restored pc=00412346'),
                        text.replace('phase=alerting', 'phase=connected'),
                        text.replace('sip_state: restored', ''),
                        text + '8890_state: FAIL incomplete\n'):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                check.check_alerting_restoration(invalid)
        for invalid in (text.replace('event=restored cpu=00000000', 'event=restored cpu=00000001'),
                        text.replace(',00000024', ''), text.split('8890_state_cpu:')[0],
                        text.replace('00170000,0000000e', '00170001,0000000e'),
                        text + '8890_state_cpu: event=saved cpu=' + registers + '\n'):
            with self.assertRaisesRegex(ValueError, 'ARM/banked'):
                check.check_alerting_restoration(invalid)

    def test_pending_restore_requires_own_digits_and_exact_snapshot(self):
        state = 'pc=00412345 sp=00170000 ram=12345678 t=32.000000000'
        text = ('gsm_call_adapter: request id=1 epoch=1 digits=1234567\n'
                '8890_state: event=saved ' + state + '\nsip_state: saved\n'
                '8890_state: event=restored ' + state + '\nsip_state: restored\n'
                'state_roundtrip: result=pass scenario=8890_outgoing_pending\n')
        values = list(range(37))
        values[13], values[15] = 0x170000, 0x412345
        registers = ','.join(f'{value:08x}' for value in values)
        text += ('8890_state_cpu: event=saved cpu=' + registers + '\n'
                 '8890_state_cpu: event=restored cpu=' + registers + '\n')
        check.check_alerting_restoration(text, outgoing=True)
        for invalid in (text.replace('digits=1234567', 'digits=5551234'),
                        text.replace('event=restored pc=00412345', 'event=restored pc=00412346'),
                        text.replace('scenario=8890_outgoing_pending', 'scenario=8890_incoming_alerting')):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                check.check_alerting_restoration(invalid, outgoing=True)

    def test_outgoing_requires_own_carrier_send_and_idle_pixels(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            (run / 'snap').mkdir()
            (run / 'console.log').write_text('')
            (run / 'nvram/nsb6hle').mkdir(parents=True)
            storage = bytearray(1611)
            storage[1604:1609] = bytes.fromhex('00f1100001')
            (run / 'nvram/nsb6hle/sim_card').write_bytes(storage)
            image = Image.new('L', (84, 48), 255)
            digest = hashlib.sha256(image.crop((0, 8, 84, 48)).tobytes()).hexdigest()
            for name in ('8890_registered_idle.png', '8890_after_outgoing_call.png'):
                image.save(run / 'snap' / name)
            valid = ('gsm_call_adapter: network registered=1 arfcn=60\n'
                     '8890_call_physical: action=send\n8890_keypad_decoded: key=0e\n')
            with patch.object(check, 'verify_stage'), patch.dict(
                    check.FRAMES, {'8890_sip_registered_idle.png': digest}):
                (run / 'error.log').write_text(valid)
                check.check_outgoing_result(run)
                with self.assertRaisesRegex(ValueError, 'carrier'):
                    check.check_outgoing_result(run, pcs1900=True)
                (run / 'error.log').write_text(valid.replace('arfcn=60', 'arfcn=600'))
                check.check_outgoing_result(run, pcs1900=True)
                with self.assertRaisesRegex(ValueError, 'carrier'):
                    check.check_outgoing_result(run)
                for invalid in (valid.replace('arfcn=60', 'arfcn=1'),
                                valid.replace('key=0e', 'key=0f')):
                    (run / 'error.log').write_text(invalid)
                    with self.assertRaises(ValueError):
                        check.check_outgoing_result(run)
                (run / 'error.log').write_text(valid)
                storage[1610] = 1
                (run / 'nvram/nsb6hle/sim_card').write_bytes(storage)
                with self.assertRaisesRegex(ValueError, 'persisted'):
                    check.check_outgoing_result(run)
                storage[1610] = 0
                (run / 'nvram/nsb6hle/sim_card').write_bytes(storage)
                Image.new('L', (84, 48), 0).save(run / 'snap/8890_after_outgoing_call.png')
                with self.assertRaisesRegex(ValueError, 'reviewed idle'):
                    check.check_outgoing_result(run)

    def test_restored_call_requires_new_epoch_and_post_load_paging(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            (run / 'console.log').write_text('')
            restored = 'state_roundtrip: result=pass scenario=8890_idle\n'
            paging = 'gsm_call_adapter: incoming state id=1 epoch=2\n'
            with patch.object(check, 'verify_stage'), patch('tools.noki8890_state_check.verify'):
                for text, epoch, message in ((paging + restored, 2, 'follow exact'),
                                             (restored + paging, 1, 'fresh host epoch')):
                    (run / 'error.log').write_text(text)
                    (run / 'sip-result.json').write_text(json.dumps({'epoch': epoch}))
                    with self.assertRaisesRegex(ValueError, message):
                        check.check_product_result(run, restore_idle=True)

    def product_result(self, mutation=lambda text: text, bad_storage=False, pcs1900=False):
        setup = '80000000000000000000032445030504046002008134015c0581551532f4'
        text = ('gsm_call_adapter: network registered=1 arfcn=60\n'
                'RX enqueue type=80 payload=34 producer=abc data=' + setup + '\n'
                '8890_sip_cancel: physical Exit\n8890_keypad_decoded: key=1a\n')
        if pcs1900:
            text = text.replace('arfcn=60', 'arfcn=600')
        storage = bytearray(1611)
        storage[1604:1609] = bytes.fromhex('00f1100001')
        storage[1610] = int(bad_storage)
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            (run / 'nvram/nsb6hle').mkdir(parents=True)
            (run / 'nvram/nsb6hle/sim_card').write_bytes(storage)
            (run / 'console.log').write_text('')
            (run / 'error.log').write_text(mutation(text))
            with patch.object(check, 'verify_stage') as stage, patch.object(check, 'check_frames'):
                check.check_product_result(run, pcs1900=pcs1900)
                stage.assert_called_once_with(mutation(text), runtime=True, selftest=True)

    def test_product_boundary_is_not_just_exit_marker(self):
        self.product_result()
        self.product_result(pcs1900=True)
        with self.assertRaisesRegex(ValueError, 'carrier'):
            self.product_result(pcs1900=True, mutation=lambda text: text.replace('arfcn=600', 'arfcn=60'))
        for options in (
            {'mutation': lambda text: text.replace('arfcn=60', 'arfcn=600')},
            {'mutation': lambda text: text.replace('0581551532f4', '0581551532f5')},
            {'mutation': lambda text: text.replace('key=1a', 'key=19')},
            {'mutation': lambda text: text.replace('physical Exit', 'physical Other')},
            {'bad_storage': True},
        ):
            with self.subTest(options=options), self.assertRaises(ValueError):
                self.product_result(**options)

    def check(self, size=(84, 48), bad_frame=False):
        with tempfile.TemporaryDirectory() as directory:
            frames = Path(directory)
            image = Image.new('L', size, 255)
            digest = hashlib.sha256(image.crop((0, 8, 84, 48)).tobytes()).hexdigest()
            expected = {name: digest for name in check.FRAMES}
            for name in expected:
                image.save(frames / name)
            if bad_frame:
                expected['8890_sip_missed_call.png'] = '0' * 64
            with patch.object(check, 'FRAMES', expected):
                check.check_frames(frames)

    def test_reviewed_content_and_geometry_required(self):
        self.check()
        for options in ({'size': (96, 60)}, {'bad_frame': True}):
            with self.subTest(options=options), self.assertRaises(ValueError):
                self.check(**options)

    def test_missing_notification_is_not_cleanup(self):
        with tempfile.TemporaryDirectory() as directory, self.assertRaises(OSError):
            check.check_frames(Path(directory))


if __name__ == '__main__':
    unittest.main()
