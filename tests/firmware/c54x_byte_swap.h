// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 inputs[]{0,1,0xff,0x100,0x101,0x7f80,0x8000,0xffff,
		0x1234,0xabcd,0x5500,0x00aa,0xff00,0x00ff,0x8080,0x7fff};
	constexpr u16 body[]{0x1386,0xf678,0xf3e8,0xf1a0,0x8196};
	for (unsigned bodies = 1; bodies <= 2; ++bodies)
	for (unsigned flags = 0; flags < 16; ++flags)
	for (unsigned pattern = 0; pattern < 16; ++pattern)
	{
		u16 const first = inputs[pattern], second = first ^ 0x5a3c;
		auto swap = [](u16 value) { return u16((value << 8) | (value >> 8)); };
		u16 const st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0) |
			(flags & 4 ? 0x40 : 0) | (flags & 8 ? 0x4000 : 0);
		auto initial = initial_case("byte-swap-sequence", {body[0]});
		for (unsigned n = 0; n < bodies * 5; ++n) initial.program[n].value = body[n % 5];
		initial.program[bodies * 5].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0xe08},{r::STATE_ST1,st1},
			{r::STATE_PMST,pattern & 1},{r::STATE_A,0xdead},{r::STATE_B,0xff80000000ULL},
			{r::STATE_AR6,0x5000}});
		initial.data = {{0x5000,first},{0x5001,second}};
		u16 const last = bodies == 1 ? first : second;
		auto final = observation("swapped", {{r::STATE_A,unsigned(last >> 8)},
			{r::STATE_B,(u32(last) << 8) | (last >> 8)},{r::STATE_AR6,0x5000 + bodies},
			{r::STATE_ST0,0x608},{r::STATE_ST1,st1},{r::STATE_PMST,pattern & 1}},
			{{0x5000,swap(first)}});
		if (bodies == 2) final.expected_data.push_back({0x5001,swap(second)});
		clocked_register_case c{{{0,std::move(initial)}},
			std::array<unsigned,3>{3,5,9}[(pattern + flags) % 3],24};
		// The single body retains its dependent intermediate accumulator/flag states.
		if (bodies == 1)
		{
			c.states.push_back({6,observation("loaded", {{r::STATE_B,first},{r::STATE_A,0xdead}})});
			c.states.push_back({7,observation("shifted", {{r::STATE_A,unsigned(first >> 8)},
				{r::STATE_ST0,unsigned(0x608 | (first & 0x80 ? 0x800 : 0))}})});
			c.states.push_back({8,observation("left", {{r::STATE_B,u32(first) << 8},{r::STATE_ST0,0x608}})});
			c.states.push_back({9,observation("merged", {{r::STATE_B,(u32(first) << 8) | (first >> 8)},
				{r::STATE_AR6,0x5001}})});
		}
		else c.checkpoint = std::array<unsigned,3>{3,9,10}[(pattern + flags) % 3];
		c.states.push_back({24,std::move(final)});
		c.actions = {{5,{}}}; c.actions.back().data = {{0x5000,0xdead}};
		c.data_events = std::vector<clocked_data_event>{{false,0x5000,first,5},{true,0x5000,swap(first),10}};
		if (bodies == 2)
		{
			c.actions.push_back({10,{}}); c.actions.back().data = {{0x5001,0xdead}};
			// Model order: the second Read overlaps the first Execute write.
			c.data_events->insert(c.data_events->begin() + 1,{false,0x5001,second,10});
			c.data_events->push_back({true,0x5001,swap(second),15});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
