// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    for (unsigned form=0;form<4;++form)
    for (unsigned mode=0;mode<4;++mode)
    for (unsigned count:{0U,6U}) {
        int const step=mode==0 ? -1 : mode==1 ? -2 : mode==2 ? 1 : 2;
        u16 const opcode=0x80c4|(form<<8)|(mode<<3);
        auto initial=initial_case("repeated-circular-STL-STH",{0xf070,u16(count),opcode,0xf4e1});
        initial.initial.insert(initial.initial.end(),{{r::STATE_ST1,0x100},{r::STATE_PMST,0},
            {r::STATE_BK,4},{r::STATE_AR0,2},{r::STATE_AR4,0x1203},
            {r::STATE_A,0x12345678},{r::STATE_B,0xabcdef01}});
        clocked_register_case c{{{0,std::move(initial)}},5,count+32};
        c.data_events=std::vector<clocked_data_event>{};
        unsigned cursor=3;
        u32 const source=form&1 ? 0xabcdef01 : 0x12345678;
        for (unsigned n=0;n<=count;++n) {
            c.data_events->push_back({true,0x1200+cursor,u16(source>>(form&2 ? 16 : 0)),8+n});
            cursor=(int(cursor)+step+4)%4;
        }
        c.states.push_back({count+32,observation("circular-count-plus-one-stores",
            {{r::STATE_RC,0},{r::STATE_AR4,0x1200+cursor},{r::STATE_BK,4},
             {r::STATE_A,0x12345678},{r::STATE_B,0xabcdef01}})});
        cases.push_back(std::move(c));
    }
    return cases;
}
}
