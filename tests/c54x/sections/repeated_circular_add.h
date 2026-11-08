// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    constexpr std::array<std::array<unsigned,3>,4> shapes{{{8,0,0},{8,7,7},{13,12,16},{1,0,25}}};
    for (unsigned destination=0; destination<2; ++destination)
    for (unsigned page:{0U,0x20000U}) for (unsigned mode=0; mode<4; ++mode)
    for (auto shape:shapes) {
        auto const [size,start,count]=shape;
        unsigned const stride=size==1 ? 1 : 2;
        int const step=mode==0 ? -1 : mode==1 ? -int(stride) : mode==2 ? 1 : int(stride);
        u16 const opcode=u16((destination<<8)|0xc3|(mode<<3));
        auto initial=initial_case("repeated-circular-ADD",{u16(0xec00|count),opcode,0x7714,0x55,0xf4e1});
        for (auto &word:initial.program) word.address+=page;
        initial.initial.insert(initial.initial.end(),{{STATE_GENPC,page+0x600},{r::STATE_ST1,0x100},
            {r::STATE_BK,size},{r::STATE_AR0,stride},{r::STATE_AR3,0x1200+start},{r::STATE_A,3},{r::STATE_B,7}});
        std::vector<u16> values;
        for (unsigned at=0; at<size; ++at) {
            values.push_back(at&1 ? u16(0xff00+at) : u16(3*at+1));
            initial.data.push_back({0x1200+at,values.back()});
        }
        std::array<u64,2> expected{3,7};
        unsigned cursor=start;
        std::vector<clocked_data_event> reads;
        for (unsigned at=0; at<=count; ++at) {
            expected[destination]=(expected[destination]+s16(values[cursor]))&0xffffffffffULL;
            clocked_data_event event{false,0x1200+cursor,values[cursor],0,false};
            if (at) event.cycles_from_previous=1;
            reads.push_back(event);
            cursor=(int(cursor)+size+step)%size;
        }
        auto final=observation("ordered-signed-circular-feedback",{{r::STATE_IDLE,1},
            {r::STATE_A,expected[0]},{r::STATE_B,expected[1]},
            {r::STATE_AR3,0x1200+cursor},{r::STATE_AR4,0x55},{r::STATE_BK,size},{r::STATE_ST1,0x100}});
        unsigned const finish=count+40;
        clocked_register_case c{{{0,std::move(initial)},{finish,std::move(final)}},6+count/2,finish};
        c.retired_instruction=retired_instruction_check{page+0x601,opcode,count+1,0};
        reads.push_back({true,0x14,0x55,0,false});
        c.data_events=std::move(reads);
        cases.push_back(std::move(c));
    }
    return cases;
}
}
