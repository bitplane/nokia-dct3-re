import unittest

from tools.bench_c54x_codec import frame_stats, paired_budget, parse_frames


class CodecTimingTests(unittest.TestCase):
    def test_frame_order_and_positive_duration(self):
        line = 'ROM4 codec frame timing: index={} host_seconds={} replay_included=0\n'
        text = line.format(0, '.010') + line.format(1, '.020')
        self.assertEqual(parse_frames(text, 2), [.010, .020])
        for invalid in (text + line.format(1, '.020'), line.format(1, '.010'),
                        line.format(0, '0') + line.format(1, '.020')):
            with self.assertRaises(ValueError):
                parse_frames(invalid, 2)

    def test_tail_statistics_and_strict_deadline(self):
        result = frame_stats([.010, .020, .030])
        self.assertEqual(result['over_20ms'], 1)
        self.assertEqual(result['p95_ms'], 30)
        self.assertEqual(result['worst_ms'], 30)
        with self.assertRaises(ValueError):
            frame_stats([])

    def test_paired_budget_adds_idle_cost_not_percentages(self):
        result = paired_budget([[.010, .010]], [[.005, .005]], .25)
        self.assertAlmostEqual(result['estimated_realtime_percent'], 100)
        self.assertEqual(result['runs'][0]['mean_ms'], 20)
        self.assertIn('not concurrent', result['scope'])

    def test_mismatched_runs_or_frames_are_rejected(self):
        for encoder, decoder in (([[.010]], []), ([[.010]], [[.010, .010]])):
            with self.assertRaises(ValueError):
                paired_budget(encoder, decoder)


if __name__ == '__main__':
    unittest.main()
