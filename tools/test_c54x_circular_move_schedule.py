"""Replay sampling audit against the actual, unchanged reference factories."""
from collections import Counter
from pathlib import Path
import re
import shutil
import subprocess
import unittest

from tools.test_c54x_case_fixture import compiled_cpp


ROOT = Path(__file__).resolve().parents[1]


class CircularMoveScheduleTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_inputs_unchanged_checkpoint_copies_removed(self):
        header = (ROOT / 'tests/c54x/tms320c54x_test_state.h').read_text()
        source = (ROOT / 'tests/c54x/tms320c54x_test_addressing.cpp').read_text()

        def extract(text, pattern):
            matches = re.findall(pattern, text, re.MULTILINE | re.DOTALL)
            self.assertEqual(len(matches), 1, pattern)
            return matches[0]

        structs = ''.join(extract(header, r'\tstruct ' + name + r'\n\t\{.*?\n\t\};\n')
                          for name in ('indexed_store_fixture', 'circular_move_fixture'))
        methods = ''.join(extract(source, r'^[^\n]*tms320c54x_test_state::' + name + r'\([^\n]*\n\{.*?^\}\n')
                          for name in ('circular_load_case', 'circular_move_case', 'indexed_move_case'))
        cpp = r'''
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <tuple>
using u8=std::uint8_t; using u16=std::uint16_t; using u32=std::uint32_t;
using u64=std::uint64_t; using s16=std::int16_t; using s32=std::int32_t; using s64=std::int64_t;
#include "tests/c54x/tms320c54x_test_circular_move_schedule.h"
namespace c54x_test {
struct tms320c54x_test_state {
STRUCTS
static indexed_store_fixture circular_load_case(unsigned);
static circular_move_fixture circular_move_case(unsigned);
static circular_move_fixture indexed_move_case(unsigned);
};
METHODS
auto signature(tms320c54x_test_state::circular_move_fixture const &c) {
    auto const &f=c.memory;
    return std::tie(f.ar,f.selected,f.selector,f.add,f.compatibility,f.excluded,f.base,f.value,
        f.word,f.st0,f.st1,f.pmst,f.address,f.final_ar,f.bk,f.mode,f.initial,
        c.destination,c.word,c.sp,c.bk,c.brc,c.t,c.trn,c.a,c.b,c.final,c.excluded,c.write_clock);
}
}
int main() {
    using namespace c54x_test;
    struct cohort { unsigned base, inputs, copies; };
    constexpr cohort blocks[]={{0,43008,3},{129024,1344,3},{133056,48384,2},{229824,21504,3}};
    for (bool indexed : {false,true}) {
        unsigned excluded=0;
        for (unsigned i=0;i<circular_move_schedule::cases;++i) {
            unsigned const key=circular_move_schedule::key(i);
            unsigned group=0;
            while (group<4 && key>=blocks[group].base+blocks[group].inputs*blocks[group].copies) ++group;
            assert(group<4);
            auto const &b=blocks[group];
            unsigned const input=(key-b.base)%b.inputs;
            auto factory=[indexed](unsigned k) { return indexed?tms320c54x_test_state::indexed_move_case(k):tms320c54x_test_state::circular_move_case(k); };
            auto const selected=factory(key);
            assert(selected.memory.snapshot==(key-b.base)/b.inputs);
            for (unsigned snapshot=0;snapshot<b.copies;++snapshot)
                assert(signature(selected)==signature(factory(b.base+input+b.inputs*snapshot)));
            excluded+=selected.excluded;
            if (!indexed) std::printf("%u\n",key);
        }
        std::printf("excluded=%u\n",excluded);
    }
}
'''.replace('STRUCTS', structs).replace('METHODS', methods)
        with compiled_cpp(cpp) as binary:
            lines = subprocess.check_output([str(binary)], text=True).splitlines()
        self.assertEqual(lines[-2:], ['excluded=2688', 'excluded=2688'])
        keys = list(map(int, lines[:-2]))
        self.assertEqual(len(keys), 114240)
        blocks = ((0, 43008, 3), (129024, 1344, 3), (133056, 48384, 2), (229824, 21504, 3))
        actual, balance, digest = [], Counter(), 14695981039346656037
        for key in keys:
            group = next(i for i, (base, span, copies) in enumerate(blocks) if base <= key < base + span * copies)
            base, span, _ = blocks[group]
            snapshot, value = divmod(key - base, span)
            actual.append((group, value))
            kind = (value // 1024, value % 21, value // 1152, value // 64)[group] % 21
            balance[group, kind, snapshot] += 1
            for byte in key.to_bytes(4, 'little'):
                digest = ((digest ^ byte) * 1099511628211) & ((1 << 64) - 1)
        self.assertEqual(actual, [(g, v) for g, (_, span, _) in enumerate(blocks) for v in range(span)])
        for group, (_, _, copies) in enumerate(blocks):
            for kind in range(21):
                counts = [balance[group, kind, s] for s in range(copies)]
                self.assertGreater(min(counts), 0)
                self.assertLessEqual(max(counts) - min(counts), 1)
        self.assertEqual(f'{digest:016x}', '620093b2bc366484')


if __name__ == '__main__':
    unittest.main()
