from pathlib import Path
import json
import tempfile
import unittest
from unittest.mock import patch

from tools.run_sip_handset_gate import verify_outgoing_restore


LOG = '''
GSM service uplink sapi=0 pd=03 message=05 length=15 data=03450401a05e0581551532f4150101
gsm_call_adapter: request id=1 epoch=1 digits=5551234
sip_state: saved
sip_state: restored
gsm_call_adapter: request id=1 epoch=2 digits=5551234
termination id=1 cause=41 result=accepted
outgoing termination consumed id=1 cause=41
GSM service downlink kind=13 sapi=0 pd=03 message=25
GSM service uplink sapi=0 pd=03 message=2d
GSM service downlink kind=26 sapi=0 pd=03 message=2a
LAPDm service Channel Release acknowledged
gsm_call_adapter: state id=1 epoch=2 phase=ended
'''
BRIDGE = '''
SIP dial identity=(1, 1) digits=5551234
SIP epoch changed old=1 new=2
SIP restored call cleared identity=(2, 1)
'''
REMOTE = 'Response msg 180/INVITE/\nRequest msg CANCEL/\nResponse msg 487/INVITE/\n'


class SipOutgoingRestoreCheckTest(unittest.TestCase):
    def test_8890_pending_requires_zero_external_media_inventory(self):
        log = LOG.replace('5551234', '1234567').replace(
            'length=15 data=03450401a05e0581551532f4150101',
            'length=19 data=03450404600200815e0581214365f7a1150101')
        counts = dict(uplink=0, downlink=0, pcm_transmitted=0, pcm_received=0)
        bridge = BRIDGE + 'SIP epoch media discarded ' + json.dumps(counts) + '\n'
        self.check(log=log, bridge=bridge, product='8890')
        for field in counts:
            invalid = BRIDGE + 'SIP epoch media discarded ' + json.dumps({**counts, field: 1})
            with self.subTest(field=field), self.assertRaisesRegex(RuntimeError, 'carried media'):
                self.check(log=log, bridge=invalid, product='8890')
        with self.assertRaisesRegex(RuntimeError, 'inventory'):
            self.check(log=log, bridge=BRIDGE, product='8890')

    def test_8290_connected_restore_composes_product_and_sip_checks(self):
        from tools.test_nsb7_sip_check import restore_fixture
        log=('GSM service uplink sapi=0 pd=03 message=05 length=18 '
             'data=03450404600201805e0581551532f4150101\n'+restore_fixture())
        log=log.replace('termination id=1 cause=41 result=accepted',
            'outgoing termination consumed id=1 cause=41\ntermination id=1 cause=41 result=accepted')
        bridge=BRIDGE+'SIP epoch media discarded {"uplink":0}\n'
        remote='state changed to CONFIRMED\nRequest msg BYE/\n'
        with patch('tools.nsb7_sip_check.check_authentication'):
            result=self.check(log=log,bridge=bridge,remote=remote,connected=True,
                              product='8290',signaling_only=True)
            self.assertIn('"speech_tested": false',result)
            self.assertIn('"native_dsp_complete": false',result)
            for bad in (log+'\ntermination id=1 cause=41 result=accepted',
                        log.replace('81551532f4','81551532f5')):
                with self.assertRaises((RuntimeError,ValueError)):
                    self.check(log=bad,bridge=bridge,remote=remote,connected=True,
                               product='8290',signaling_only=True)

    def test_6210_requires_own_physically_dialed_number(self):
        log = LOG.replace('5551234', '1234567').replace(
            '03450401a05e0581551532f4150101', '03450401a05e0581214365f7150101')
        self.assertIn('6210 pending outgoing', self.check(log=log, product='6210'))
        with self.assertRaises(RuntimeError):
            self.check(product='6210')

    def test_8210_uses_own_number_not_older_fixture_number(self):
        log = LOG.replace('5551234', '1234567').replace(
            '03450401a05e0581551532f4150101', '03450401a05e0581214365f7150101')
        self.check(log=log, product='8210')
        for wrong in (log.replace('digits=1234567', 'digits=5551234'),
                      log.replace('03450401a05e0581214365f7150101',
                                  '03450401a05e0581551532f4150101')):
            with self.assertRaises(RuntimeError):
                self.check(log=wrong, product='8210')

    def check(self, log=LOG, bridge=BRIDGE, remote=REMOTE, connected=False, product='3210', signaling_only=False):
        if product == '3310':
            log = log.replace('length=15 data=03450401a05e0581551532f4150101',
                              'length=19 data=03450404600200815e0581551532f4a2150101')
        elif product in ('3330', '8850'):
            log = log.replace('length=15 data=03450401a05e0581551532f4150101',
                              'length=18 data=03450404600200815e0581551532f4150101')
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'error.log').write_text(log)
            (root / 'sip-bridge.log').write_text(bridge)
            verify_outgoing_restore(root, remote, connected, product, signaling_only=signaling_only)
            return (root / 'sip-result.json').read_text()

    def test_product_scope(self):
        self.assertIn('3310 pending outgoing', self.check(product='3310'))
        self.assertIn('3410 pending outgoing', self.check(product='3410'))
        self.assertIn('5210 pending outgoing', self.check(product='5210'))
        self.assertIn('3330 pending outgoing', self.check(product='3330'))
        self.assertIn('8850 pending outgoing', self.check(product='8850'))

    def test_6210_connected_signaling_restore_requires_no_codec_or_uplink(self):
        log = LOG.replace('5551234', '1234567').replace(
            '03450401a05e0581551532f4150101', '03450401a05e0581214365f7150101')
        log = log.replace('sip_state: saved',
            'gsm_call_adapter: state id=1 epoch=1 phase=connected\nsip_state: saved')
        log = log.replace('termination id=1 cause=41 result=accepted\noutgoing termination consumed id=1 cause=41',
            'outgoing termination consumed id=1 cause=41\ntermination id=1 cause=41 result=accepted')
        bridge = BRIDGE + 'SIP epoch media discarded {"uplink": 0}\n'
        remote = 'state changed to CONFIRMED\nRequest msg BYE/\n'
        result = self.check(log=log, bridge=bridge, remote=remote, connected=True,
                            product='6210', signaling_only=True)
        self.assertIn('"speech_tested": false', result)
        for bad_bridge, bad_log in (
                (BRIDGE, log), (bridge + 'SIP epoch media discarded {"uplink": 0}\n', log),
                (bridge.replace('"uplink": 0', '"uplink": 1'), log),
                (bridge, log + 'dsp_hle: speech tick\n')):
            with self.subTest(bridge=bad_bridge, log=bad_log), self.assertRaises(RuntimeError):
                self.check(log=bad_log, bridge=bad_bridge, remote=remote, connected=True,
                           product='6210', signaling_only=True)

    def test_6250_connected_restore_requires_own_digits_and_connection(self):
        log = LOG.replace('5551234', '123').replace(
            '03450401a05e0581551532f4150101', '03450401a05e038121f3150101').replace('length=15', 'length=13')
        log = log.replace('sip_state: saved',
            'gsm_call_adapter: state id=1 epoch=1 phase=connected\nsip_state: saved')
        log = log.replace('termination id=1 cause=41 result=accepted\noutgoing termination consumed id=1 cause=41',
            'outgoing termination consumed id=1 cause=41\ntermination id=1 cause=41 result=accepted')
        bridge = BRIDGE + 'SIP epoch media discarded {"uplink": 0}\n'
        remote = 'state changed to CONFIRMED\nRequest msg BYE/\n'
        self.check(log=log, bridge=bridge, remote=remote, connected=True,
                   product='6250', signaling_only=True)
        for bad_log in (log.replace('digits=123', 'digits=1234567'),
                        log.replace('phase=connected', 'phase=alerting'),
                        log + 'dsp_hle: speech tick\n'):
            with self.subTest(log=bad_log), self.assertRaises(RuntimeError):
                self.check(log=bad_log, bridge=bridge, remote=remote, connected=True,
                           product='6250', signaling_only=True)

    def test_missing_or_malformed_product_setup_is_rejected(self):
        for log in (LOG.replace('message=05', 'message=06'),
                    LOG.replace('03450401a05e0581551532f4150101',
                                '03450401a05e0581551532f415010100')):
            with self.assertRaises(RuntimeError):
                self.check(log=log, product='3410')

    def test_duplicate_accepted_clear_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(log=LOG + 'termination id=1 cause=41 result=accepted\n')

    def test_pending_call_clears_without_redial(self):
        self.check()

    def test_connected_call_requires_connection_before_save(self):
        log = LOG.replace('sip_state: saved',
            'gsm_call_adapter: state id=1 epoch=1 phase=connected\nsip_state: saved')
        log = log.replace('termination id=1 cause=41 result=accepted\noutgoing termination consumed id=1 cause=41',
            'outgoing termination consumed id=1 cause=41\ntermination id=1 cause=41 result=accepted')
        remote = 'state changed to CONFIRMED\nRequest msg BYE/\n'
        for product in ('3210', '5210'):
            with self.subTest(product=product):
                self.check(log=log, remote=remote, connected=True, product=product)
                with self.assertRaises(RuntimeError):
                    self.check(remote=remote, connected=True, product=product)
                with self.assertRaises(RuntimeError):
                    self.check(log=log, remote='state changed to CONFIRMED\n',
                               connected=True, product=product)

    def test_8890_connected_signaling_restore_has_no_media_or_redial(self):
        log = LOG.replace('5551234', '1234567').replace(
            '03450401a05e0581551532f4150101', '03450401a05e0581214365f7150101')
        log = log.replace('sip_state: saved',
            'gsm_call_adapter: state id=1 epoch=1 phase=connected\nsip_state: saved')
        log = log.replace('termination id=1 cause=41 result=accepted\noutgoing termination consumed id=1 cause=41',
            'outgoing termination consumed id=1 cause=41\ntermination id=1 cause=41 result=accepted')
        bridge = BRIDGE + 'SIP epoch media discarded {"uplink": 0}\n'
        remote = 'state changed to CONFIRMED\nRequest msg BYE/\n'
        result = self.check(log=log, bridge=bridge, remote=remote, connected=True,
                            product='8890', signaling_only=True)
        self.assertIn('"speech_tested": false', result)
        for bad_bridge in (bridge.replace('"uplink": 0', '"uplink": 1'),
                           bridge + 'SIP dial identity=(2, 1)\n'):
            with self.assertRaises(RuntimeError):
                self.check(log=log, bridge=bad_bridge, remote=remote, connected=True,
                           product='8890', signaling_only=True)

    def test_missing_real_cancel_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(remote=REMOTE.replace('Request msg CANCEL/', ''))

    def test_pending_media_acceptance_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(log=LOG + 'gsm_call_adapter: media direction=downlink id=1 epoch=1 result=accepted\n',
                       product='5210')

    def test_reordered_sip_release_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(remote='\n'.join(reversed(REMOTE.splitlines())))

    def test_false_connection_or_redial_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(bridge=BRIDGE + 'SIP dial identity=(2, 1)\n')
        with self.assertRaises(RuntimeError):
            self.check(remote=REMOTE + 'state changed to CONFIRMED\n')
        with self.assertRaises(RuntimeError):
            self.check(log=LOG + 'termination id=1 cause=41 result=rejected\n')

    def test_missing_rr_release_or_wrong_epoch_is_rejected(self):
        for log in (LOG.replace('LAPDm service Channel Release acknowledged', ''),
                    LOG.replace('epoch=2', 'epoch=1')):
            with self.assertRaises(RuntimeError):
                self.check(log=log)


if __name__ == '__main__':
    unittest.main()
