// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    // SPRU131G7.5.10: mapped writes preserve sibling fields and are visible
    // to the following direct accumulator consumer without an ALU latency.
    for(unsigned source=8;source<=13;++source)for(unsigned destination=8;destination<=13;++destination)
    for(u64 a:{0ULL,0x123456789aULL,0xff80008000ULL,0xffffffffffULL}) {
        u64 const b=0xfedcba9876ULL;
        unsigned const source_shift=16*((source-8)%3),shift=16*((destination-8)%3);
        u16 const value=u16((source<11 ? a : b)>>source_shift);
        u64 const mask=(shift==32 ? 0xffULL : 0xffffULL)<<shift;
        u64 after_a=a,after_b=b;
        auto &after=destination<11 ? after_a : after_b;
        after=(after&~mask)|((u64(value)<<shift)&mask);
        auto initial=initial_case("MVMD-mapped-accumulator-fields",{u16(0x7300|source),u16(destination),0xf4e1});
        initial.initial.insert(initial.initial.end(),{{r::STATE_A,a},{r::STATE_B,b}});
        auto final=observation("mapped-copy-preserves-sibling-fields",{{r::STATE_IDLE,1},{r::STATE_A,after_a},{r::STATE_B,after_b}});
        clocked_register_case c{{{0,std::move(initial)},{12,std::move(final)}},5,12};
        c.data_events=std::vector<clocked_data_event>{{false,source,value,5},{true,destination,value,6}};
        cases.push_back(std::move(c));
    }
    return cases;
}
}
