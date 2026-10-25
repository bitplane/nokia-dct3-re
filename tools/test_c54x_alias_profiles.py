"""Execute the fixture's profile decoder; preserve every encoded case, not strings."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


BASES = {
    'mpy-abs-edge': [0x6200], 'mac-abs-edge': [0x6400, 0x6600],
    'delay-abs-edge': [0x4c00], 'pmac-abs-edge': [0x7800, 0x7a00],
    'mem-t-abs-edge': list(range(0x2000, 0x3000, 0x200)),
    'load-abs-edge': [0x1000, 0x1200, 0x1400, 0x1600, 0x4400, 0x5600],
    'rmw-abs-edge': [0x6800, 0x6900, 0x6a00, 0x6b00],
    'logic-abs-edge': [0x1800, 0x1a00, 0x1c00],
    'arith-abs-edge': [0x0000, 0x0200, 0x0400, 0x0600, 0x0800, 0x0a00,
                       0x0c00, 0x1e00, 0x3c00, 0x3e00, 0x4000, 0x4200],
    'long-abs-edge': [0x5000, 0x5200, 0x5400, 0x5800, 0x5a00, 0x5c00, 0x5e00],
}


class AliasProfileTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'host C++ compiler unavailable')
    def test_all_original_encoded_cases_and_status_inputs(self):
        root = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as name:
            binary = Path(name) / 'profiles'
            source = r'''
#include "driver/tms320c54x_test_profiles.h"
#include <iostream>
int main() {
    if (c54x_test::find_source_alias_profile("unknown")) return 1;
    for (auto const &p : c54x_test::source_alias_profiles) {
        if (c54x_test::find_source_alias_profile(p.bios) != &p) return 2;
        for (unsigned i = 0; i < p.cases(); ++i)
            std::cout << p.bios << ' ' << i << ' ' << p.opcode(i) << ' ' << p.st0(i) << '\n';
    }
}
'''
            subprocess.run(['c++', '-std=c++17', '-x', 'c++', '-', '-I', str(root),
                            '-o', str(binary)], input=source, text=True, check=True,
                           capture_output=True)
            rows = subprocess.check_output([str(binary)], text=True).splitlines()
        expected = []
        for name, bases in BASES.items():
            for variant in range(len(bases) * 224):
                flags = variant // 7
                rmw = name == 'rmw-abs-edge'
                opcode = bases[variant // 224] | (0x100 if flags & 1 and not rmw else 0) | (0xf9 + variant % 7)
                st0 = 0x0804 if rmw and flags & 1 else 0x1804
                expected.append(f'{name} {variant} {opcode} {st0}')
        self.assertEqual(rows, expected)
        self.assertEqual(len(rows), 10304)


if __name__ == '__main__':
    unittest.main()
