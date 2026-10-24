import unittest

from tools.run_efr_mixing_replay import total_records


def fixture():
    lines = []
    for frame in range(2):
        group = ['efr_boundary: kind=entry']
        for sub in range(4):
            group.append('efr_mixing: kind=TOTAL values=' + f'{frame * 4 + sub:04x}' * 40)
        lines.extend(group * 2)
    return '\n'.join(lines)


class MixingRecordsTests(unittest.TestCase):
    def test_four_complete_vectors_per_original_and_replay_frame(self):
        records = total_records(fixture(), frames=2)
        self.assertEqual(len(records), 2)
        self.assertEqual(records[1][3], (7,) * 40)

    def test_changed_replay_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'replay'):
            total_records(fixture().replace('values=0000', 'values=0001', 1), frames=2)

    def test_short_or_missing_observations_are_rejected(self):
        with self.assertRaisesRegex(ValueError, 'malformed'):
            total_records(fixture().replace('values=', 'values=0000'), frames=2)
        with self.assertRaisesRegex(ValueError, 'incomplete'):
            total_records(fixture().rsplit('efr_boundary: kind=entry', 1)[0], frames=2)
        with self.assertRaisesRegex(ValueError, 'unowned'):
            total_records(fixture().splitlines()[1], frames=2)
