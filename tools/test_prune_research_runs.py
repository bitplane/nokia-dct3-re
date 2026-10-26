import os
from pathlib import Path
import tempfile
import time
import unittest
from unittest.mock import patch

from tools.prune_research_runs import apply, inventory, references


class PruneTests(unittest.TestCase):
    def test_references_include_expanded_prefixes(self):
        self.assertEqual(references('tmp/abc-{old,new} tmp/exact/file',
                                   {'abc-old', 'abc-new', 'exact', 'other'}),
                         {'abc-old', 'abc-new', 'exact'})

    def setup_root(self, root):
        (root / 'tmp').mkdir()
        (root / 'notes.md').write_text('tmp/kept/output\n')
        for name in ('kept', 'dependency', 'old', 'recent', 'source', 'unknown'):
            run = root / 'tmp' / name
            run.mkdir()
            (run / ('other.txt' if name == 'unknown' else 'console.log')).write_text('output')
        (root / 'tmp/kept/inputs.json').write_text('{"source":"tmp/dependency/seed"}')
        (root / 'tmp/source/original.nand').write_bytes(b'valuable')
        ago = time.time() - 3 * 86400
        for path in (root / 'tmp').rglob('*'):
            if path.is_file() and 'recent' not in path.parts:
                os.utime(path, (ago, ago))

    def test_reference_closure_and_safe_defaults(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            self.setup_root(root)
            with patch('tools.prune_research_runs.subprocess.check_output', return_value=b'notes.md\0'):
                report = inventory(root)
            self.assertEqual([row['name'] for row in report['runs'] if row['delete']], ['old'])

    def test_apply_only_unchanged_unreferenced_run(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            self.setup_root(root)
            with patch('tools.prune_research_runs.subprocess.check_output', return_value=b'notes.md\0'), \
                 patch('tools.prune_research_runs.active_run', return_value=False):
                report = inventory(root)
                self.assertEqual(apply(root, report)[0], 1)
            self.assertFalse((root / 'tmp/old').exists())
            self.assertTrue((root / 'tmp/source/original.nand').exists())

    def test_compressed_logs_keep_the_same_retention_rules(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            self.setup_root(root)
            for path in (root / 'tmp').glob('*/console.log'):
                path.rename(path.with_suffix('.log.zst'))
            with patch('tools.prune_research_runs.subprocess.check_output', return_value=b'notes.md\0'), \
                 patch('tools.prune_research_runs.active_run', return_value=False):
                report = inventory(root)
                self.assertEqual([row['name'] for row in report['runs'] if row['delete']], ['old'])
                self.assertEqual(apply(root, report)[0], 1)
            self.assertTrue((root / 'tmp/kept/console.log.zst').exists())
            self.assertTrue((root / 'tmp/dependency/console.log.zst').exists())
            self.assertTrue((root / 'tmp/source/original.nand').exists())
            self.assertTrue((root / 'tmp/recent/console.log.zst').exists())

    def test_changed_active_or_newly_referenced_run_is_rejected(self):
        for mode in ('changed', 'active', 'referenced'):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as name:
                root = Path(name)
                self.setup_root(root)
                with patch('tools.prune_research_runs.subprocess.check_output', return_value=b'notes.md\0'), \
                     patch('tools.prune_research_runs.active_run', return_value=mode == 'active'):
                    report = inventory(root)
                    if mode == 'changed':
                        (root / 'tmp/old/console.log').write_text('new evidence')
                    elif mode == 'referenced':
                        (root / 'notes.md').write_text('tmp/kept tmp/old')
                    with self.assertRaises(ValueError):
                        apply(root, report)
                    self.assertTrue((root / 'tmp/old').exists())


if __name__ == '__main__':
    unittest.main()
