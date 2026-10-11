import unittest
import hashlib
from pathlib import Path
import tempfile

from tools.run_c54x_rom4_rf_delivery import check_repeats, input_provenance


class RepeatTests(unittest.TestCase):
    def test_provenance_hashes_actual_bytes_and_marks_missing_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'input.bin'
            path.write_bytes(b'own ROM input')
            result = input_provenance({'present': path, 'missing': path.with_name('missing')})
            self.assertEqual(result['present']['sha256'],
                             hashlib.sha256(b'own ROM input').hexdigest())
            self.assertEqual(result['present']['bytes'], 13)
            self.assertEqual(result['present']['path'], str(path.resolve()))
            self.assertFalse(result['missing']['present'])
            self.assertNotIn('sha256', result['missing'])

    def case(self, mode="tone", **changes):
        return dict(mode=mode, results=5146, accepted=9,
                    object="00101438", native_body="006d") | changes

    def test_identical_repeats(self):
        check_repeats([self.case(), self.case()])

    def test_modes_are_independent(self):
        check_repeats([self.case(), self.case("zero", results=5158, accepted=0)])

    def test_each_observable_must_repeat(self):
        for key, value in (("results", 5556), ("accepted", 8),
                           ("object", "00101480"), ("native_body", "0001")):
            with self.subTest(key=key), self.assertRaises(ValueError):
                check_repeats([self.case(), self.case(**{key: value})])


if __name__ == "__main__":
    unittest.main()
