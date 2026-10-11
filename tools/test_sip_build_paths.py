import os
import subprocess
import tempfile
import unittest
from pathlib import Path


class SipBuildPathsTest(unittest.TestCase):
    def test_project_root_resolves_and_exports_dependencies(self):
        root = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory) / 'pjproject'
            binary = project / 'pjsip-apps/bin/pjsua-test'
            binding = project / 'pjsip-apps/src/swig/python'
            extension = binding / 'build/lib.test'
            binary.parent.mkdir(parents=True)
            binary.touch()
            extension.mkdir(parents=True)
            recipe = 'sip-path-test:\n\t@printf "%s\\n" "$$SIP_PROJECT_DIR" "$$SIP_PJSUA_BIN" "$$SIP_PYTHON_PATH"\n'
            environment = {key: value for key, value in os.environ.items()
                           if key not in ('SIP_PROJECT_DIR', 'SIP_PJSUA_BIN',
                                          'SIP_PYTHON_PATH', 'MAKEFLAGS', 'MFLAGS')}
            for overrides, expected_binary in (([], binary),
                    (['SIP_PJSUA_BIN=/explicit/pjsua'], Path('/explicit/pjsua'))):
                result = subprocess.run(['make', '--no-print-directory', '-f', 'Makefile',
                    '-f', '-', 'sip-path-test', f'SIP_PROJECT_DIR={project}', *overrides],
                    cwd=root, input=recipe, text=True, capture_output=True, check=True,
                    env=environment)
                self.assertEqual(result.stdout.splitlines(), [str(project),
                    str(expected_binary), f'{binding}:{extension}'])


if __name__ == '__main__':
    unittest.main()
