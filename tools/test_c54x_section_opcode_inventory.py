import json
from pathlib import Path
import tempfile
import unittest

from tools.c54x_opcode_coverage import summarize
from tools.c54x_section_opcode_inventory import fixture_log, inventory


def suite(root, *, complete=True, alpha_cases=2, beta_log=True):
    (root / 'fixtures').mkdir()
    (root / 'fixtures/c54x_sections.json').write_text(json.dumps({'alpha': 2, 'beta': 1}))
    sections = root / 'tests/c54x/sections'
    sections.mkdir(parents=True)
    trial = root / 'tmp/c54x-section-trial'
    for name, cases, log in (('alpha', alpha_cases, '[opcov] op=6a05 first_pc=000600 count=3\n'),
                             ('beta', 1, '[opcov] op=6a05 first_pc=000600 count=1\n'
                                         '[opcov] op=f495 first_pc=000601 count=2\n')):
        (sections / f'{name}.h').write_text('')
        (trial / name).mkdir(parents=True)
        (trial / name / 'console.log').write_text(f'C54x section: PASS cases={cases} restores={cases}\n')
        if name == 'alpha' or beta_log:
            (trial / name / 'error.log').write_text(log)
    (trial / 'all-summary.json').write_text(json.dumps(dict(
        complete=complete, completed=[dict(section='alpha'), dict(section='beta')])))


class SectionOpcodeInventoryTest(unittest.TestCase):
    def test_passing_sections_assert_their_executed_opcodes(self):
        with tempfile.TemporaryDirectory() as directory:
            suite(Path(directory))
            opcodes = inventory(root=Path(directory))
        self.assertEqual(opcodes, {0x6a05: {'alpha': 3, 'beta': 1}, 0xf495: {'beta': 2}})
        result = summarize(fixture_log(opcodes))
        self.assertEqual(set(result['first_pc']), {0x6a05, 0xf495})

    def test_section_without_admitted_opcodes_contributes_nothing(self):
        with tempfile.TemporaryDirectory() as directory:
            suite(Path(directory))
            (Path(directory) / 'tmp/c54x-section-trial/beta/error.log').write_text('Soft reset\n')
            self.assertEqual(inventory(root=Path(directory)), {0x6a05: {'alpha': 3}})

    def test_incomplete_or_unproved_runs_are_rejected(self):
        for options in (dict(complete=False), dict(alpha_cases=1), dict(beta_log=False)):
            with self.subTest(**options), tempfile.TemporaryDirectory() as directory:
                suite(Path(directory), **options)
                with self.assertRaises(ValueError):
                    inventory(root=Path(directory))


if __name__ == '__main__':
    unittest.main()
