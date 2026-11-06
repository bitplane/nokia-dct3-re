"""The section catalogue owns counts; source paths determine CPU/firmware scope."""
from pathlib import Path
import contextlib
import io
import json
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools.run_c54x_section import batch_compatible, batch_header, load_sections, main, plan_batches, refresh_cpu_overlay, run_section, run_suite


class SectionCatalogTests(unittest.TestCase):
    def test_focused_build_refreshes_changed_cpu_sources_without_touching_cached_ones(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'cpu/tms320c54x'
            target = root / 'mame/src/devices/cpu/tms320c54x'
            source.mkdir(parents=True)
            target.mkdir(parents=True)
            for name in ('tms320c54x.cpp', 'tms320c54x.h', 'tms320c54x_decode.h',
                         'tms320c54x_dasm.cpp', 'tms320c54x_dasm.h'):
                (source / name).write_text(name)
            (source / 'README.md').write_text('not a build input')
            refresh_cpu_overlay(root)
            self.assertEqual({p.name for p in target.iterdir()},
                             {p.name for p in source.iterdir() if p.suffix != '.md'})
            cached = (target / 'tms320c54x.h').stat().st_mtime_ns
            (source / 'tms320c54x.cpp').write_text('changed CPU implementation')
            refresh_cpu_overlay(root)
            self.assertEqual((target / 'tms320c54x.cpp').read_text(), 'changed CPU implementation')
            self.assertEqual((target / 'tms320c54x.h').stat().st_mtime_ns, cached)

    def test_batches_preserve_inventory_and_isolate_special_configuration(self):
        sections = load_sections()
        selected = list(sections)
        batches = plan_batches(selected, sections, 8)
        flattened = [name for batch in batches for name in batch]
        self.assertCountEqual(flattened, selected)
        self.assertEqual(len(flattened), len(set(flattened)))
        self.assertLess(len(batches), len(selected))
        for batch in batches:
            self.assertLessEqual(len(batch), 8)
            if len(batch) > 1:
                self.assertTrue(all(batch_compatible(sections[name]) for name in batch))
        for name in ('c5416_io_extended', 'bio_conditions', 'mcbsp_control', 'mu4_name_copy'):
            self.assertIn([name], batches)
        self.assertEqual(plan_batches(['immediate_store', 'basic_memory'], sections, 1),
                         [['immediate_store'], ['basic_memory']])

    def test_batch_eligibility_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'x.h'
            base = '#include "c54x_section_common.h"\nnamespace c54x_test { inline auto section_cases() { return 0; } }'
            for extra, cases, scope, expected in [('', 2, 'cpu', True),
                    ('\n#define C54X_SECTION_INSTRUCTION_EVENTS', 2, 'cpu', False),
                    ('\n#include "other.h"', 2, 'cpu', False),
                    ('\ninline auto helper() { return 0; }', 2, 'cpu', False),
                    ('', 65, 'cpu', False), ('', 2, 'firmware', False)]:
                source.write_text(base + extra)
                self.assertEqual(batch_compatible(dict(source=source, cases=cases, scope=scope)), expected)

    def test_batch_selection_is_explicit_and_unknown_names_are_rejected(self):
        text = batch_header(['one', 'two'])
        self.assertIn('section_cases_one()', text)
        self.assertIn('section_cases_two()', text)
        self.assertIn('Missing C54x batch section', text)
        self.assertIn('Unknown C54x batch section', text)
        self.assertEqual(text.count('#undef section_cases'), 2)

    def test_individual_execution_requires_own_counts_and_completion(self):
        good = 'C54x section: PASS cases=2 restores=2\nC54x section complete\n'
        for code, text, passes in [(3, good, True), (0, good, False),
                (3, good.replace('cases=2', 'cases=3'), False),
                (3, good.replace('restores=2', 'restores=1'), False),
                (3, good.replace('restores=2', 'restores=20'), False),
                (3, good.replace('C54x section complete', ''), False)]:
            with self.subTest(code=code, text=text), tempfile.TemporaryDirectory() as directory, \
                    patch('tools.run_c54x_section.subprocess.run',
                          return_value=subprocess.CompletedProcess([], code, text, '')) as run, \
                    contextlib.redirect_stdout(io.StringIO()):
                if passes:
                    run_section(['emulator'], Path(directory), 'one', 2, 1.5)
                else:
                    with self.assertRaises(SystemExit):
                        run_section(['emulator'], Path(directory), 'one', 2, 1.5)
                self.assertEqual(run.call_args.kwargs['env']['C54X_TEST_SECTION'], 'one')
                self.assertEqual(json.loads((Path(directory) / 'timing.json').read_text())['exit_code'], code)

    def test_suite_reports_only_successful_children(self):
        for fail in (False, True):
            with self.subTest(fail=fail), tempfile.TemporaryDirectory() as directory:
                failure = subprocess.CalledProcessError(1, ['runner'])
                results = [None, failure] if fail else [None, None]
                with patch('tools.run_c54x_section.subprocess.run', side_effect=results) as run, \
                        contextlib.redirect_stdout(io.StringIO()):
                    if fail:
                        with self.assertRaises(subprocess.CalledProcessError):
                            run_suite(['a', 'b'], {'a': {'cases': 2}, 'b': {'cases': 3}}, 4, 'cpu', Path(directory))
                    else:
                        run_suite(['a', 'b'], {'a': {'cases': 2}, 'b': {'cases': 3}}, 4, 'cpu', Path(directory))
                report = json.loads((Path(directory) / 'tmp/c54x-section-trial/cpu-summary.json').read_text())
                self.assertEqual(report['complete'], not fail)
                self.assertEqual(report['passed_cases'], 2 if fail else 5)
                self.assertEqual([item['section'] for item in report['completed']], ['a'] if fail else ['a', 'b'])
                self.assertEqual(report.get('active_section'), 'b' if fail else None)
                self.assertEqual(run.call_count, 2)
                self.assertTrue(all(call.kwargs['check'] for call in run.call_args_list))

    def test_invalid_build_modes_do_not_start_a_build(self):
        for arguments in (['all', '--build-only'],
                          ['immediate_store', '--build-only', '--negative-control'],
                          ['--batch', 'immediate_store', 'immediate_store'],
                          ['--batch', 'immediate_store', '--negative-control'],
                          ['--batch', 'c5416_io_extended'],
                          ['immediate_store', '--batch', 'paired_store']):
            with self.subTest(arguments=arguments), patch('sys.argv', ['runner', *arguments]), \
                    patch('tools.run_c54x_section.subprocess.run') as run, \
                    contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as error:
                    main()
                self.assertEqual(error.exception.code, 2)
                run.assert_not_called()

    def test_repository_catalog_covers_sources(self):
        sections = load_sections()
        self.assertTrue(any(item['scope'] == 'cpu' for item in sections.values()))
        self.assertTrue(any(item['scope'] == 'firmware' for item in sections.values()))

    def test_invalid_catalogues_fail_closed(self):
        for content, files in [('{"x": 1, "x": 2}', ['tests/c54x/sections/x.h']),
                               ('{"x": 0}', ['tests/c54x/sections/x.h']),
                               ('{"x": true}', ['tests/c54x/sections/x.h']),
                               ('{"x": 1}', []),
                               ('{"x": 1}', ['tests/c54x/sections/x.h', 'tests/firmware/c54x_x.h']),
                               ('{}', ['tests/c54x/sections/x.h'])]:
            with self.subTest(content=content, files=files), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'fixtures').mkdir()
                (root / 'fixtures/c54x_sections.json').write_text(content)
                for name in files:
                    path = root / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.touch()
                with self.assertRaises(ValueError):
                    load_sections(root)


if __name__ == '__main__':
    unittest.main()
