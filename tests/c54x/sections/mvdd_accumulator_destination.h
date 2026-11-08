// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    constexpr std::array<u64,2> seeds{0x123456789aULL,0xabcdeffedcULL};
    auto replace=[](u64 original,unsigned field,u16 value) {
        unsigned const shift=16*(field%3);
        u64 const mask=u64(field%3==2 ? 0xff : 0xffff)<<shift;
        return (original&~mask)|((u64(value)<<shift)&mask);
    };
    auto extract=[](u64 value,unsigned field) { return u16((value>>(16*(field%3)))&(field%3==2 ? 0xff : 0xffff)); };
    for (unsigned field=0; field<6; ++field) for (u16 value:{0,1,0x8000,0xffff})
    for (bool increment:{false,true}) for (unsigned consumer=0; consumer<3; ++consumer) {
        unsigned const owner=field/3,address=8+field;
        unsigned const read_field=consumer==1 ? 3*owner+(field%3+1)%3 : field;
        u32 const page=(field+consumer+unsigned(increment))&1 ? 0x20000 : 0;
        u16 const opcode=increment ? 0xe59a : 0xe556;
        auto initial=initial_case("MVDD-RAM-to-accumulator-field",{opcode});
        if (consumer==2) {
            initial.program[1].value=u16(0x7100|address);
            initial.program[2].value=0x1400;
            initial.program[3].value=0xf4e1;
        } else {
            initial.program[1].value=u16((owner ? 0x0000 : 0x0100)|(8+read_field));
            initial.program[2].value=0xf4e1;
        }
        for (auto &word:initial.program) word.address+=page;
        initial.initial.insert(initial.initial.end(),{{STATE_GENPC,page+0x600},{r::STATE_ST0,0},{r::STATE_ST1,0x100},
            {r::STATE_AR3,0x1201},{r::STATE_AR4,address},
            {r::STATE_A,!owner || consumer==2 ? seeds[0] : 0},
            {r::STATE_B,owner || consumer==2 ? seeds[1] : 0}});
        initial.data={{0x1201,value},{0x1400,0xdead}};
        auto expected=seeds;
        expected[owner]=replace(expected[owner],field,value);
        u16 const read_value=extract(expected[owner],read_field);
        if (consumer!=2) expected[owner^1]=u64(s64(s16(read_value)))&0xffffffffffULL;
        auto final=observation("mapped-write-and-younger-read",{{r::STATE_IDLE,1},
            {r::STATE_A,expected[0]},{r::STATE_B,expected[1]},
            {r::STATE_AR3,increment ? 0x1202U : 0x1200U},
            {r::STATE_AR4,increment ? address+1 : address-1},{r::STATE_ST0,0},{r::STATE_ST1,0x100}},
            {{0x1201,0xdead},{0x1400,consumer==2 ? read_value : u16(0xdead)}});
        clocked_register_case c{{{0,std::move(initial)},{24,std::move(final)}},5+(field&1),24};
        c.actions={{5,{},false,{},{},{{0x1201,0xdead}}}};
        c.data_events=std::vector<clocked_data_event>{{false,0x1201,value,5}};
        if (consumer==1) c.data_events->push_back({false,8+read_field,read_value,6});
        c.data_events->push_back({true,address,value,6});
        if (consumer==2) {
            c.data_events->push_back({false,address,read_value,6});
            c.data_events->push_back({true,0x1400,read_value,7});
        }
        cases.push_back(std::move(c));
    }
    for (unsigned address:{9U,12U}) for (unsigned page:{0U,0x20000U}) {
        auto initial=initial_case("block-repeated-MVDD-accumulator-field",{0xf072,u16(0x602),0xe590,0xf4e1});
        for (auto &word:initial.program) word.address+=page;
        initial.initial.insert(initial.initial.end(),{{STATE_GENPC,page+0x600},{r::STATE_ST1,0x100},{r::STATE_BRC,2},
            {r::STATE_AR2,address},{r::STATE_AR3,0x1200},{r::STATE_A,seeds[0]},{r::STATE_B,seeds[1]}});
        initial.data={{0x1200,1},{0x1201,0x8000},{0x1202,0xffff}};
        auto expected=seeds;
        expected[(address-8)/3]=replace(expected[(address-8)/3],address-8,0xffff);
        auto final=observation("block-continuation-after-three-mapped-writes",{{r::STATE_IDLE,1},
            {r::STATE_A,expected[0]},{r::STATE_B,expected[1]},
            {r::STATE_AR2,address},{r::STATE_AR3,0x1203},{r::STATE_BRC,0},{r::STATE_ST1,0x100}});
        clocked_register_case c{{{0,std::move(initial)},{40,std::move(final)}},8,40};
        c.retired_instruction=retired_instruction_check{page+0x602,0xe590,3,0};
        cases.push_back(std::move(c));
    }
    for (unsigned kind=0; kind<4; ++kind) {
        unsigned const source=kind==1 ? 8 : 0x1200;
        unsigned const destination=kind==0 ? 7 : kind==3 ? 0x0e : 9;
        auto initial=initial_case("unqualified-MVDD-mapped-composition",{u16(kind==2 ? 0xec00 : 0xe510),0xf4e1});
        if (kind==2) { initial.program[1].value=0xe510; initial.program[2].value=0xf4e1; }
        initial.initial.insert(initial.initial.end(),{{r::STATE_AR2,destination},{r::STATE_AR3,source},
            {r::STATE_A,seeds[0]},{r::STATE_B,seeds[1]}});
        initial.expected.clear();
        auto final=observation("model-limit-rejects-before-bus-effects",{{r::STATE_ILLEGAL,1},
            {r::STATE_AR2,destination},{r::STATE_AR3,source},{r::STATE_A,seeds[0]},{r::STATE_B,seeds[1]}});
        final.expected.pop_back();
        clocked_register_case c{{{0,std::move(initial)},{20,std::move(final)}},3,20};
        c.require_idle=false;
        c.forbid_data_access=true;
        cases.push_back(std::move(c));
    }
    return cases;
}
}
