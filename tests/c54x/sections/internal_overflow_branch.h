// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    for (unsigned accumulator=0;accumulator<2;++accumulator) for (bool polarity:{false,true})
    for (bool overflow:{false,true}) for (unsigned count:{0U,2U}) for (u32 page:{0U,0x20000U}) {
        unsigned const bit=10-accumulator;
        u16 const mask=1U<<bit,other_mask=1U<<(accumulator ? 10 : 9);
        u16 const status=0x1825|other_mask|(overflow ? mask : 0);
        auto initial=initial_case("internal-overflow-BCD-retains-predicate-and-block",
            {0xf072,0x60e,0xf495,u16(0xfa60|(accumulator<<3)|(polarity ? 0x10 : 0)),0x60c,
             u16((overflow ? 0xf4b0 : 0xf5b0)|bit),0x6d94,0x7693,0x1111,0xf073,0x60e,0xf495,
             0x7693,0x2222,0x6d96,0xf4e1});
        for (auto &word:initial.program)word.address+=page;
        initial.initial.insert(initial.initial.end(),{{STATE_GENPC,page+0x600},{r::STATE_A,0x1234},
            {r::STATE_B,0x2345},{r::STATE_ST0,status},{r::STATE_ST1,0x900},{r::STATE_BRC,count},
            {r::STATE_AR3,0x6000},{r::STATE_AR4,0x1400},{r::STATE_AR6,0x1600}});
        auto final=observation("one-selected-arm-and-two-delays-per-iteration",
            {{STATE_GENPC,page+0x610},{r::STATE_A,0x1234},{r::STATE_B,0x2345},
             {r::STATE_ST0,u16((status&~mask)|(!overflow ? mask : 0))},{r::STATE_ST1,0x900},
             {r::STATE_BRC,0},{r::STATE_RSA,0x602},{r::STATE_REA,0x60e},
             {r::STATE_AR3,0x6001+count},{r::STATE_AR4,0x1401+count},{r::STATE_AR6,0x1601+count}});
        clocked_register_case c{{{0,std::move(initial)}},overflow ? 10U : 14U,192};
        c.data_events=std::vector<clocked_data_event>{};
        for (unsigned n=0;n<=count;++n) {
            bool const taken=(n ? !overflow : overflow)==polarity;
            u16 const value=taken ? 0x2222 : 0x1111;
            final.expected_data.push_back({0x6000+n,value});
            c.data_events->push_back({true,0x6000+n,value,0,false});
        }
        c.states.push_back({192,std::move(final)});
        cases.push_back(std::move(c));
    }
    return cases;
}
}
