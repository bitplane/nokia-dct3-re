// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	// Indirect RAM success / rejected aliases / direct low-RAM forwarding.
	for (unsigned bank = 0; bank < 3; ++bank)
	for (unsigned n = 0; n < (bank == 2 ? 512U : 256U); ++n)
	{
		bool const direct = bank == 2, rejected = bank == 1;
		unsigned const selector = direct ? 0 : n % 8;
		unsigned const gap = direct ? n / 256 : 0;
		unsigned const flags = direct ? (n % 32 + (n / 32) % 8 + gap) % 4 : (n / 128) % 2;
		bool const cmpt = flags & 1;
		unsigned const ar = !direct && cmpt && !selector ? 5 : selector;
		u16 const destination = direct ? 0x60 + n % 32 : rejected ?
			std::array<u16,4>{0,0x10,0x28,0x5f}[(n / 8) % 4] :
			std::array<u16,4>{0x60,0x68,0x70,0x7f}[(n / 8) % 4];
		u16 const literal = direct ? std::array<u16,8>{0,1,0x7fff,0x8000,0xffff,0xdead,0x7767,0xf4e1}[(n / 32) % 8] :
			std::array<u16,4>{0,1,0xb3a6,0xffff}[(n / 32) % 4];
		// Nonzero ARP is confined to compatibility mode.
		u16 const st0 = direct ? std::array<u16,4>{0x600,0xa633,0x67f,0}[flags] : cmpt ? 0xa633 : 0x633;
		u16 const st1 = 0xb00 | (cmpt ? 0x20 : 0) | (flags & 2 ? 0x4000 : 0);
		u16 const opcode = direct ? 0x7700 | destination : 0x7780 | selector;
		auto initial = initial_case("stm-ram", {opcode,literal,0xf4e1});
		if (direct) {
			initial.program[2].value = 0xf495;
			initial.program[2 + gap].value = 0x4800 | destination;
			initial.program[3 + gap].value = 0xf4e1;
		}
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_SP,direct ? 0x1234 : 0x1000},{r::STATE_A,0x123456789aULL},{r::STATE_B,0xabcdef0123ULL}});
		for (unsigned i = 0; i < 8; ++i)
			initial.initial.push_back({r::STATE_AR0 + i,i == ar ? 0x1200 | destination : 0x2000 + i});
		for (unsigned i = 0x60; i < 0x80; ++i) initial.data.push_back({i,0x5aa5});
		auto state = [&](unsigned tick) {
			auto s = observation("stm-state", {}); s.expected[0].value = rejected && tick >= 5;
			s.expected.insert(s.expected.end(), {{r::STATE_ST0,direct && tick > 5 ? st0 ^ 0x7fU : st0},
				{r::STATE_ST1,direct && tick > 5 ? st1 ^ 0x4000U : st1},
				{r::STATE_SP,direct ? tick > 5 ? 0x2345U : 0x1234U : 0x1000U},{r::STATE_B,0xabcdef0123ULL}});
			for (unsigned i = 0; i < 8; ++i) {
				u16 const expected = i != ar ? 0x2000 + i : rejected || tick < (direct ? 6U : 5U) ?
					0x1200 | destination : tick < 6 ? destination : 0xface;
				s.expected.push_back({int(r::STATE_AR0) + int(i),expected});
			}
			for (unsigned i = 0x60; i < 0x80; ++i)
				s.expected_data.push_back({i,!rejected && tick >= 7 && i == destination ? literal : 0x5aa5});
			if (tick == 20) s.expected.push_back({r::STATE_A,direct ? u64(literal) : 0x123456789aULL});
			return s;
		};
		initial.expected = state(0).expected;
		unsigned const point = direct ? std::array<unsigned,4>{3,4,5,7}[(n + n / 32) % 4] :
			std::array<unsigned,3>{4,5,7}[(n + n / 8 + n / 32) % 3];
		clocked_register_case c{{{0,std::move(initial)},{5,state(5)},{6,state(6)},{7,state(7)},{20,state(20)}},point,20};
		c.require_idle = !rejected; c.clear_pipeline_before_restore = true;
		c.retained_program_reads = {{0x601,literal}};
		c.actions.push_back({4,{},false,{{0x601,0xdead}}});
		if (!rejected) {
			clocked_register_action poison{5,{{r::STATE_AR0 + ar,0xface}}};
			if (direct) poison.values.insert(poison.values.end(), {{r::STATE_ST0,st0 ^ 0x7f},
				{r::STATE_ST1,st1 ^ 0x4000},{r::STATE_SP,0x2345}});
			c.actions.push_back(std::move(poison));
		}
		c.data_events = std::vector<clocked_data_event>{};
		if (!rejected) c.data_events->push_back({true,destination,literal,7});
		if (direct && gap) c.data_events->push_back({false,destination,literal,8});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
