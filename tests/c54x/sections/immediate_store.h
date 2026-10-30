// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	// ST #lk: DP, postincrement, absolute, peripheral; opcode-shaped literal.
	for (unsigned form = 0; form < 4; ++form) for (unsigned after = 0; after < 2; ++after)
	{
		u16 const target = form == 3 ? 0x25 : 0x700;
		u16 const opcode = std::array<u16,4>{0x7600,0x7691,0x76f8,0x7681}[form];
		auto initial = form == 2 ? initial_case("immediate-store", {opcode,target,0xf495}) :
			initial_case("immediate-store", {opcode,0xf495});
		initial.initial.push_back({r::STATE_AR1,target}); initial.data.push_back({target,0xaaaa});
		unsigned const write = form >= 2 ? 8 : 7;
		clocked_register_case c{{{0,std::move(initial)},
			{5,observation("address", {{r::STATE_AR1,form == 1 ? 0x701U : target}})},
			{write,observation("commit", {{r::STATE_AR1,form == 1 ? 0x701U : target}}, {{target,0xf495}})}},
			write - 1 + after,12};
		c.require_idle = false; c.check_pipeline_clock = true;
		c.data_events = std::vector<clocked_data_event>{{true,target,0xf495,write}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
