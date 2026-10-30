// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 inputs[]{0,1,0xff,0x100,0x101,0x7f80,0x8000,0xffff,
		0x1234,0xabcd,0x5500,0x00aa,0xff00,0x00ff,0x8080,0x7fff};
	for (unsigned count : {0U,1U,2U,7U})
	for (unsigned flags = 0; flags < 16; ++flags)
	for (unsigned pattern = 0; pattern < 16; ++pattern)
	{
		u16 const st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0) |
			(flags & 4 ? 0x40 : 0) | (flags & 8 ? 0x4000 : 0);
		auto initial = initial_case("block-byte-swap", {0xf072,0x606,0x1386,0xf678,0xf3e8,0xf1a0,0x8196,0xf4e1});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0xe08},{r::STATE_ST1,st1},
			{r::STATE_BRC,count},{r::STATE_A,0xdead},{r::STATE_B,0xff80000000ULL},{r::STATE_AR6,0x5000}});
		auto final = observation("complete", {{r::STATE_BRC,0},{r::STATE_ST1,st1},
			{r::STATE_ST0,0x608},{r::STATE_AR6,0x5001 + count},{r::STATE_SP,0x1000}});
		clocked_register_case c{{{0,std::move(initial)}},
			std::array<unsigned,3>{3,6,13}[(count + flags + pattern) % 3],64};
		c.data_events = std::vector<clocked_data_event>{};
		for (unsigned i = 0; i <= count; ++i)
		{
			u16 const value = inputs[pattern] ^ u16(i * 0x1357);
			u16 const swapped = u16((value << 8) | (value >> 8));
			c.states.front().observation.data.push_back({0x5000 + i,value});
			final.expected_data.push_back({0x5000 + i,swapped});
			// The next body Read and older Write share a model clock.
			c.data_events->push_back({false,0x5000 + i,value,9 + i * 5});
			if (i) c.data_events->push_back({true,0x5000 + i - 1,
				u16(((inputs[pattern] ^ u16((i - 1) * 0x1357)) << 8) |
				((inputs[pattern] ^ u16((i - 1) * 0x1357)) >> 8)),9 + i * 5});
			if (i == count) c.data_events->push_back({true,0x5000 + i,swapped,14 + i * 5});
		}
		// Decode-owned BRAF/count progress, independently of store retirement.
		for (unsigned tick = 4; tick < 14 + count * 5; ++tick)
		{
			unsigned const ended = tick < 11 ? 0 : (tick - 11) / 5 + 1;
			c.states.push_back({tick,observation("loop-state", {{r::STATE_BRC,ended < count ? count - ended : 0},
				{r::STATE_ST1,unsigned(st1 | (tick < 7 + count * 5 ? 0x8000 : 0))}})});
		}
		c.states.push_back({64,std::move(final)});
		cases.push_back(std::move(c));
	}
	// Interrupts arrive during the second body. Preserve ordering, not a fixed
	// ISR duration: that was deliberately not a timing contract in the old bank.
	for (unsigned interrupt = 1; interrupt <= 2; ++interrupt)
	for (unsigned page = 0; page < 2; ++page)
	// Snapshot after the synchronized host pin has latched, not in its issue tick.
	for (unsigned point : {3U,6U,9U,13U,15U,20U})
	{
		u32 const base = page * 0x20000;
		int const line = interrupt == 2 ? INPUT_LINE_NMI : 2;
		u32 const vector = base + (interrupt == 2 ? 4 : 0x48);
		auto initial = initial_case("block-swap-interrupt", {0xf072,0x606,0x1386,0xf678,0xf3e8,0xf1a0,0x8196,0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.program.insert(initial.program.end(), {{vector,0xf495},{vector + 1,0xf4eb}});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xe08},
			{r::STATE_ST1,interrupt == 1 ? 0 : 0x800},{r::STATE_IMR,interrupt == 1 ? 4 : 0},
			{r::STATE_BRC,2},{r::STATE_A,0xdead},{r::STATE_B,0xff80000000ULL},{r::STATE_AR6,0x5000}});
		auto final = observation("returned", {{r::STATE_BRC,0},{r::STATE_AR6,0x5003},
			{r::STATE_ST1,0,0x8000},{r::STATE_SP,0x1000}});
		clocked_register_case c{{{0,std::move(initial)}},point,96};
		c.data_ranges = {{0x5000,0x50ff}};
		c.data_events = std::vector<clocked_data_event>{};
		for (unsigned i = 0; i < 3; ++i)
		{
			u16 const value = 0x1234 ^ u16(i * 0x1357);
			c.states.front().observation.data.push_back({0x5000 + i,value});
			final.expected_data.push_back({0x5000 + i,u16((value << 8) | (value >> 8))});
			c.data_events->push_back({false,0x5000 + i,value,0,false});
			if (i) {
				u16 const previous = 0x1234 ^ u16((i - 1) * 0x1357);
				c.data_events->push_back({true,0x5000 + i - 1,u16((previous << 8) | (previous >> 8)),0,false});
			}
			if (i == 2) c.data_events->push_back({true,0x5002,u16((value << 8) | (value >> 8)),0,false});
		}
		c.states.push_back({96,std::move(final)});
		c.actions = {{14,{},false,{},{{line,ASSERT_LINE},{line,CLEAR_LINE}}}};
		c.irq = clocked_irq_expectation{line,14,96,3};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
