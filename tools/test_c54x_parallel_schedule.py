"""Audit retained parallel input identities and numeric checkpoint equivalence."""
from collections import Counter
from pathlib import Path
import re
import shutil
import subprocess
import unittest

from tools.test_c54x_case_fixture import compiled_cpp


ROOT = Path(__file__).resolve().parents[1]


class ParallelScheduleTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_real_factories_preserve_inputs_and_checkpoint_classes(self):
        address = (ROOT / 'tests/c54x/tms320c54x_test_addressing.cpp').read_text()
        arithmetic = (ROOT / 'tests/c54x/tms320c54x_test_arithmetic.cpp').read_text()
        header = (ROOT / 'tests/c54x/tms320c54x_test_state.h').read_text()

        def extract(text, pattern):
            matches = re.findall(pattern, text, re.M | re.S)
            self.assertEqual(len(matches), 1, pattern)
            return matches[0]

        shape = extract(header, r'\tstruct parallel_load_fixture\n\t\{.*?\n\t\};')
        methods = ''.join(extract(address + arithmetic,
            r'^[^\n]*tms320c54x_test_state::' + name + r'\([^\n]*\n\{.*?^\}\n')
            for name in ('finish_parallel_load_case', 'parallel_load_case',
                         'parallel_arithmetic_case', 'parallel_multiply_case'))
        consumers = extract(address, r'^void tms320c54x_test_state::start_parallel_load\([^\n]*\n\{.*?^\}\n')
        consumers += extract(address, r'^\tif \(m_phase == 62164\)\n\t\{.*?^\t\}\n')
        # These fields are reference-generator metadata, not consumed input.
        # A future consumer must not silently invalidate the projection basis.
        self.assertNotRegex(consumers, r'\.(?:flags|operation|yword|result)\b')
        self.assertEqual(consumers.count('f.result_st0'), 1)
        self.assertRegex(consumers, r'\(\(arithmetic\s*\|\|\s*multiply\)\s*\?\s*f\.result_st0\s*:\s*f\.st0\)')
        cpp = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <tuple>
using u8=std::uint8_t; using u16=std::uint16_t; using u32=std::uint32_t; using u64=std::uint64_t;
using s16=std::int16_t; using s32=std::int32_t; using s64=std::int64_t;
template<class T> bool BIT(T n, unsigned b) { return (n >> b) & 1; }
#include "tests/c54x/tms320c54x_test_dual_input.h"
#include "tests/c54x/tms320c54x_test_parallel_schedule.h"
namespace c54x_test {
struct tms320c54x_test_state {
SHAPE
static void finish_parallel_load_case(parallel_load_fixture &);
static parallel_load_fixture parallel_load_case(unsigned);
static parallel_load_fixture parallel_arithmetic_case(unsigned);
static parallel_load_fixture parallel_multiply_case(unsigned);
};
METHODS
auto live(tms320c54x_test_state::parallel_load_fixture const &c, unsigned kind) {
    auto const &f=c.memory;
    return std::make_tuple(f.x,f.y,f.destination,f.source,f.linear,f.base,f.initial,f.final,
        f.xaddress,f.yaddress,f.xword,f.st0,f.st1,f.pmst,f.stored,kind?f.result_st0:0,
        c.form,c.a,c.b,c.result_a,c.result_b,c.imr,c.ifr,c.t);
}
}
int main() {
    using namespace c54x_test;
    using s=tms320c54x_test_state;
    for(unsigned kind=0;kind<3;++kind) {
        unsigned const forms=kind==0?6:kind==1?8:4;
        u64 digest=14695981039346656037ULL;
        auto build=[kind](unsigned k) { return kind==0?s::parallel_load_case(k):kind==1?s::parallel_arithmetic_case(k):s::parallel_multiply_case(k); };
        for(unsigned ordinal=0;ordinal<forms*parallel_schedule::inputs_per_form;++ordinal) {
            unsigned const key=parallel_schedule::key(ordinal), form=ordinal/34816, value=ordinal%34816;
            assert(key/67584==form);
            if(value<18432) assert(key==form*67584+value);
            else {
                unsigned const n=value-18432, selected=(key-form*67584-18432)/16384;
                assert(selected<3 && (key-form*67584-18432)%16384==n);
                auto const actual=build(key);
                assert(actual.memory.snapshot==selected);
                for(unsigned snapshot=0;snapshot<3;++snapshot)
                    assert(live(actual,kind)==live(build(form*67584+18432+n+16384*snapshot),kind));
                if(kind==1) std::printf("N %u %u %u %u %u\n",form,(n/64)%8,(n/512)%16,n/8192,selected);
            }
            for(unsigned b=0;b<4;++b) digest=(digest^u8(key>>(b*8)))*1099511628211ULL;
        }
        std::printf("H %u %u %016llx\n",kind,forms*34816,(unsigned long long)digest);
    }
}
'''.replace('SHAPE', shape).replace('METHODS', methods)
        with compiled_cpp(cpp) as binary:
            lines = subprocess.check_output([str(binary)], text=True).splitlines()
        self.assertEqual([line for line in lines if line.startswith('H ')], [
            'H 0 208896 41e5a5c35767ca59', 'H 1 278528 3de2e358bb5ee4d3',
            'H 2 139264 55a81cebe64489d8'])
        counts = Counter(tuple(map(int, line.split()[1:])) for line in lines if line.startswith('N '))
        self.assertEqual(sum(counts.values()), 8 * 16384)
        for form in range(8):
            for shift in range(8):
                for flags in range(16):
                    for page in range(2):
                        bins = [counts[form, shift, flags, page, snapshot] for snapshot in range(3)]
                        self.assertGreater(min(bins), 0)
                        self.assertLessEqual(max(bins) - min(bins), 1)


if __name__ == '__main__':
    unittest.main()
