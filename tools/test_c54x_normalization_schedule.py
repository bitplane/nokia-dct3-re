"""Check replay selection and unchanged inputs, not CPU arithmetic helpers."""
import shutil
import subprocess
import unittest

from tools.test_c54x_case_fixture import compiled_cpp


class NormalizationScheduleTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_distinct_inputs_and_balanced_context_checkpoints(self):
        with compiled_cpp(r'''
#include <cstdint>
#include <tuple>
#include <map>
#include <vector>
#include <cassert>
#include <cstdio>
using u16=std::uint16_t; using u32=std::uint32_t; using u64=std::uint64_t;
using s16=std::int16_t;
constexpr bool BIT(u64 value,unsigned bit) { return (value>>bit)&1; }
#include "tests/c54x/tms320c54x_test_normalization_input.h"
#include "tests/c54x/tms320c54x_test_normalization_schedule.h"
#include "tests/c54x/tms320c54x_test_support.h"
auto state(c54x_test::normalization_fixture const &f) {
    return std::tie(f.selector,f.prefix,f.gap,f.read_cycle,f.execute_cycle,
        f.t,f.st0,f.st1,f.result_st0,f.base,f.before,f.after);
}
auto state(c54x_test::exponent_fixture const &f) {
    return std::tie(f.value,f.b,f.producer,f.gap,f.st0,f.st1,f.base);
}
int main() {
    using namespace c54x_test;
    using k=normalization_kind;
    for (auto kind : {k::documented,k::aliases,k::producer,k::exponent}) {
        unsigned const span=normalization_schedule::count(kind), snaps=kind==k::aliases?2:4;
        bool const exp=kind==k::exponent, producer=kind==k::producer, alias=kind==k::aliases;
        std::vector<unsigned> seen(span);
        std::map<std::array<u32,6>, std::array<unsigned,4>> groups;
        ordered_case_digest coverage;
        for (unsigned i=0;i<span;++i) {
            unsigned const key=normalization_schedule::key(kind,i);
            assert(key<span*snaps && key%span==i && !seen[key%span]++);
            coverage.add(key);
            auto n=normalization_case(key,producer,alias);
            auto e=exponent_case(key);
            for (unsigned snap=0;snap<snaps;++snap) {
                if (exp) assert(state(e)==state(exponent_case(i+span*snap)));
                else assert(state(n)==state(normalization_case(i+span*snap,producer,alias)));
            }
            std::array<u32,6> group = exp ? std::array<u32,6>{e.b,e.producer,e.gap,e.st0,e.st1,e.base} :
                std::array<u32,6>{n.selector,producer?n.gap:u32(n.t&63),n.st0,n.st1,n.base,0};
            ++groups[group][key/span];
        }
        for (auto const &entry : groups)
            for (unsigned snap=0;snap<4;++snap)
                assert(entry.second[snap]==(snap<snaps ? exp||alias?4U:2U : 0U));
        std::printf("cases=%u digest=%016llx\n",coverage.count,(unsigned long long)coverage.value);
    }
}
''') as binary:
            lines = subprocess.check_output([str(binary)], text=True).splitlines()
        self.assertEqual(lines, ['cases=24576 digest=6d96e57cd246b165',
                                 'cases=8192 digest=870eba4cadda7525',
                                 'cases=1536 digest=9738168624bb55e5',
                                 'cases=3072 digest=0f4f33ce639d0dc5'])


if __name__ == '__main__':
    unittest.main()
