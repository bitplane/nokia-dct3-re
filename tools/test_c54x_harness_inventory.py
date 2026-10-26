import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.c54x_harness_inventory import BASELINE, cpp_inventory, inventory
from tools import run_c54x_core as runner


class HarnessInventoryTests(unittest.TestCase):
    def test_current_contract_matches_frozen_baseline(self):
        self.assertEqual(inventory(), json.loads(BASELINE.read_text()))

    def test_cpp_formats_join_adjacent_literals_and_keep_duplicates(self):
        source = r'''ROM_START(test)
ROM_SYSTEM_BIOS(0, "core", "Core tests")
ROM_END
osd_printf_info("Test: PASS cases=2 " "restore=2\n");
osd_printf_info("Test: PASS cases=2 restore=2\n");
expect(true, "sample");
expect_opcode(0, true, "other sample");'''
        actual = cpp_inventory([source])
        self.assertEqual(actual['pass_formats'], {'Test: PASS cases=2 restore=2\n': 2})
        self.assertEqual(actual['assertion_sites'], 2)
        self.assertEqual(actual['machines']['test'][0]['name'], 'core')

    def test_each_profile_selects_its_banked_machine_and_exact_result(self):
        baseline = json.loads(BASELINE.read_text())
        with tempfile.TemporaryDirectory() as directory:
            for name, contract in baseline['isolated'].items():
                with self.subTest(profile=name):
                    calls = []

                    def execute(command, run, output):
                        calls.append(command)
                        output.write('\n'.join(contract['required'] + [contract['completion']]) + '\n')
                        return 3

                    argv = ['run_c54x_core', '--mame', 'mame/mame', '--rompath', 'mame/roms',
                            '--profile', name, '--output-root', directory]
                    with patch('sys.argv', argv), patch.object(runner, 'run_emulator', execute), \
                         contextlib.redirect_stdout(io.StringIO()):
                        self.assertEqual(runner.main(), 0)
                    self.assertEqual(len(calls), 1)
                    self.assertEqual(calls[0][1], contract['machine'])
                    self.assertEqual(calls[0][calls[0].index('-bios') + 1], name)


if __name__ == '__main__':
    unittest.main()
