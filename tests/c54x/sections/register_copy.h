// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned source = 0; source < 9; ++source) for (unsigned destination = 0; destination < 9; ++destination)
	for (u16 value : {0U,1U,0x8000U,0xf495U})
	{
		auto initial = initial_case("register-copy", {u16(0xe700 | (source << 4) | destination)});
		if (destination < 8) initial.program[1].value = 0x1080 | destination;
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0x608},{r::STATE_ST1,0x800},{r::STATE_A,0x1111}});
		for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({int(r::STATE_AR0) + int(ar),source == ar ? value : 0x1000 + ar});
		initial.initial.push_back({r::STATE_SP,source == 8 ? value : 0x1008U});
		if (value >= 0x60) initial.data = {{value,0x55aa}};
		auto final = observation("consumer", {{r::STATE_A,destination < 8 ? value < 0x60 ? 0U : 0x55aaU : 0x1111U}},
			{{0x10 + destination,value}});
		clocked_register_case c{{{0,std::move(initial)},
			{4,observation("access-publication", {}, {{0x10 + destination,value}})},
			{7,std::move(final)}},std::array<unsigned,3>{3,4,6}[(source + destination + value) % 3],16};
		c.require_idle = false;
		c.actions = {{3,{}}}; c.actions.front().data = {{0x10 + source,0xdead}};
		c.data_events = std::vector<clocked_data_event>{{false,0x10 + source,value,3},{true,0x10 + destination,value,4}};
		if (destination < 8) c.data_events->push_back({false,value,u16(value < 0x60 ? 0 : 0x55aa),6});
		cases.push_back(std::move(c));
	}
	// Table7-9 Category I requires one NOP after MVMM SP in compiler mode.
	for (unsigned compiler = 0; compiler < 2; ++compiler) for (unsigned point : {3U,4U,6U})
	{
		u16 const address = compiler ? 0x1202 : 0x402;
		auto initial = initial_case("register-copy-sp", {0xe718});
		initial.program[1 + compiler].value = 0x1002;
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,8},{r::STATE_ST1,compiler ? 0x4800 : 0x800},
			{r::STATE_AR1,0x1200},{r::STATE_SP,0x1000}});
		initial.data = {{address,address}};
		auto final = observation("sp-consumer", {{r::STATE_SP,0x1200},{r::STATE_A,address}});
		clocked_register_case c{{{0,std::move(initial)},
			{4,observation("sp-publication", {{r::STATE_SP,0x1200}})},
			{7 + compiler,std::move(final)}},point,16};
		c.require_idle = false;
		c.data_events = std::vector<clocked_data_event>{{false,0x11,0x1200,3},{true,0x18,0x1200,4},
			{false,address,address,6 + compiler}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
