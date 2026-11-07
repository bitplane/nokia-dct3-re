// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    for(bool sub:{false,true})for(unsigned selector=0;selector<4;++selector)
    for(unsigned count:{0,1,25,255})for(u64 a:{3ULL,0xfffffffffdULL,0x7fffffffULL}) {
        u16 const opcode=(sub ? 0xf420 : 0xf400)|(selector<<8);
        std::array<u64,2> expected{a,7};
        for(unsigned i=0;i<=count;++i) {
            u64 const operand=expected[selector>>1];
            auto &destination=expected[selector&1];
            destination=(sub ? destination-operand : destination+operand)&0xffffffffffULL;
        }
        auto initial=initial_case("repeated-live-accumulator-ADD-SUB",{u16(0xec00|count),opcode,0xf4e1});
        initial.initial.insert(initial.initial.end(),{{r::STATE_A,a},{r::STATE_B,7},{r::STATE_ST1,0x100}});
        auto final=observation("exact-repeated-40-bit-feedback",{{r::STATE_IDLE,1},
            {r::STATE_A,expected[0]},{r::STATE_B,expected[1]}});
        unsigned const finish=count+40;
        clocked_register_case c{{{0,std::move(initial)},{finish,std::move(final)}},8+count/2,finish};
        c.retired_instruction=retired_instruction_check{0x601,opcode,count+1,0};
        c.data_events=std::vector<clocked_data_event>{};
        cases.push_back(std::move(c));
    }
    return cases;
}
}
