import unittest

from tools.mu4_notification_check import check, SITES
from tools.mu4_context_contract import PLAYBACK_PENDING_CONTROL


def fixture():
    lines = ['mu4_notify_scope: identity=Execute_entry operands=context_only observer_writes=0 audio_acceptance=0',
             'mu4_playback_execute_entry: address=026d62 opcode=fd00 t=0.2',
             'mu4_native_playback_overlay: PASS marker=aa99 records=3127 words=49843 checked_ram_words=49839 excluded_register_words=4 before_execution=1']
    for index, (kind, flag) in enumerate(zip(
            ('publish','callback','vector','wrapper','request'), ('0000','0000','0001','0001','0000')), 1):
        address, opcode = SITES[kind]
        lines.append(f'mu4_notify: ordinal={index} kind={kind} address={address:06x} opcode={opcode:04x} cycles={index} '
                     f'sp=1000 f2=0012 f3=00ba f5=0000 mailbox=0012 flag={flag} cbh=0002 cbl=0981')
    lines += ['mu4_playback_execute_entry: address=026d62 opcode=fd00 t=9.8',
              'mu4_notify_terminal: events=5 cap=4096 observer_writes=0 audio_acceptance=0',
              'mu4_notify_stop: t=10.0 illegal=0',
              'mu4_playback_parent_terminal: first_session_complete=1 overlay_verified=1 illegal=0 opcode_mismatch=0 '
              'selection=first_verified_aa99_session identity=Execute_entry observer_writes=0 audio_acceptance=0']
    return '\n'.join(lines)


class NotificationTest(unittest.TestCase):
    def test_sites_agree_with_independently_pinned_source_spans(self):
        words = {base+i: value for base, witness in PLAYBACK_PENDING_CONTROL.items()
                 for i, value in enumerate(witness)}
        for address, opcode in SITES.values():
            self.assertEqual(words[address], opcode)

    def test_complete_context_chain(self):
        report = check(fixture())
        self.assertEqual(report['groups'], 1)
        self.assertEqual(report['requests'], {'0012': 1})
        self.assertEqual(report['selected_controls'], dict(input16=0,input17=0,receive16=0))
        self.assertIn('not retained operands', report['scope'])

    def test_rejects_incomplete_or_unverified_capture(self):
        text = fixture()
        for before, after in (('events=5','events=4'), ('cap=4096','cap=5000'),
                              ('illegal=0','illegal=1'), ('t=10.0','t=9.0'),
                              ('t=10.0','t=nan'), ('overlay_verified=1','overlay_verified=0'),
                              ('before_execution=1','before_execution=0'),
                              ('kind=publish','kind=unknown'), ('opcode=56f8','opcode=ffff'),
                              ('ordinal=3','ordinal=2'), ('cycles=3','cycles=2'),
                              ('sp=1000','sp=10000'), ('cbh=0002','cbh=0003'),
                              ('f2=0012','f2=0011'), ('flag=0001','flag=0000')):
            with self.subTest(before=before), self.assertRaises(ValueError):
                check(text.replace(before, after))

    def test_missing_publisher_cannot_be_absence_evidence(self):
        lines = [line for line in fixture().splitlines() if 'kind=publish' not in line]
        with self.assertRaises(ValueError):
            check('\n'.join(lines).replace('events=5','events=4'))

    def test_rejects_duplicate_terminal_and_missing_context(self):
        text = fixture()
        for bad in (text + '\nmu4_notify_stop: t=10 illegal=0',
                    text.replace(' f3=00ba', ''), text.replace('cbh=0002', 'cbh=0002 cbh=0002')):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check(bad)
