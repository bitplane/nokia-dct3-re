// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
    std::vector<clocked_register_case> cases;
    for(u16 value:{0U,1U,0x8000U,0xffffU}) {
        auto initial=initial_case("ordered-independent-IO-input",{0x74f8,0x5000,0x27,0x74f8,0x5001,0x27,0xf4e1});
        auto final=observation("two-fresh-inputs-and-restored-stream-cursor",{{r::STATE_IDLE,1}},{{0x5000,value},{0x5001,u16(value^0x5a5a)}});
        clocked_register_case c{{{0,std::move(initial)},{32,std::move(final)}},9,32};
        c.scripted_io_reads=true;
        c.io_events=std::vector<clocked_data_event>{{false,0x27,value,0,false},{false,0x27,u16(value^0x5a5a),0,false}};
        cases.push_back(std::move(c));
    }
    return cases;
}
}
