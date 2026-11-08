// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    for (unsigned page:{0U,0x20000U}) for (unsigned count:{0U,1U,7U,31U}) {
        auto initial=initial_case("MVDM-BRC-before-RPTB",{0x721a,0x1400,0xf072,0x605,0x1096,0x8097,0xf4e1});
        for (auto &word:initial.program) word.address+=page;
        initial.initial.insert(initial.initial.end(),{{STATE_GENPC,page+0x600},{r::STATE_ST1,0x800},
            {r::STATE_BRC,count^1},{r::STATE_AR6,0x5000},{r::STATE_AR7,0x5100}});
        initial.data.push_back({0x1400,u16(count)});
        for (unsigned n=0;n<=count;++n) initial.data.push_back({0x5000+n,u16(0x6000+n)});
        unsigned const finish=80+count*4;
        auto final=observation("captured-count-drives-following-block",{{r::STATE_BRC,0},
            {r::STATE_AR6,0x5001+count},{r::STATE_AR7,0x5101+count},{r::STATE_A,0x6000+count}});
        for (unsigned n=0;n<=count;++n) final.expected_data.push_back({0x5100+n,u16(0x6000+n)});
        clocked_register_case c{{{0,std::move(initial)},{finish,std::move(final)}},5,finish};
        c.actions={{6,{},false,{},{},{{0x1400,0xdead}}}};
        c.data_events=std::vector<clocked_data_event>{{false,0x1400,u16(count),6},{true,0x1a,u16(count),6}};
        for (unsigned n=0;n<=count;++n) {
            c.data_events->push_back({false,0x5000+n,u16(0x6000+n),0,false});
            if (n)c.data_events->push_back({true,0x5100+n-1,u16(0x6000+n-1),0,false});
        }
        c.data_events->push_back({true,0x5100+count,u16(0x6000+count),0,false});
        cases.push_back(std::move(c));
    }
    for (unsigned kind=0;kind<3;++kind) {
        auto initial=initial_case("MVDM-BRC-rejects-unmodelled-composition",{0x721a,u16(kind==2 ? 0 : 0x1400),0xf4e1});
        if (kind==1) {
            initial.program[0].value=0xec00;initial.program[1].value=0x721a;
            initial.program[2].value=0x1400;initial.program[3].value=0xf4e1;
        }
        initial.initial.insert(initial.initial.end(),{{r::STATE_ST1,kind==0 ? 0x8800U : 0x800U},{r::STATE_BRC,55}});
        initial.expected.clear();
        auto final=observation("reject-before-source-read-or-BRC-write",{{r::STATE_ILLEGAL,1},{r::STATE_BRC,55}});
        final.expected.pop_back();
        clocked_register_case c{{{0,std::move(initial)},
            {20,std::move(final)}},2,20};
        c.require_idle=false;c.data_events=std::vector<clocked_data_event>{};
        cases.push_back(std::move(c));
    }
    return cases;
}
}
