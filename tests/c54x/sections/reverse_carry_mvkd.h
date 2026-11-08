// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    for (bool add:{false,true}) for (unsigned shape=0;shape<3;++shape)
    for (bool compatibility:{false,true}) {
        unsigned const count=shape==0 ? 1 : shape==1 ? 8 : 256;
        bool const repeated=shape!=0;
        u16 const stride=shape==2 ? 0x80 : 4,first=add ? 0x2f00 : shape==2 ? 0x2fff : 0x2f07;
        unsigned const selected=compatibility ? 3 : 2;
        u16 const opcode=u16((add ? 0x70b8 : 0x70a0)|(compatibility ? 0 : 2));
        auto initial=initial_case("MVKD-reverse-carry-destination-permutation",{opcode,0x3000,0xf4e1});
        if (repeated) {
            initial.program[0].value=0xec00|(count-1);initial.program[1].value=opcode;
            initial.program[2].value=0x3000;initial.program[3].value=0xf4e1;
        }
        initial.initial.insert(initial.initial.end(),{{r::STATE_AR0,stride},
            {r::STATE_AR0+selected,first},{r::STATE_ST0,compatibility ? 0x6608U : 0x608U},
            {r::STATE_ST1,0x100U|(compatibility ? 0x20U : 0)},{r::STATE_BK,13}});
        clocked_register_case c{{{0,std::move(initial)}},repeated ? 10U : 5U,count+24};
        c.data_events=std::vector<clocked_data_event>{};
        auto final=observation("literal-progression-and-bit-reversed-writes",{{r::STATE_RC,0},
            {r::STATE_AR0,stride},{r::STATE_BK,13},{r::STATE_ST0,compatibility ? 0x6608U : 0x608U},
            {STATE_GENPC,repeated ? 0x604U : 0x603U}});
        u16 pointer=first,previous=0,previous_value=0;
        unsigned const read=6+unsigned(repeated);
        for (unsigned n=0;n<count;++n) {
            u16 const value=0x3210+n*0x31;
            c.states[0].observation.data.push_back({0x3000+n,value});
            c.data_events->push_back({false,0x3000+n,value,read+n});
            if (n)c.data_events->push_back({true,previous,previous_value,read+n});
            final.expected_data.push_back({pointer,value});
            previous=pointer;previous_value=value;
            // Independent bit-serial carry/borrow from bit15 toward bit0.
            u16 next=0;int carry=0;
            for (int bit=15;bit>=0;--bit) {
                int const sum=int(pointer>>bit&1)+(add ? int(stride>>bit&1) : -int(stride>>bit&1))+carry;
                next|=(sum&1)<<bit;
                carry=add ? int(sum>=2) : sum<0 ? -1 : 0;
            }
            pointer=next;
        }
        c.data_events->push_back({true,previous,previous_value,read+count});
        final.expected.push_back({int(r::STATE_AR0+selected),pointer});
        c.states.push_back({count+24,std::move(final)});
        cases.push_back(std::move(c));
    }
    auto initial=initial_case("reverse-carry-MVKD-retains-mapped-repeat-guard",{0xec07,0x70ba,0x3000,0xf4e1});
    initial.initial.insert(initial.initial.end(),{{r::STATE_AR0,4},{r::STATE_AR2,0x10}});
    initial.expected.clear();
    auto final=observation("reject-before-source-read-or-destination-modification",
        {{r::STATE_ILLEGAL,1},{r::STATE_AR2,0x10}});final.expected.pop_back();
    clocked_register_case rejected{{{0,std::move(initial)},{20,std::move(final)}},3,20};
    rejected.require_idle=false;rejected.forbid_data_access=true;
    cases.push_back(std::move(rejected));
    return cases;
}
}
