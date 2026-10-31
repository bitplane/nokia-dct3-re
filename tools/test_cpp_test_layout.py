"""Check test ownership and the actual Make source selection without building MAME."""
from pathlib import Path
import subprocess
import shlex
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]


def selected_sources(enabled):
    return subprocess.run(
        ['make', '--no-print-directory', '-f', 'Makefile', '-f', '-',
         'test-source-query', f'DCT3_TEST_DRIVERS={enabled}'],
        input="test-source-query:\n\t@printf '%s\\n' '$(BUILD_MAME_SOURCES)'\n",
        cwd=ROOT, check=True, capture_output=True, text=True).stdout.strip().split(',')


class CppTestLayoutTests(unittest.TestCase):
    def test_test_sources_follow_mame_headers(self):
        # Source hygiene only, not an executed hardware/processor contract.
        for source in (ROOT / 'tests').rglob('*'):
            if source.suffix not in ('.cpp', '.h'):
                continue
            with self.subTest(source=source.relative_to(ROOT)):
                text = source.read_text()
                first = text.splitlines()[:2]
                self.assertEqual(first[0], '// license:BSD-3-Clause')
                self.assertTrue(first[1].startswith('// copyright-holders:'))
                if source.suffix == '.h':
                    self.assertNotIn('#pragma once', text)
                    self.assertNotIn('#include "emu.h"', text)
                    # Case generators are single-include translation-unit inputs,
                    # selected by the runner, not independently reusable headers.
                    fragment = (source.parent.name == 'sections' and source.name != 'common.h'
                                or source.name.endswith('_section.h')
                                or source.parent.name == 'firmware' and source.name.startswith('c54x_'))
                    if not fragment:
                        guard = re.search(r'^#ifndef (\w+)\n#define \1\b', text, re.M)
                        self.assertIsNotNone(guard)
                        self.assertIn(f'#endif // {guard[1]}', text)

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

    def test_cpu_only_build_uses_an_independent_subtarget_without_rom_preparation(self):
        output = subprocess.run(
            ['make', '--no-print-directory', '--dry-run', 'build-c54x-isa',
             'C54X_SECTION=immediate_store', 'JOBS=2'],
            cwd=ROOT, check=True, capture_output=True, text=True).stdout
        commands = [shlex.split(line) for line in output.splitlines() if 'run_c54x_section.py' in line]
        self.assertEqual(commands, [['python3', 'tools/run_c54x_section.py', 'immediate_store',
                                    '--build-only', '--jobs', '2']])
        self.assertNotIn('make_eeprom_profile.py', output)

    def test_invalid_selection_fails_closed(self):
        with self.assertRaises(subprocess.CalledProcessError):
            selected_sources('unknown')

    def test_test_sources_are_not_production_device_files(self):
        self.assertFalse(list((ROOT / 'driver').glob('*test*.cpp')))
        self.assertFalse(list((ROOT / 'driver').glob('*test*.h')))


if __name__ == '__main__':
    unittest.main()
