from pathlib import Path
import tempfile
import unittest
from tools.make_sim_card_profile import make_profile
from tools.sim_fdn_profile import extend
from tools.nsb1_fdn_check import expected_card
from tools.nsb1_fdn_prepare import prepare


class Nsb1FdnTests(unittest.TestCase):
    def card(self):
        base = bytearray(make_profile(True))
        base.extend(bytes(3570 - len(base)))
        return extend(base, True)

    def test_only_validity_changes_during_four_phases(self):
        before = self.card()
        for phase in range(4):
            after = expected_card(before, phase)
            changed = [i for i, (a, b) in enumerate(zip(before, after)) if a != b]
            self.assertEqual(changed, [] if phase % 2 else [3575])
            before = after

    def test_wrong_state_or_profile_rejected(self):
        card = self.card()
        with self.assertRaises(ValueError):expected_card(card, 1)
        with self.assertRaises(ValueError):expected_card(card, 4)
        with self.assertRaises(ValueError):expected_card(card[:-1], 0)
        bad = bytearray(card)
        bad[3573] = 2
        with self.assertRaises(ValueError):expected_card(bad, 0)

    def test_preparation_rejects_already_extended_card_before_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'source'
            storage = source / 'nvram/nsb1r4t'
            storage.mkdir(parents=True)
            (storage / 'sim_card').write_bytes(self.card())
            output = root / 'output'
            with self.assertRaises(ValueError):prepare(source, output)
            self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
