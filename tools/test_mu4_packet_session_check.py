import unittest

from tools.mu4_packet_session_check import check_packets, extract_observed_packets


def fixture():
    packet = bytes.fromhex('fff95080021ffc') + bytes(range(1,10))
    lines = ['mu4_packet_read_scope: first=0.8 last=10.0 first_address=162e last_address=1e2d cap=131072 ownership=data_bus pc=context_only observer_writes=0 audio_acceptance=0',
             'mu4_decoder_call: call=1 phase=entry base=162e index=0001 remaining=0010 reservoir=0000fff9 t=1.0 identity=Execute_entry context_only=1 observer_writes=0']
    for index, offset in enumerate(range(2,16,2),1):
        lines.append(f'mu4_packet_read: ordinal={index} address={0x162e+offset//2:04x} word={packet[offset:offset+2].hex()} '
                     f'mask=ffff t={1.1+offset*.01:.4f} context_pc=025110 observer_writes=0')
    lines += ['mu4_decoder_call: call=1 phase=return result=0000000000 index=0008 remaining=0000 t=2.0 identity=Execute_entry context_only=1 observer_writes=0',
              'mu4_packet_read_terminal: reads=7 closed=1 t=20.0 illegal=0 observer_writes=0 audio_acceptance=0']
    return '\n'.join(lines), packet


class PacketSessionTest(unittest.TestCase):
    def test_extracts_only_observed_bytes(self):
        text, packet = fixture()
        data, report = extract_observed_packets(text)
        self.assertEqual(data, packet)
        self.assertFalse(report['reference_independent'])
        extended = text.replace('last=10.0', 'last=12.0')
        self.assertEqual(extract_observed_packets(extended, window_last=12.0)[0], packet)
        with self.assertRaises(ValueError):
            extract_observed_packets(extended)

    def test_extraction_rejects_gaps_conflicts_and_bad_framing(self):
        text, _ = fixture()
        missing = '\n'.join(line for line in text.splitlines() if 'ordinal=7 ' not in line)
        for changed in (missing, text.replace('word=5080', 'word=ffff'),
                        text.replace('reservoir=0000fff9', 'reservoir=00000000')):
            with self.assertRaises(ValueError):
                extract_observed_packets(changed)
        duplicate = next(line for line in text.splitlines() if 'ordinal=1 ' in line)
        with self.assertRaises(ValueError):
            extract_observed_packets(text + '\n' + duplicate.replace('word=5080', 'word=ffff'))

    def test_prefix_is_complete_but_tail_is_explicit(self):
        text, packet = fixture()
        report = check_packets(text,packet+packet)
        self.assertEqual(report['calls'],1)
        self.assertEqual(report['consumed_prefix_bytes'],16)
        self.assertEqual(report['unconsumed_reference_bytes'],16)
        self.assertEqual(report['remaining_complete_adts_headers'],1)
        self.assertEqual(report['packets'][0]['covered_bytes'],16)
        self.assertFalse(report['native_audio_validated'])
        self.assertTrue(report['single_raw_block_packets'])

    def test_malformed_tail_is_not_hidden_or_promoted(self):
        text, packet = fixture()
        report = check_packets(text,packet+b'bad')
        self.assertEqual(report['unconsumed_reference_bytes'],3)
        self.assertEqual(report['remaining_unparsed_bytes'],3)
        self.assertEqual(report['remaining_complete_adts_headers'],0)

    def test_rejects_partial_faulted_wrong_domain_or_changed_byte(self):
        text, packet = fixture()
        for before, after in (('reads=7','reads=6'), ('closed=1','closed=0'),
                              ('illegal=0','illegal=1'), ('t=20.0','t=9.0'),
                              ('mask=ffff','mask=ff00'), ('ordinal=3','ordinal=2'),
                              ('context_pc=025110','context_pc=025144'),
                              ('base=162e','base=0000'), ('phase=return','phase=entry'),
                              ('result=0000000000','result=0000000001'),
                              ('remaining=0010','remaining=000f'), ('word=5080','word=ffff')):
            with self.subTest(before=before), self.assertRaises(ValueError):
                check_packets(text.replace(before,after),packet)
        with self.assertRaises(ValueError):
            check_packets(text,packet[:-1])
        with self.assertRaises(ValueError):
            check_packets(text.replace(' base=162e',''),packet)

    def test_no_missing_bus_word_can_be_replaced_by_reference_data(self):
        text, packet = fixture()
        lines = [line for line in text.splitlines() if 'ordinal=7 ' not in line]
        with self.assertRaises(ValueError):
            check_packets('\n'.join(lines).replace('reads=7','reads=6'),packet)
