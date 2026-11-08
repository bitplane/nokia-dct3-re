// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    for (bool b:{false,true}) for (unsigned mode=0;mode<3;++mode)
    for (unsigned reg:{0U,7U}) for (unsigned compatibility=0;compatibility<2;++compatibility)
    for (unsigned count:{0U,1U}) {
        u64 const a=0x123456789aULL,other=0xffabcdef01ULL;
        u32 const value=u32(b ? other : a);
        unsigned const selected=compatibility && !reg ? 3 : reg;
        u16 const status=0x608|(compatibility ? selected<<13 : 0);
        u16 const counter=u16(count+(mode==1 ? -1 : mode==2 ? 1 : 0));
        auto initial=initial_case("absolute-DST-in-BANZD-two-word-delay",
            {u16(0x6e80|(mode<<3)|reg),0x700,u16(b ? 0x4ff8 : 0x4ef8),0x5000,0xf4e1});
        initial.program.push_back({0x700,0xf4e1});
        initial.initial.insert(initial.initial.end(),{{r::STATE_A,a},{r::STATE_B,other},
            {r::STATE_ST0,compatibility ? 0x6608U : 0x608U},
            {r::STATE_ST1,0x100U|(compatibility ? 0x20U : 0)},
            {r::STATE_AR0+selected,count}});
        auto final=observation("delay-store-on-both-count-branch-outcomes",
            {{STATE_GENPC,count ? 0x701U : 0x605U},{r::STATE_A,a},{r::STATE_B,other},
             {int(r::STATE_AR0+selected),counter},{r::STATE_ST0,status}},
            {{0x5000,u16(value>>16)},{0x5001,u16(value)}});
        clocked_register_case c{{{0,std::move(initial)},{32,std::move(final)}},count ? 6U : 9U,32};
        c.data_events=std::vector<clocked_data_event>{{true,0x5000,u16(value>>16),0,false},
            {true,0x5001,u16(value),0,false}};
        cases.push_back(std::move(c));
    }
    return cases;
}
}
