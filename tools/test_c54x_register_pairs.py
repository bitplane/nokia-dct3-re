"""Host checks of clocked case data; native profiles execute the CPU/replays."""
import shutil
import subprocess
import unittest

from tools.test_c54x_case_fixture import compiled_cpp


class RegisterPairTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_case_layouts_and_literal_publication_examples(self):
        with compiled_cpp(r'''
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <cstdlib>
using u16=std::uint16_t; using u32=std::uint32_t; using u64=std::uint64_t;
constexpr int STATE_GENPC=-1;
enum {AS_PROGRAM,AS_DATA};
struct emu_fatalerror { template<class... A> emu_fatalerror(char const *,A...) {} };
struct r { enum {STATE_A,STATE_B,STATE_ST0,STATE_ST1,STATE_ILLEGAL,STATE_IDLE,
    STATE_PMST,STATE_TCR,STATE_IFR,STATE_IMR}; };
#include "tests/c54x/tms320c54x_test_register_pairs.h"
using namespace c54x_test;
u64 get(instruction_case const &s,unsigned index) {
    for(auto const &check:s.expected) if(check.index==int(index)) return check.value;
    std::abort();
}
void literal(register_pair_kind kind,unsigned raw,u64 input,u64 first,u64 second,u16 status1,u16 status2) {
    auto c=register_pair_case<r>(raw,kind);
    assert(get(c.states[0].observation,r::STATE_A)==input);
    assert(get(c.states[1].observation,r::STATE_A)==first);
    assert(get(c.states[2].observation,r::STATE_A)==second);
    assert(get(c.states[1].observation,r::STATE_ST0)==status1);
    assert(get(c.states[2].observation,r::STATE_ST0)==status2);
}
int main() {
    for(auto kind:{register_pair_kind::saturation,register_pair_kind::rotate}) {
        ordered_case_digest coverage;
        bool const sat=kind==register_pair_kind::saturation;
        for(unsigned raw=0;raw<register_pair_count(kind);++raw) {
            auto c=register_pair_case<r>(raw,kind);
            assert(c.states.size()==3 && c.finish==16 && c.checkpoint==5+raw/(sat?1024:1536));
            assert(c.states[0].tick==0 && c.states[1].tick==6 && c.states[2].tick==7);
            auto const &initial=c.states[0].observation;
            assert(!initial.reset_before && initial.program.size()==12 && initial.data.empty());
            for(unsigned i=0;i<12;++i) {
                assert(initial.program[i].address==0x9000+i+((raw/(sat?512:768))%2?0x20000:0));
                assert(initial.program[i].value==(i<2?initial.opcode:i==2?0xf4e1:0xf495));
            }
            for(auto const &s:c.states) {
                assert(s.observation.expected.size()==5 && get(s.observation,r::STATE_ILLEGAL)==0);
                for(auto const &check:s.observation.expected) assert(check.mask==~u64(0));
            }
            coverage.add(raw);
        }
        std::printf("cases=%u digest=%016llx\n",coverage.count,(unsigned long long)coverage.value);
    }
    using k=register_pair_kind;
    literal(k::saturation,8,0x80000000,0x7fffffff,0x7fffffff,0x1c24,0x1824);
    literal(k::saturation,12,0xff7fffffff,0xff80000000,0xff80000000,0x1c24,0x1824);
    literal(k::saturation,14,0xff80000000,0xff80000000,0xff80000000,0x1824,0x1824);
    literal(k::rotate,54,1,0x80000000,0xc0000000,0x0824,0x0024);
    literal(k::rotate,112,0x80000000,1,3,0x1824,0x1024);
    literal(k::rotate,24,0x5fb0001234,0x5800091a,0x2c00048d,0x0024,0x0024);
}
''') as binary:
            lines = subprocess.check_output([str(binary)], text=True).splitlines()
        self.assertEqual(lines, ['cases=3072 digest=7351b9c205db7825',
                                 'cases=4608 digest=897bd2c47c8b0625'])


if __name__ == '__main__':
    unittest.main()
