"""Independent old-matrix identity census for the reduced bus-load cohort."""
import shutil
import subprocess
import unittest

from tools.test_c54x_case_fixture import compiled_cpp


class PeripheralScheduleTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_boundaries_cross_arithmetic_interiors_cross_bus_contracts(self):
        with compiled_cpp(r'''
#include <cstdio>
#include "tests/c54x/tms320c54x_test_peripheral_schedule.h"
int main() {
    for (unsigned i=0; i<c54x_test::shifted_bus_schedule::cases; ++i)
        std::printf("%u\n", c54x_test::shifted_bus_schedule::key(i));
}
''') as binary:
            actual = list(map(int, subprocess.check_output([str(binary)], text=True).splitlines()))
        boundary, interior = [], []
        # Decode the original 245760 identities without using the new selector.
        for key in range(245760):
            v, address = divmod(key, 64)
            v, shift = divmod(v, 4)
            v, form = divmod(v, 5)
            v, flags = divmod(v, 4)
            v, accumulator = divmod(v, 2)
            v, pattern = divmod(v, 4)
            snapshot, page = divmod(v, 2)
            if address in (0, 63):
                boundary.append(key)
            elif (flags, accumulator, pattern, page) == (0, 0, 2, 0):
                interior.append(key)
        self.assertEqual(actual, boundary + interior)
        self.assertEqual((len(boundary), len(interior), len(set(actual))), (7680, 3720, 11400))
        digest = 14695981039346656037
        for key in actual:
            for byte in key.to_bytes(4, 'little'):
                digest = ((digest ^ byte) * 1099511628211) & ((1 << 64) - 1)
        self.assertEqual(f'{digest:016x}', '2496c383dd2c8dc5')


if __name__ == '__main__':
    unittest.main()
