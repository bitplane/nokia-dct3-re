// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    for (bool b:{false,true}) for (u16 address:{0x5000U,0xf495U,0xfffeU})
    for (unsigned snapshot:{3U,6U,9U}) {
        u64 const a=0x123456789aULL,other=0xffabcdef01ULL;
        u32 const value=u32(b ? other : a);
        auto initial=initial_case("absolute-DST-in-BD-two-word-delay",
            {0xf273,0x700,u16(b ? 0x4ff8 : 0x4ef8),address,0xf4e1});
        initial.program.push_back({0x700,0xf4e1});
        initial.initial.insert(initial.initial.end(),{{r::STATE_A,a},{r::STATE_B,other},
            {r::STATE_ST0,0xaa5},{r::STATE_ST1,0x100}});
        auto final=observation("delay-store-then-branch-target",{{STATE_GENPC,0x701},
            {r::STATE_A,a},{r::STATE_B,other},{r::STATE_ST0,0xaa5},{r::STATE_ST1,0x100}},
            {{address,u16(value>>16)},{address^1U,u16(value)}});
        clocked_register_case c{{{0,std::move(initial)},{32,std::move(final)}},snapshot,32};
        c.data_events=std::vector<clocked_data_event>{{true,address,u16(value>>16),0,false},
            {true,address^1U,u16(value),0,false}};
        cases.push_back(std::move(c));
    }
    return cases;
}
}
