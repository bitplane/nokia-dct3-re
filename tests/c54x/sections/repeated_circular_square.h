// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    constexpr std::array<s16,4> values{-2,3,-4,5};
    for (unsigned form=0;form<4;++form) for (unsigned mode=0;mode<4;++mode)
    for (unsigned fractional=0;fractional<2;++fractional) {
        int const step=mode==0 ? -1 : mode==1 ? -2 : mode==2 ? 1 : 2;
        u16 const opcode=0x38c2|(form<<8)|(mode<<3);
        auto initial=initial_case("repeated-circular-square-feedback",{0xec06,opcode,0xf4e1});
        initial.initial.insert(initial.initial.end(),{{r::STATE_A,1000},{r::STATE_B,2000},
            {r::STATE_ST0,0x20},{r::STATE_ST1,0x100U|(fractional ? 0x40U : 0)},
            {r::STATE_AR0,2},{r::STATE_AR2,0x1203},{r::STATE_BK,4}});
        for (unsigned n=0;n<4;++n)initial.data.push_back({0x1200+n,u16(values[n])});
        clocked_register_case c{{{0,std::move(initial)}},fractional ? 9U : 5U,32};
        c.data_events=std::vector<clocked_data_event>{};
        std::array<u64,2> accumulators{1000,2000};
        unsigned cursor=3,last=3;
        for (unsigned n=0;n<7;++n) {
            last=cursor;
            c.data_events->push_back({false,0x1200+cursor,u16(values[cursor]),6+n});
            int const product=values[cursor]*values[cursor]*(fractional ? 2 : 1);
            accumulators[form&1]=u64(s64(accumulators[form&1])+(form&2 ? -product : product));
            cursor=(int(cursor)+step+4)%4;
        }
        c.states.push_back({32,observation("signed-square-counted-circular-reads",
            {{r::STATE_A,accumulators[0]},{r::STATE_B,accumulators[1]},
             {r::STATE_AR2,0x1200+cursor},{r::STATE_RC,0},{r::STATE_T,u16(values[last])},
             {r::STATE_ST0,0x20}})});
        cases.push_back(std::move(c));
    }
    auto initial=initial_case("repeated-circular-square-rejects-mapped-source",{0xec06,0x3ad2,0xf4e1});
    initial.initial.insert(initial.initial.end(),{{r::STATE_AR2,0x10},{r::STATE_BK,4},{r::STATE_A,1000}});
    initial.expected.clear();
    auto final=observation("guard-before-pointer-or-bus-effect",{{r::STATE_ILLEGAL,1},{r::STATE_AR2,0x10},{r::STATE_A,1000}});
    final.expected.pop_back();
    clocked_register_case rejected{{{0,std::move(initial)},{20,std::move(final)}},3,20};
    rejected.require_idle=false;rejected.forbid_data_access=true;
    cases.push_back(std::move(rejected));
    return cases;
}
}
