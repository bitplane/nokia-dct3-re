// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    constexpr std::array<u64,2> seeds{0x123456789aULL,0xabcdeffedcULL};
    for (unsigned field=0; field<6; ++field) for (u16 value:{0,1,0x8000,0xffff})
    for (bool absolute:{false,true}) {
        unsigned const owner=field/3,address=8+field,shift=16*(field%3);
        u32 const page=(field+unsigned(absolute))&1 ? 0x20000 : 0;
        unsigned const read=absolute ? 8 : 7,write=read+1;
        auto initial=initial_case("MVPD-program-to-accumulator-field",{u16(absolute ? 0x7cf8 : 0x7c8b)});
        if (absolute) { initial.program[1].value=address; initial.program[2].value=0x1700; }
        else initial.program[1].value=0x1700;
        initial.program[absolute ? 3 : 2].value=u16((owner ? 0 : 0x100)|address);
        initial.program[absolute ? 4 : 3].value=0xf4e1;
        for (auto &word:initial.program) word.address+=page;
        initial.program.push_back({page+0x1700,value});
        initial.program.push_back({(page^0x20000)+0x1700,u16(value^0xffff)});
        initial.initial.insert(initial.initial.end(),{{STATE_GENPC,page+0x600},{r::STATE_ST0,0},{r::STATE_ST1,0x100},
            {r::STATE_AR3,address},{r::STATE_A,owner ? 0 : seeds[0]},{r::STATE_B,owner ? seeds[1] : 0}});
        auto expected=seeds;
        u64 const mask=u64(field%3==2 ? 0xff : 0xffff)<<shift;
        expected[owner]=(expected[owner]&~mask)|((u64(value)<<shift)&mask);
        u16 const mapped=value&(field%3==2 ? 0xff : 0xffff);
        expected[owner^1]=u64(s64(s16(mapped)))&0xffffffffffULL;
        auto final=observation("retained-program-word-and-immediate-mapped-read",{{r::STATE_IDLE,1},
            {r::STATE_A,expected[0]},{r::STATE_B,expected[1]},{r::STATE_AR3,0x2222},
            {r::STATE_ST0,0},{r::STATE_ST1,0x100}});
        clocked_register_case c{{{0,std::move(initial)},{24,std::move(final)}},read,24};
        c.actions={{read-1,{{r::STATE_AR3,0x2222}}},{read,{},false,{{page+0x1700,0xdead}}}};
        c.program_events={{false,page+0x601,u16(absolute ? address : 0x1700),3}};
        if (absolute) c.program_events.push_back({false,page+0x602,0x1700,4});
        c.program_events.push_back({false,page+0x1700,value,read});
        c.data_events=std::vector<clocked_data_event>{{true,address,value,write}};
        cases.push_back(std::move(c));
    }
    for (bool repeated:{false,true}) {
        auto initial=initial_case("unqualified-MVPD-mapped-destination",{u16(repeated ? 0xec00 : 0x7cf8)});
        unsigned const prefix=repeated ? 1 : 0;
        initial.program[prefix].value=repeated ? 0x7c83 : 0x7cf8;
        if (repeated) {
            initial.initial.push_back({r::STATE_AR3,8});
            initial.program[prefix+1].value=0x1700;
        } else {
            initial.program[prefix+1].value=7;
            initial.program[prefix+2].value=0x1700;
        }
        initial.expected.clear();
        auto final=observation("reject-before-program-source-read",{{r::STATE_ILLEGAL,1},{r::STATE_A,0},{r::STATE_B,0}});
        final.expected.pop_back();
        clocked_register_case c{{{0,std::move(initial)},{20,std::move(final)}},4,20};
        c.require_idle=false;
        c.forbid_data_access=true;
        if (repeated) c.program_events={{false,0x602,0x1700,4}};
        else c.program_events={{false,0x601,7,3},{false,0x602,0x1700,4}};
        cases.push_back(std::move(c));
    }
    return cases;
}
}
