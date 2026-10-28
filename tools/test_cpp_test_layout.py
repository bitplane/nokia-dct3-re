"""Check test ownership and the actual Make source selection without building MAME."""
from pathlib import Path
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[1]


def selected_sources(enabled):
    return subprocess.run(
        ['make', '--no-print-directory', '-f', 'Makefile', '-f', '-',
         'test-source-query', f'DCT3_TEST_DRIVERS={enabled}'],
        input="test-source-query:\n\t@printf '%s\\n' '$(BUILD_MAME_SOURCES)'\n",
        cwd=ROOT, check=True, capture_output=True, text=True).stdout.strip().split(',')


class CppTestLayoutTests(unittest.TestCase):
    def test_handset_build_excludes_every_synthetic_test_source(self):
        sources = selected_sources(0)
        self.assertIn('src/mame/nokia/nokia_dct3.cpp', sources)
        self.assertFalse(any('_test' in Path(source).name for source in sources))

    def test_conformance_build_includes_each_bench_once(self):
        sources = selected_sources(1)
        self.assertEqual(len(sources), len(set(sources)))
        files = [source for directory in ('c54x', 'devices', 'firmware')
                 for source in (ROOT / 'tests' / directory).glob('*.cpp')]
        self.assertTrue(files)
        self.assertEqual(len(files), len({source.name for source in files}))
        for source in files:
            with self.subTest(source=source.name):
                self.assertIn(f'src/mame/nokia/{source.name}', sources)

    def test_invalid_selection_fails_closed(self):
        with self.assertRaises(subprocess.CalledProcessError):
            selected_sources('unknown')

    def test_test_sources_are_not_production_device_files(self):
        self.assertFalse(list((ROOT / 'driver').glob('*test*.cpp')))
        self.assertFalse(list((ROOT / 'driver').glob('*test*.h')))


if __name__ == '__main__':
    unittest.main()
