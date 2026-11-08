// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    for (u16 address:{0x60,0x70,0x7f}) for (bool b:{false,true})
    for (unsigned count:{0U,1U,7U}) for (u32 page:{0U,0x20000U}) {
        auto initial=initial_case("block-STLM-ordinary-low-RAM",
            {0xf072,0x605,u16(0x8800|(b ? 0x100 : 0)|address),u16(b ? 0xf300 : 0xf000),1,0xf495,0xf4e1});
        for (auto &word:initial.program) word.address+=page;
        initial.initial.insert(initial.initial.end(),{{STATE_GENPC,page+0x600},
            {r::STATE_BRC,count},{r::STATE_A,b ? 0x5678 : 0x1234},
            {r::STATE_B,b ? 0x1234 : 0x5678},{r::STATE_ST0,0x1ff},{r::STATE_ST1,0x4800}});
        initial.data={{address,0xdead}};
        auto final=observation("counted-RAM-writes-and-continuation",
            {{r::STATE_IDLE,1},{r::STATE_BRC,0},{r::STATE_RSA,0x602},{r::STATE_REA,0x605},
             {r::STATE_A,b ? 0x5678U : 0x1235U+count},{r::STATE_B,b ? 0x1235U+count : 0x5678U},
             {r::STATE_ST0,0x1ff},{r::STATE_ST1,0x4800}},{{address,u16(0x1234+count)}});
        clocked_register_case c{{{0,std::move(initial)},{120,std::move(final)}},8,120};
        c.data_events=std::vector<clocked_data_event>{};
        for (unsigned i=0;i<=count;++i) c.data_events->push_back({true,address,u16(0x1234+i),0,false});
        cases.push_back(std::move(c));
    }
    for (u16 mode:{0x1a,0x1d,0x21,0x83}) for (bool b:{false,true}) {
        auto initial=initial_case("block-STLM-control-and-indirect-limits",
            {0xf072,0x603,u16(0x8800|(b ? 0x100 : 0)|mode),0xf495,0xf4e1});
        initial.expected.clear();
        auto final=observation("reject-before-data-effects",{{r::STATE_ILLEGAL,1}});
        final.expected.pop_back();
        clocked_register_case c{{{0,std::move(initial)},{30,std::move(final)}},3,30};
        c.require_idle=false;
        c.forbid_data_access=true;
        cases.push_back(std::move(c));
    }
    return cases;
}
}
