import struct
import unittest

from tools.mu4_decoder_session_check import check_session


def fixture():
    data = struct.pack('<4096h',*range(4096))
    fingerprint = 2166136261
    for byte in data:
        fingerprint = ((fingerprint ^ byte)*16777619) & 0xffffffff
    lines = []
    for call in range(1,4):
        for phase,time in (('entry',call),('return',call+0.1)):
            result = f' result={1 if call==3 else 0:010x}' if phase == 'return' else ''
            lines.append(f'mu4_decoder_call: call={call} phase={phase}{result} t={time:.9f} '
                         'identity=Execute_entry context_only=1 observer_writes=0')
        if call < 3:
            lines.append(f'mu4_decoder_block: call={call} frame={call-1} words={2048*call} bad=0 '
                         f't={call+0.2:.9f} identity=Execute_entry retirement=unclaimed observer_writes=0')
    lines.append(f'mu4_decoded_return_capture: calls=3 returns=3 blocks=2 words=4096 bytes=8192 '
                 f'fnv1a={fingerprint:08x} bad=0 pending=0 capped=0 encoding=le16 file=mu4_decoded_returns.le16 '
                 'retirement=unclaimed save_restore=unsupported observer_writes=0 audio_acceptance=0')
    return '\n'.join(lines),data


class DecoderSessionTests(unittest.TestCase):
    def test_failure_and_reference_extent_are_not_hidden(self):
        text,data = fixture()
        report = check_session(text,data,data+data[:4096])
        self.assertEqual(report['calls'],3)
        self.assertEqual(report['converted_blocks'],2)
        self.assertEqual(report['failed_calls'][0]['call'],3)
        self.assertEqual(report['reference_blocks_not_compared'],1)
        self.assertEqual(report['frame_metrics'][1]['max_absolute_error'],0)
        self.assertFalse(report['native_audio_validated'])
        self.assertFalse(report['fitted_alignment'])
        report = check_session(text,data,data[:4096])
        self.assertEqual(report['native_blocks_not_compared'],1)

    def test_wrong_order_scope_counts_hash_and_partial_outputs_are_rejected(self):
        text,data = fixture()
        for bad in (text.replace('calls=3 returns=3','calls=3 returns=2'),
                    text.replace('pending=0','pending=1'),text.replace('capped=0','capped=1'),
                    text.replace('bad=0','bad=1'),text+'\n'+text.splitlines()[-1],
                    text.replace('call=2 phase=entry','call=1 phase=entry'),
                    text.replace('t=1.200000000','t=2.000000000'),
                    text.replace('result=0000000001','result=0000000000'),
                    text.replace('retirement=unclaimed','retirement=claimed'),
                    text.replace('context_only=1','context_only=0'),
                    text.replace('words=4096 bad=0','words=4095 bad=0')):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_session(bad,data,data)
        for bad in (data[:-1],data[:-2],bytes(len(data))):
            with self.assertRaises(ValueError):
                check_session(text,bad,data)
        for reference in (b'',data[:-1],data[:-2]):
            with self.assertRaises(ValueError):
                check_session(text,data,reference)

    def test_waveform_difference_is_reported_not_fitted_or_called_conformant(self):
        text,data = fixture()
        reference = struct.pack('<4096h',*([0]*4096))
        report = check_session(text,data,reference)
        self.assertEqual(report['frame_metrics'][1]['max_absolute_error'],4095)
        self.assertFalse(report['native_audio_validated'])
        self.assertFalse(report['fitted_gain'])
        self.assertEqual(report['reference_comparison_diagnostic']['max_absolute_error'],4095)

    def test_return_status_uses_the_callers_low_word_not_stale_upper_bits(self):
        text,data = fixture()
        report = check_session(text.replace('result=0000000000','result=0000010000'),data,data)
        self.assertEqual(report['converted_blocks'],2)
        self.assertEqual(report['failed_calls'][0]['status_word'],'0001')

    def test_reference_comparison_does_not_collapse_a_failed_middle_call(self):
        text,data = fixture()
        lines = text.splitlines()
        block = next(line for line in lines if 'mu4_decoder_block: call=2' in line)
        lines.remove(block)
        lines.insert(-1,block.replace('call=2','call=3').replace('t=2.200000000','t=3.200000000'))
        text = '\n'.join(lines).replace('call=2 phase=return result=0000000000','call=2 phase=return result=0000000001')
        text = text.replace('call=3 phase=return result=0000000001','call=3 phase=return result=0000000000')
        report = check_session(text,data,data)
        self.assertEqual(report['compared_blocks'],1)
        self.assertEqual(report['native_blocks_not_compared'],1)
        self.assertTrue(report['comparison_stops_at_first_failure'])


if __name__ == '__main__':
    unittest.main()
