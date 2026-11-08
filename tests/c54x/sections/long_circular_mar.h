// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    constexpr std::array<std::array<int,3>,4> shapes{{{1,0,1},{13,12,3},{13,0,-3},{16,15,0}}};
    for (unsigned reg=0;reg<8;++reg) for (unsigned compatibility=0;compatibility<2;++compatibility)
    for (auto const &shape:shapes) {
        auto const [size,start,offset]=shape;
        unsigned const selected=compatibility && !reg ? 4 : reg;
        u32 const page=(reg+compatibility)&1 ? 0x20000 : 0;
        u16 const base=size==16 ? 0xff00 : 0x1200;
        u16 const status=compatibility && reg ? 0x608|(reg<<13) : 0x8608;
        unsigned const result=(start+offset+size)%size;
        auto initial=initial_case("long-circular-MAR",{u16(0x6df0|reg),u16(offset),0xf4e1});
        for (auto &word:initial.program)word.address+=page;
        initial.initial.insert(initial.initial.end(),{{STATE_GENPC,page+0x600},{r::STATE_ST0,0x8608},
            {r::STATE_ST1,0x800U|(compatibility ? 0x20U : 0)},{r::STATE_BK,unsigned(size)},
            {r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL}});
        auto before=observation("before-long-MAR-Access",{{r::STATE_ST0,0x8608}});
        auto after=observation("one-circular-offset-publication",{{r::STATE_ST0,status},
            {r::STATE_BK,unsigned(size)},{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL}});
        for (unsigned ar=0;ar<8;++ar) {
            unsigned const value=ar==selected ? base+start : 0x4000+ar;
            initial.initial.push_back({r::STATE_AR0+ar,value});
            before.expected.push_back({int(r::STATE_AR0+ar),value});
            after.expected.push_back({int(r::STATE_AR0+ar),ar==selected ? base+result : value});
        }
        auto retained=after;
        clocked_register_case c{{{0,std::move(initial)},{4,std::move(before)},
            {5,std::move(after)},{16,std::move(retained)}},
            compatibility ? 6U : 4U,16};
        c.forbid_data_access=true;
        cases.push_back(std::move(c));
    }
    return cases;
}
}
