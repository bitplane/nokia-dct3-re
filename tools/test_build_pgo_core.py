import unittest

from tools.build_pgo_core import core_command, with_flags

PCH = '-include ../../../../linux_gcc/obj/x64/Release/emu.h'
DRY = f"""echo Compiling src/devices/cpu/tms320c54x/tms320c54x_dasm.cpp...
g++ -O3 {PCH} -c "dasm.cpp"
echo Compiling src/devices/cpu/tms320c54x/tms320c54x.cpp...
g++ -MMD -O3 -fno-strict-aliasing {PCH} -o "core.o" -c "src/devices/cpu/tms320c54x/tms320c54x.cpp"
"""


class CoreCommandTest(unittest.TestCase):
    def test_selects_core_without_precompiled_header(self):
        command = core_command(DRY)
        self.assertIn('-c "src/devices/cpu/tms320c54x/tms320c54x.cpp"', command)
        self.assertNotIn(PCH, command)
        self.assertIn('-include ../../../../../src/emu/emu.h', command)

    def test_adds_profile_flags_after_optimisation(self):
        self.assertTrue(with_flags(core_command(DRY), '-fprofile-use').startswith(
            'g++ -MMD -O3 -fprofile-use -fno-strict-aliasing'))

    def test_rejects_missing_core(self):
        with self.assertRaises(SystemExit):
            core_command('echo Compiling other.cpp...\ng++ -O3 other.cpp\n')

    def test_rejects_unexpected_flags(self):
        with self.assertRaises(SystemExit):
            core_command(DRY.replace('-O3 -fno', '-O2 -fno'))


if __name__ == '__main__':
    unittest.main()
