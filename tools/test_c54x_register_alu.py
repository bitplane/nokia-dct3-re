"""Independent coverage census for Execute-owned register ALU cases."""
import shutil
import subprocess
import unittest

from tools.test_c54x_case_fixture import compiled_cpp


class RegisterAluTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_retained_inputs_ties_sentinels_and_replay_classes(self):
        with compiled_cpp(r'''
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <map>
#include <tuple>
using u16=std::uint16_t; using u32=std::uint32_t; using u64=std::uint64_t; using s64=std::int64_t;
constexpr bool BIT(u64 value,unsigned bit) { return (value>>bit)&1; }
constexpr int STATE_GENPC=-1;
enum {AS_PROGRAM,AS_DATA};
struct emu_fatalerror { template<class... A> emu_fatalerror(char const *,A...) {} };
struct r { enum {STATE_A,STATE_B,STATE_ST0,STATE_ST1,STATE_T,STATE_ILLEGAL,
    STATE_IDLE,STATE_PMST,STATE_TCR,STATE_IFR,STATE_IMR}; };
#include "tests/c54x/tms320c54x_test_register_alu.h"
using namespace c54x_test;
auto state(minmax_fixture const &f) {
    return std::tie(f.selector,f.base,f.a,f.b,f.result,f.st0,f.st1,f.result_st0);
}
int main() {
    for(auto kind:{register_alu_kind::minmax,register_alu_kind::square}) {
        bool const square=kind==register_alu_kind::square;
        unsigned const span=square?8192:16384;
        std::vector<unsigned> seen(4*span);
        std::map<std::array<u32,5>,std::array<unsigned,4>> groups;
        ordered_case_digest coverage;
        for(unsigned i=0;i<register_alu_count(kind);++i) {
            unsigned const key=register_alu_key(i,kind);
            assert(key<4*span && !seen[key]++);
            auto input=[square](unsigned raw) {return minmax_case(square?raw%512+(raw/512)*1024:raw,square);};
            auto f=input(key);
            for(unsigned s=0;s<4;++s) assert(state(f)==state(input(key%span+s*span)));
            if(!square) {
                // Signed order by sign-bit bias, independent of fixture s64 conversion.
                auto a=f.a^(u64(1)<<39),b=f.b^(u64(1)<<39);
                bool const choose=f.selector&2?a<b:a>b;
                assert(f.result==(choose?f.a:f.b));
                assert(f.result_st0==((f.st0&~0x800)|(choose?0:0x800)));
                unsigned const relation=a==b?0:a>b?1:2;
                ++groups[{f.selector,f.st0,f.st1,f.base,relation}][key/span];
            } else {
                assert((key/16)%16==11 || (key/16)%16==12);
                assert(f.b!=f.result); // Both sentinels expose a stale destination or unwanted B write.
            }
            auto c=register_alu_case<r>(i,kind);
            assert(c.identity==key && c.finish==12 && c.checkpoint==(std::array<unsigned,4>{3,5,6,8}[f.snapshot]));
            assert(c.states.size()==2 && c.states[0].tick==0 && c.states[1].tick==6);
            assert(c.actions.size()==1 && c.actions[0].tick==5 && c.actions[0].values.size()==2);
            assert(c.actions[0].values[0].value==f.a && c.actions[0].values[1].value==f.b);
            assert(c.forbid_data_access && c.single_fetch==f.base+0x600);
            assert(!c.states[0].observation.reset_before);
            coverage.add(key);
        }
        for(unsigned input=0;input<span;++input) {
            unsigned copies=0;
            for(unsigned s=0;s<4;++s) copies+=seen[input+s*span];
            if(square) assert(copies==(((input/16)%16==11 || (input/16)%16==12)?4U:0U));
            else assert(copies==(input%16==(input/16)%16?4U:1U));
        }
        // Every selector/status/page/strict-order-or-equality class still has
        // all four restore boundaries, including sticky-overflow seed parity.
        for(auto const &group:groups) for(auto count:group.second) assert(count);
        std::printf("cases=%u digest=%016llx\n",coverage.count,(unsigned long long)coverage.value);
    }
    assert(minmax_case(178,true).result==0xfffe0001ULL); // +65535 squared, not -1.
    assert(minmax_case(179,true).result==0x100000000ULL); // Signed17 minimum squared.
    assert(minmax_case(180,true).result==1);             // Signed17 -1 squared.
}
''') as binary:
            lines = subprocess.check_output([str(binary)], text=True).splitlines()
        self.assertEqual(lines, ['cases=19456 digest=8e180db77f0a8b25',
                                 'cases=4096 digest=fac4cb2f8288e625'])


if __name__ == '__main__':
    unittest.main()
