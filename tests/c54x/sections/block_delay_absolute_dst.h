// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    for (bool b:{false,true}) for (u16 address:{0x5000U,0xfffeU})
    for (unsigned count:{0U,3U}) for (unsigned snapshot:{4U,9U}) {
        u64 const a=0x123456789aULL,other=0xffabcdef01ULL;
        u32 const value=u32(b ? other : a);
        auto initial=initial_case("absolute-DST-in-RPTBD-two-word-delay",
            {0xf272,0x604,u16(b ? 0x4ff8 : 0x4ef8),address,0x6d94,0xf4e1});
        initial.initial.insert(initial.initial.end(),{{r::STATE_A,a},{r::STATE_B,other},
            {r::STATE_AR4,0x1200},{r::STATE_BRC,count},{r::STATE_ST0,0xaa5},{r::STATE_ST1,0x100}});
        auto final=observation("delay-store-once-before-counted-body",{{STATE_GENPC,0x606},
            {r::STATE_A,a},{r::STATE_B,other},{r::STATE_AR4,0x1201+count},
            {r::STATE_BRC,0},{r::STATE_RSA,0x604},{r::STATE_REA,0x604},
            {r::STATE_ST0,0xaa5},{r::STATE_ST1,0x100}},
            {{address,u16(value>>16)},{address^1U,u16(value)}});
        clocked_register_case c{{{0,std::move(initial)},{48,std::move(final)}},snapshot,48};
        c.data_events=std::vector<clocked_data_event>{{true,address,u16(value>>16),0,false},
            {true,address^1U,u16(value),0,false}};
        cases.push_back(std::move(c));
    }
    return cases;
}
}
