import struct
import unittest

from tools.mu4_cleanup_buffer_check import FIRST, WORDS, inspect_ram, check, compare_projection_tail


def fixture():
    data = bytearray(WORDS*2)
    for address, value in ((0x15e9, 0x160a), (0x160a, 0x162e), (0x160b, 0x1632)):
        struct.pack_into('<H', data, 2*(address-FIRST), value)
    header = bytes.fromhex('fff9508000fffc00')
    for index in range(0, len(header), 2):
        struct.pack_into('<H', data, 2*(0x162e-FIRST)+index, int.from_bytes(header[index:index+2], 'big'))
    return bytes(data)


class CleanupBufferTest(unittest.TestCase):
    def test_projection_tail_uses_processing_ordinal_not_search(self):
        data = fixture()
        result = inspect_ram(data, 0x160a, 0x162e, 0x1632)
        result['bit_reader']['base'] = '162e'
        text = '\n'.join([
            'mu4_decoder_call: call=1 phase=entry identity=Execute_entry context_only=1 observer_writes=0',
            'mu4_decoder_call: call=1 phase=return result=0000000000 identity=Execute_entry context_only=1 observer_writes=0',
            'mu4_decoder_block: call=1 frame=0 words=2048 bad=0 identity=Execute_entry retirement=unclaimed observer_writes=0'])
        reference = bytes.fromhex('fff9508000fffc')*2
        report = compare_projection_tail(text, data, result, reference)
        self.assertEqual(report['reference_offset'], 7)
        self.assertEqual(report['equal_tail_bytes'], 7)
        self.assertEqual(report['uncompared_buffer_bytes'], 1)
        for altered in (reference[:-1]+b'X', reference[:7]):
            with self.assertRaises(ValueError):
                compare_projection_tail(text, data, result, altered)
        with self.assertRaises(ValueError):
            compare_projection_tail(text.replace('result=0000000000', 'result=0000000001'), data, result, reference)
        result['bit_reader']['remaining'] = 1
        with self.assertRaises(ValueError):
            compare_projection_tail(text, data, result, reference)
        # The source reader keeps the valid reservoir bits at the low end.
        captured = bytearray(fixture())
        tail = reference[10:] + bytes(4)
        for index in range(0, len(tail), 2):
            struct.pack_into('<H', captured, 2*(0x162e-FIRST)+index,
                             int.from_bytes(tail[index:index+2], 'big'))
        for address, value in ((0xa854, 24), (0xa858, 0xe0ff), (0xa859, 0xf950), (0xb332, 0x162e)):
            struct.pack_into('<H', captured, 2*(address-FIRST), value)
        result = inspect_ram(captured, 0x160a, 0x162e, 0x1632)
        report = compare_projection_tail(text, captured, result, reference)
        self.assertEqual(report['reservoir_prefix_hex'], 'fff950')
        self.assertEqual(report['equal_tail_bytes'], 7)
        result['bit_reader']['reservoir'] = 'e1fff950'
        self.assertEqual(compare_projection_tail(text, captured, result, reference)['equal_tail_bytes'], 7)
        result['bit_reader']['reservoir'] = 'e1fff951'
        with self.assertRaises(ValueError):
            compare_projection_tail(text, captured, result, reference)

    @staticmethod
    def log():
        return '\n'.join([
            'mu4_cleanup_buffer_scope: first=0080 words=65408 encoding=le16 identity=debugger_Execute_hook operands=context_only observer_writes=0 audio_acceptance=0',
            'mu4_playback_execute_entry: address=026d62 opcode=fd00 t=0.2',
            'mu4_native_playback_overlay: PASS marker=aa99 records=3127 words=49843 checked_ram_words=49839 excluded_register_words=4 before_execution=1',
            'mu4_cleanup_buffer: events=1 address=02956f cycles=123 object=160a cursor=162e limit=1632 file_buffer=0000 lifecycle=0000 identity=debugger_Execute_hook operands=context_only observer_writes=0',
            'mu4_playback_execute_entry: address=026d62 opcode=fd00 t=9.86',
            'mu4_cleanup_buffer_terminal: events=1 observer_writes=0 audio_acceptance=0',
            'mu4_cleanup_buffer_stop: t=10 illegal=0 observer_writes=0 audio_acceptance=0',
            'mu4_playback_parent_terminal: first_session_complete=1 overlay_verified=1 illegal=0 opcode_mismatch=0 observer_writes=0'])

    def test_session_scope_and_ram_context_are_checked(self):
        self.assertEqual(check(self.log(), fixture())['cycles'], 123)
        for before, after in [('events=1', 'events=2'), ('illegal=0', 'illegal=1'),
                              ('words=49843', 'words=1'), ('lifecycle=0000', 'lifecycle=0001'),
                              ('file_buffer=0000', 'file_buffer=0001'),
                              ('operands=context_only', 'operands=retired'),
                              ('observer_writes=0', 'observer_writes=1'),
                              ('t=10', 't=9.5'), ('encoding=le16', 'encoding=be16')]:
            with self.subTest(before=before, after=after), self.assertRaises(ValueError):
                check(self.log().replace(before, after), fixture())

    def test_reports_both_views_without_promoting_headers_to_payload_proof(self):
        result = inspect_ram(fixture(), 0x160a, 0x162e, 0x1632)
        self.assertEqual(result['available_words'], 4)
        self.assertEqual(result['byte_views']['high_byte_first']['first_candidate_chain'], [0])
        self.assertEqual(result['byte_views']['low_byte_first']['candidate_count'], 0)

    def test_rejects_partial_wrapped_and_unbound_snapshots(self):
        for data, obj, cursor, limit in ((fixture()[:-1], 0x160a, 0x162e, 0x1632),
                                         (fixture(), 0x160b, 0x162e, 0x1632),
                                         (fixture(), 0x160a, 0x162e, 0x1633),
                                         (fixture(), 0x160a, 0x1632, 0x162e)):
            with self.subTest(obj=obj, cursor=cursor, limit=limit), self.assertRaises(ValueError):
                inspect_ram(data, obj, cursor, limit)


if __name__ == '__main__':
    raise SystemExit(unittest.main())
