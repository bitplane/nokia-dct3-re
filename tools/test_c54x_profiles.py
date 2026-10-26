import ast
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools import c54x_profiles as profiles


SOURCE = '''ROM_START(one)
ROM_SYSTEM_BIOS(0, "first", "First profile")
ROM_SYSTEM_BIOS(0x1, "second", "Second profile")
ROM_END
'''


class ProfileDeclarationTests(unittest.TestCase):
    def tearDown(self):
        profiles.declared_machines.cache_clear()

    def test_source_inventory_includes_compiled_modules_and_headers_only(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            (root / 'driver').mkdir()
            for filename in ('tms320c54x_test_state.h', 'tms320c54x_test.cpp',
                             'tms320c54x_test_rom4.cpp', 'tms320c54x_test_notes.md',
                             'tms320c54x.cpp', 'unrelated.h'):
                (root / 'driver' / filename).write_text('')
            self.assertEqual([p.name for p in profiles.harness_sources(root)], [
                'tms320c54x_test.cpp', 'tms320c54x_test_rom4.cpp',
                'tms320c54x_test_state.h'])

    def test_literal_metadata_ignores_commented_tables(self):
        actual = profiles.declarations('// ROM_START(fake)\n/*' + SOURCE + '*/\n' + SOURCE)
        self.assertEqual(actual, {'one': [
            dict(index=0, name='first', description='First profile'),
            dict(index=1, name='second', description='Second profile')]})

    def test_unsupported_conditional_and_duplicate_entries_fail_closed(self):
        for source in (SOURCE.replace('0x1,', '0,'), SOURCE.replace('"second"', '"first"'),
                       SOURCE.replace('0x1,', 'BASE + 1,'), SOURCE + SOURCE,
                       SOURCE.replace('ROM_SYSTEM_BIOS(0x1', '#if ENABLED\nROM_SYSTEM_BIOS(0x1')):
            with self.subTest(source=source), self.assertRaises(ValueError):
                profiles.declarations(source)

    def test_missing_and_ambiguous_profile_owners_fail_closed(self):
        with patch.object(profiles, 'declared_machines', return_value={'a': [{'name': 'first'}]}):
            self.assertEqual(profiles.machine_for_profile('first'), 'a')
            with self.assertRaises(ValueError):
                profiles.machine_for_profile('missing')
        with patch.object(profiles, 'declared_machines', return_value={
                'a': [{'name': 'first'}], 'b': [{'name': 'first'}]}):
            with self.assertRaises(ValueError):
                profiles.machine_for_profile('first')

    def test_duplicate_machine_across_modules_is_rejected(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            (root / 'driver').mkdir()
            for suffix in ('a', 'b'):
                (root / f'driver/tms320c54x_test_{suffix}.cpp').write_text(SOURCE)
            with self.assertRaises(ValueError):
                profiles.declared_machines(root)

    def test_oracles_remain_data_without_profile_or_cpu_imports(self):
        path = Path(__file__).resolve().parent / 'c54x_test_oracles.py'
        tree = ast.parse(path.read_text())
        self.assertIsInstance(tree.body[0].value, ast.Constant)
        self.assertTrue(all(isinstance(node, ast.Assign) for node in tree.body[1:]))


if __name__ == '__main__':
    unittest.main()
