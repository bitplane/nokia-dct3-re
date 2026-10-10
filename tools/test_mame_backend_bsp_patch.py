"""Keep native test-machine registration patches independently reversible."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BackendPatchTest(unittest.TestCase):
    @unittest.skipUnless(shutil.which('git'), 'git required')
    def test_registration_does_not_break_existing_overlay_check(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / 'src/mame/mame.lst'
            target.parent.mkdir(parents=True)
            target.write_text(
                '@source:nokia/nokia_dct3.cpp\nnoki8890\n\n'
                '@source:northstar/horizon.cpp\nnshrz\nnshrz2mhz\n\n'
                '@source:zaccaria/zaccaria.cpp\nmonymony2\nmonymony3\n\n'
                '@source:zpa/iq151.cpp\niq151\n')
            patches = [ROOT / 'patches' / name for name in (
                'mame-tms320c54x-test.patch', 'mame-c54x-backend-bsp-test.patch')]
            environment = dict(os.environ, GIT_CEILING_DIRECTORIES=str(root.parent))
            for patch in patches:
                subprocess.run(['git', 'apply', str(patch)], cwd=root,
                               env=environment, check=True, capture_output=True, text=True)
            for patch in patches:
                subprocess.run(['git', 'apply', '--reverse', '--check', str(patch)],
                               cwd=root, env=environment, check=True, capture_output=True, text=True)
            self.assertEqual(target.read_text().splitlines().count('tms54bsp'), 1)


if __name__ == '__main__':
    unittest.main()
