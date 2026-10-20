import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from tools.make_sim_card_profile import make_profile
from tools.nsb1_puk2_check import exhausted_card, rejected_puk_card
from tools.nsb1_puk2_exhaust import check_chain
from tools.sim_security_trace_check import ATTEMPTS_OFFSET


class Nsb1Puk2ExhaustTests(unittest.TestCase):
    def test_full_chain_rechecks_each_saved_card(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            seed, blocked, wrong, output = [root / name for name in ('seed', 'blocked', 'wrong', 'output')]
            base = bytearray(make_profile(True))
            base.extend(bytes(3570 - len(base)))
            for run, card in ((seed, base), (blocked, exhausted_card(base, 0)), (wrong, rejected_puk_card(base))):
                storage = run / 'nvram/nsb1r4t'
                storage.mkdir(parents=True)
                (storage / 'sim_card').write_bytes(card)
            (wrong / 'exit_status.json').write_text(json.dumps({'returncode': 0}))
            for remaining in range(8, -1, -1):
                run = output / f'retries-{remaining}'
                storage = run / 'nvram/nsb1r4t'
                storage.mkdir(parents=True)
                card = bytearray(exhausted_card(base, 0))
                card[ATTEMPTS_OFFSET + 3] = remaining
                (storage / 'sim_card').write_bytes(card)
                (run / 'exit_status.json').write_text(json.dumps({'returncode': 0}))

            def observe(run, code, seconds):
                self.assertEqual((code, seconds), (0, 80))
                remaining = 9 if run == wrong else int(run.name.split('-')[-1])
                contacts = '\n'.join(f'nsb1_pin_contact: phase={phase} index={i} down={down} t=12.0 physical_only=1'
                                     for phase, count in (('pin', 5), ('phone', 6)) for i in range(1, count + 1) for down in (1, 0))
                contacts += '\nSIM status ins=20 sw=9000 chv=3/0 puk=10/10 enabled=1\n'
                contacts += f'SIM status ins=2c sw={0x9804 if remaining else 0x9840:04x} chv=3/0 puk=10/{remaining} enabled=1\n'
                contacts += '\n'.join(f'nsb1_pin_change_contact: index={i} down={down} t=26.0 physical_only=1'
                                      for i in range(1, 25) for down in (1, 0))
                return contacts, (run / 'nvram/nsb1r4t/sim_card').read_bytes()

            with patch('tools.nsb1_puk2_exhaust.check_cold_link'), patch('tools.nsb1_puk2_exhaust.check_card_run', side_effect=observe):
                self.assertEqual(check_chain(seed, blocked, wrong, output), output / 'retries-0')
                target = output / 'retries-4/nvram/nsb1r4t/sim_card'
                card = bytearray(target.read_bytes())
                card[0] ^= 1
                target.write_bytes(card)
                with self.assertRaisesRegex(ValueError, 'complete saved'):
                    check_chain(seed, blocked, wrong, output)


if __name__ == '__main__':
    unittest.main()
