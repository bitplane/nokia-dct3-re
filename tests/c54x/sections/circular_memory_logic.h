// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    for (unsigned operation=0;operation<3;++operation) for (bool b:{false,true})
    for (unsigned mode=0;mode<4;++mode) for (unsigned form=0;form<3;++form) {
        unsigned const size=std::array<unsigned,3>{1,7,16}[(operation+unsigned(b)+form)%3];
        unsigned const start=mode<2 ? 0 : size-1, count=form ? 4 : 1;
        unsigned const selected=2+(operation+unsigned(b)+form)%4;
        int const step=mode==0 ? -1 : mode==1 ? -2 : mode==2 ? 1 : 2;
        u16 const opcode=u16(0x1800+operation*0x200+(b ? 0x100 : 0)+0xc0+mode*8+selected);
        std::vector<u16> code=form==0 ? std::vector<u16>{opcode,0x7716,0x55,0xf4e1} :
            form==1 ? std::vector<u16>{0xec03,opcode,0x7716,0x55,0xf4e1} :
                      std::vector<u16>{0xf072,0x602,opcode,0x7716,0x55,0xf4e1};
        auto initial=initial_case("circular-memory-logic",{0xf495});
        initial.program.clear();
        u32 const page=(operation+form+unsigned(b))&1 ? 0x20000 : 0;
        for (unsigned at=0;at<code.size();++at) initial.program.push_back({page+0x600+at,code[at]});
        u64 const source=0xa512345678ULL,other=0x8182838485ULL;
        initial.initial.insert(initial.initial.end(),{{STATE_GENPC,page+0x600},
            {r::STATE_A,b ? other : source},{r::STATE_B,b ? source : other},
            {r::STATE_ST0,0xb601},{r::STATE_ST1,0x900},{r::STATE_BK,size},
            {r::STATE_AR0,2},{int(r::STATE_AR0+selected),0x1200+start},{r::STATE_BRC,form==2 ? 3U : 0U}});
        for (unsigned at=0;at<size;++at) initial.data.push_back({0x1200+at,u16(0x8421^(at*0x1357))});
        unsigned cursor=start;
        u64 result=source;
        std::vector<clocked_data_event> events;
        for (unsigned at=0;at<count;++at) {
            u16 const value=u16(0x8421^(cursor*0x1357));
            events.push_back({false,0x1200+cursor,value,0,false});
            result=operation==0 ? result&value : operation==1 ? result|value : result^value;
            cursor=(int(cursor)+int(size)+step)%size;
        }
        events.push_back({true,0x16,0x55,0,false});
        auto final=observation("ordered-read-wrap-live-accumulator-and-continuation",{
            {r::STATE_IDLE,1},{r::STATE_A,b ? other : result},{r::STATE_B,b ? result : other},
            {int(r::STATE_AR0+selected),0x1200+cursor},{r::STATE_AR6,0x55},
            {r::STATE_ST0,0xb601},{r::STATE_ST1,0x900},{r::STATE_BK,size},{r::STATE_BRC,0}});
        clocked_register_case c{{{0,std::move(initial)},{96,std::move(final)}},8+mode,96};
        c.data_events=std::move(events);
        cases.push_back(std::move(c));
    }
    for (unsigned operation=0;operation<3;++operation) for (unsigned mode=0;mode<4;++mode) {
        auto initial=initial_case("circular-logic-mapped-limit",{u16(0x1800+operation*0x200+0xc3+mode*8),0xf4e1});
        initial.initial.insert(initial.initial.end(),{{r::STATE_AR0,2},{r::STATE_AR3,0x25},
            {r::STATE_BK,16},{r::STATE_A,0x1234},{r::STATE_B,0x5678}});
        initial.expected.clear();
        auto final=observation("reject-before-mapped-read-or-pointer-update",{
            {r::STATE_ILLEGAL,1},{r::STATE_AR3,0x25},{r::STATE_A,0x1234},{r::STATE_B,0x5678}});
        final.expected.pop_back();
        clocked_register_case c{{{0,std::move(initial)},{24,std::move(final)}},3,24};
        c.require_idle=false;c.forbid_data_access=true;
        cases.push_back(std::move(c));
    }
    return cases;
}
}
