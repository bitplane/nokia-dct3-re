import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools import efr_reference_calculator as calculator


class ReferenceCalculatorTests(unittest.TestCase):
    def run_calculator(self, *, mode='filter', bad_pcm=False, unstable=False, changed=False):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / 'source'
            source.mkdir()
            for name in ('decoder', 'syn_filt.c', 'pred_lt6.c', 'basicop2.c',
                         'basic_op.h', 'typedef.h', 'count.c', 'count.h', 'agc.c',
                         'inv_sqrt.c', 'inv_sqrt.tab', 'sig_proc.h', 'cnst.h'):
                (source / name).write_bytes(b'original source or executable')
            (source / 'spch_dos.dec').write_bytes(b'installation input')
            expected = dict(dec=(None, hashlib.sha256(b'installation input').hexdigest()),
                            out=(None, hashlib.sha256(b'installation output').hexdigest()))
            commands = []

            def execute(command, **kwargs):
                commands.append(command)
                if command[0] == 'gcc':
                    Path(command[-1]).write_bytes(b'calculator')
                    if changed:
                        (source / 'pred_lt6.c').write_bytes(b'changed source')
                elif command[0] == str(source / 'decoder'):
                    Path(command[-1]).write_bytes(b'bad PCM' if bad_pcm else b'installation output')
                else:
                    Path(command[-1]).write_bytes(b'changed' if unstable and
                                                  command[-1].endswith('replay.bin') else b'calculated')

            with patch.object(calculator, 'EXPECTED', expected), \
                    patch.object(calculator.subprocess, 'run', side_effect=execute):
                result, evidence = calculator.calculate(source, root / 'output', b'arguments',
                                                         mode=mode)
            self.assertIn('-fwrapv', commands[0])
            self.assertEqual(commands[0][-1], str(root / 'output/reference-calculator'))
            self.assertEqual('--predictor' in commands[-1], mode == 'predictor')
            self.assertEqual('--mix' in commands[-1], mode == 'mix')
            self.assertEqual('--agc' in commands[-1], mode == 'agc')
            self.assertEqual(result, b'calculated')
            self.assertTrue(evidence['installation_verified'])
            self.assertTrue(evidence['fresh_process_replay'])
            self.assertEqual(len(evidence['calculator_sha256']), 64)

    def test_both_modes_rebuild_and_preserve_provenance(self):
        self.run_calculator()
        self.run_calculator(mode='predictor')
        self.run_calculator(mode='mix')
        self.run_calculator(mode='agc')

    def test_bad_installation_pcm_rejected(self):
        with self.assertRaisesRegex(ValueError, 'installation PCM'):
            self.run_calculator(bad_pcm=True)

    def test_changed_calculation_rejected(self):
        with self.assertRaisesRegex(ValueError, 'fresh processes'):
            self.run_calculator(unstable=True)

    def test_changed_reference_source_rejected(self):
        with self.assertRaisesRegex(ValueError, 'sources changed'):
            self.run_calculator(changed=True)

    def test_invalid_mode_fails_before_creating_artifacts(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'unused'
            with self.assertRaisesRegex(ValueError, 'unknown'):
                calculator.calculate(Path(directory), output, b'', mode='unknown')
            self.assertFalse(output.exists())
