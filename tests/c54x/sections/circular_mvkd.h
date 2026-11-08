// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    for (unsigned mode=0; mode<4; ++mode) for (unsigned size:{8U,13U})
    for (unsigned end:{0U,1U}) for (bool alias:{false,true}) for (unsigned page:{0U,0x20000U}) {
        unsigned const cursor=end ? size-1 : 0;
        unsigned const destination=0x1200+cursor,source=alias ? destination : 0x1100;
        int const step=mode==0 ? -1 : mode==1 ? -2 : mode==2 ? 1 : 2;
        unsigned const next=(int(cursor)+size+step)%size;
        u16 const value=end ? 0x8123 : 0x44a5,opcode=u16(0x70c3|(mode<<3));
        auto initial=initial_case("circular-MVKD",{opcode,u16(source),0xe955,0xf4e1});
        for (auto &word:initial.program) word.address+=page;
        initial.initial.insert(initial.initial.end(),{{STATE_GENPC,page+0x600},{r::STATE_ST1,0x100},
            {r::STATE_AR0,2},{r::STATE_AR3,destination},{r::STATE_BK,size},
            {r::STATE_A,0x123456789aULL},{r::STATE_B,0xffabcdef01ULL}});
        initial.data.push_back({source,value});
        if (!alias) initial.data.push_back({destination,0xdead});
        auto final=observation("read-before-circular-store",{{r::STATE_IDLE,1},{r::STATE_AR3,0x1200+next},
            {r::STATE_BK,size},{r::STATE_ST1,0x100},
            {r::STATE_A,0x123456789aULL},{r::STATE_B,0x55}},{{destination,value},{source,value}});
        clocked_register_case c{{{0,std::move(initial)},{24,std::move(final)}},6,24};
        clocked_data_event write{true,destination,value,0,false};
        write.cycles_from_previous=1;
        c.data_events=std::vector<clocked_data_event>{{false,source,value,0,false},write};
        cases.push_back(std::move(c));
    }
    return cases;
}
}
