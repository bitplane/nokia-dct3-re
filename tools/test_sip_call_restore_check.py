import json
from pathlib import Path
import tempfile
import unittest

from tools.run_sip_handset_gate import verify_restore


LOG = '''
incoming state id=1 epoch=1 phase=connected
sip_state: saved
sip_state: restored
termination id=1 cause=41 result=accepted
GSM service downlink kind=13 sapi=0 pd=03 message=25
GSM service uplink sapi=0 pd=03 message=2a
LAPDm service Channel Release acknowledged
incoming state id=1 epoch=2 phase=ended
'''
BRIDGE = '''
SIP incoming identity=(1, 1) caller=5551234
SIP epoch changed old=1 new=2
SIP restored call cleared identity=(2, 1)
'''
REMOTE = 'state changed to CONFIRMED\nRequest msg BYE/\n'


class SipRestoreCheckTest(unittest.TestCase):
    def test_8850_requires_own_decoded_physical_answer(self):
        prefix = ('8850_incoming_physical: action=Call / Send\n'
                  '8850_keypad_decoded key=0e\n'
                  'GSM service uplink sapi=0 pd=03 message=07 length=2\n')
        self.check(log=prefix + LOG, product='8850')
        for wrong in (LOG, prefix.replace('key=0e', 'key=0f') + LOG,
                      prefix.replace('8850', '8210') + LOG, LOG + prefix):
            with self.subTest(log=wrong), self.assertRaises(RuntimeError):
                self.check(log=wrong, product='8850')

    def test_8210_requires_own_decoded_physical_answer(self):
        prefix = ('8210_incoming_physical: action=Call / Send\n'
                  '8210_keypad_decoded: key=0e\n'
                  'GSM service uplink sapi=0 pd=03 message=07 length=2\n')
        self.check(log=prefix + LOG, product='8210')
        for wrong in (LOG, prefix.replace('key=0e', 'key=0f') + LOG,
                      LOG + prefix):
            with self.assertRaises(RuntimeError):
                self.check(log=wrong, product='8210')

    def check(self, log=LOG, bridge=BRIDGE, remote=REMOTE, phase='connected', product='3210'):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'error.log').write_text(log)
            (root / 'sip-bridge.log').write_text(bridge)
            verify_restore(root, remote, phase, product)
            result = json.loads((root / 'sip-result.json').read_text())
            self.assertTrue(result['passed'])
            self.assertTrue(result['scope'].startswith(product + ' '))

    def test_connected_restore_clears_both_sides(self):
        self.check()
        self.check(product='3310')
        self.check(product='3330')
        self.check(product='3410')
        self.check(product='5210')

    def test_bye_before_confirmation_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(remote='Request msg BYE/\nstate changed to CONFIRMED\n')

    def test_end_flag_without_radio_release_is_rejected(self):
        for checkpoint in ('GSM service downlink kind=13',
                           'GSM service uplink sapi=0 pd=03 message=2a',
                           'LAPDm service Channel Release acknowledged'):
            with self.assertRaises(RuntimeError):
                self.check(log=LOG.replace(checkpoint, 'missing'))

    def test_duplicate_clear_or_rejected_repeat_is_rejected(self):
        for extra in ('termination id=1 cause=41 result=accepted\n',
                      'termination id=1 cause=41 result=rejected\n'):
            with self.assertRaises(RuntimeError):
                self.check(log=LOG + extra)

    def test_missing_external_clear_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(remote='state changed to CONFIRMED')

    def test_old_epoch_or_wrong_cause_is_rejected(self):
        for log in (LOG.replace('epoch=2', 'epoch=1'),
                    LOG.replace('cause=41', 'cause=16')):
            with self.assertRaises(RuntimeError):
                self.check(log=log)

    def test_replayed_incoming_dialog_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.check(bridge=BRIDGE + 'SIP incoming identity=(2, 1)\n')

    def test_alerting_restore_requires_rejected_invite(self):
        log = LOG.replace('phase=connected', 'phase=alerting')
        self.check(log=log, remote='Response msg 603/INVITE/\n', phase='alerting')
        self.check(log=log, remote='Response msg 603/INVITE/\n', phase='alerting', product='3330')
        self.check(log=log, remote='Response msg 603/INVITE/\n', phase='alerting', product='5210')
        with self.assertRaises(RuntimeError):
            self.check(log=log, remote='Response msg 180/INVITE/\n', phase='alerting')

    def test_alerting_restore_rejects_answer_or_connection(self):
        log = LOG.replace('phase=connected', 'phase=alerting')
        for remote in ('Response msg 603/INVITE/\nstate changed to CONFIRMED',):
            with self.assertRaises(RuntimeError):
                self.check(log=log, remote=remote, phase='alerting')
        with self.assertRaises(RuntimeError):
            self.check(log=log + 'GSM service uplink sapi=0 pd=03 message=07\n',
                remote='Response msg 603/INVITE/\n', phase='alerting')
        with self.assertRaises(RuntimeError):
            self.check(log=log, bridge=BRIDGE + 'SIP physical answer\n',
                remote='Response msg 603/INVITE/\n', phase='alerting')
        with self.assertRaises(RuntimeError):
            self.check(log=log + 'gsm_call_adapter: media direction=downlink id=1 sequence=0 result=accepted\n',
                remote='Response msg 603/INVITE/\n', phase='alerting')


if __name__ == '__main__':
    unittest.main()
