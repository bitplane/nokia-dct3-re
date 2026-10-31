"""The section catalogue owns counts; source paths determine CPU/firmware scope."""
from pathlib import Path
import contextlib
import io
import json
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools.run_c54x_section import load_sections, main, run_suite


class SectionCatalogTests(unittest.TestCase):
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
                          ['immediate_store', '--build-only', '--negative-control']):
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
