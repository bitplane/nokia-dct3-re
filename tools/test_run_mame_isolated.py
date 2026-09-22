import pathlib
import unittest

from tools.run_mame_isolated import rebase_paths


class RunMameIsolatedTest(unittest.TestCase):
    def test_rebases_only_path_options_from_old_mame_directory(self):
        old_cwd = pathlib.Path("/project/mame")
        self.assertEqual(rebase_paths([
            "noki5210", "-rompath", "roms", "-cfg_directory", "../fixtures/radio",
            "-autoboot_script", "../input.lua", "-nvram_directory", "/tmp/nvram",
            "-seconds_to_run", "30",
        ], old_cwd), [
            "noki5210", "-rompath", "/project/mame/roms",
            "-cfg_directory", "/project/fixtures/radio",
            "-autoboot_script", "/project/input.lua",
            "-nvram_directory", "/tmp/nvram", "-seconds_to_run", "30",
        ])


if __name__ == "__main__":
    unittest.main()
