// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    for (bool increment:{false,true}) for (u16 value:{0,0xb0,0xe0,0xffff})
    for (u32 page:{0U,0x20000U}) {
        u16 const target=increment ? 0x2001 : 0x1fff;
        auto initial=initial_case("constant-ST-pointer-update-before-MVDD-destination",
            {0x7713,0x2000,u16(increment ? 0x7693 : 0x768b),0x5aa5,0xe501,0xf4e1});
        for (auto &word:initial.program) word.address+=page;
        initial.initial.insert(initial.initial.end(),{{STATE_GENPC,page+0x600},{r::STATE_AR2,0x3000},
            {r::STATE_AR3,0x7777},{r::STATE_ST0,0},{r::STATE_ST1,0x100},
            {r::STATE_A,0x123456789aULL},{r::STATE_B,0xabcdef1234ULL}});
        initial.data={{0x3000,value},{0x2000,0xdead},{target,0xbeef}};
        auto final=observation("ordered-constant-and-copy-writes-with-retained-pointer",
            {{r::STATE_IDLE,1},{r::STATE_AR2,0x3000},{r::STATE_AR3,target},
             {r::STATE_ST0,0},{r::STATE_ST1,0x100},
             {r::STATE_A,0x123456789aULL},{r::STATE_B,0xabcdef1234ULL}},
            {{0x2000,0x5aa5},{target,value},{0x3000,value}});
        clocked_register_case c{{{0,std::move(initial)},{30,std::move(final)}},6,30};
        c.data_events=std::vector<clocked_data_event>{{true,0x13,0x2000,5},
            {false,0x3000,value,9},{true,0x2000,0x5aa5,9},{true,target,value,10}};
        cases.push_back(std::move(c));
    }
    return cases;
}
}
