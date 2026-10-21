"""Cold PUK-chain orchestration preserves actual predecessor storage."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.nsb7_puk_check import card_state
from tools.nsb7_puk_exhaustion import run_chain
from tools.test_nsb7_puk_check import fixture


class ExhaustionTest(unittest.TestCase):
    def test_existing_output_is_never_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaises(ValueError):
                run_chain(root, root/'missing', root)

    def test_complete_predecessor_chain(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root/'source/nvram/nsb7hle'
            source.mkdir(parents=True)
            (source/'sim_card').write_bytes(card_state(0))
            (source/'flash').write_bytes(b'own flash')
            (source/'ccont').write_bytes(b'own clock')
            config = root/'fixtures/nsb7_sim_security'
            config.mkdir(parents=True)
            (config/'nsb7hle.cfg').write_text('own config')
            calls = []

            def execute(command, **kwargs):
                run = Path(command[command.index('--run-dir')+1])
                calls.append(run)
                card = run/'nvram/nsb7hle/sim_card'
                if run.name == 'permanent-cold':
                    self.assertEqual(card.read_bytes(), card_state(0, 0))
                    text = 'nsb7_puk_permanent: complete'
                else:
                    attempt = int(run.name.split('-')[1])
                    remaining = 10-attempt
                    self.assertEqual(card.read_bytes(), card_state(0, remaining+1))
                    card.write_bytes(card_state(0, remaining))
                    text = fixture(True).replace('puk=9/10', f'puk={remaining}/10')
                    if not remaining:
                        text = text.replace('sw=9804', 'sw=9840')
                (run/'error.log').write_text(text)

            with patch('tools.nsb7_puk_exhaustion.inspect'), patch(
                    'tools.nsb7_puk_exhaustion.check_frame') as pixels, patch(
                    'tools.nsb7_puk_exhaustion.subprocess.run', side_effect=execute):
                run_chain(root, root/'source', root/'output')
            self.assertEqual(pixels.call_count, 1)
            self.assertEqual(len(calls), 11)
            self.assertEqual((source/'sim_card').read_bytes(), card_state(0))


if __name__ == '__main__':
    unittest.main()
