// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u64 values[]{0,1,0x8000000000ULL,0xffffffffffULL,0x7fffffffffULL,0x0100000000ULL,0x0080000000ULL,0x00ffffffffULL};
	for (unsigned family = 0; family < 3; ++family)
	for (unsigned condition = 0; condition < (family == 2 ? 2U : 12U); ++condition)
	for (unsigned pattern = 0; pattern < (family == 2 ? 2U : 8U); ++pattern)
	{
		bool const returning = family == 1, tc = family == 2, b = !tc && condition >= 6;
		u64 const value = values[pattern];
		s64 const signed_value = value & 0x8000000000ULL ? s64(value) - (s64(1) << 40) : s64(value);
		bool const taken = tc ? pattern == condition :
			std::array<bool,6>{signed_value >= 0,signed_value < 0,value != 0,value == 0,signed_value > 0,signed_value <= 0}[condition % 6];
		u64 const changed = tc ? unsigned(taken) : taken ? std::array<u64,6>{0x8000000000ULL,0,0,1,0,1}[condition % 6] :
			std::array<u64,6>{0,0x8000000000ULL,1,0,1,0}[condition % 6];
		u32 const base = (family + condition + pattern) & 1 ? 0x20000 : 0;
		u16 const opcode = tc ? condition ? 0xf930 : 0xf920 : (returning ? 0xfc42 : 0xf942) + condition % 6 + (b ? 8 : 0);
		auto initial = initial_case("conditional-control", {opcode,u16(returning ? 0xf4e1 : 0x700),0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.program.insert(initial.program.end(), {{base + 0x700,u16(returning ? 0xf4e1 : 0xf495)},{base + 0x701,0xfc00}});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa608 | (tc ? pattern << 12 : 0)},
			{r::STATE_A,b ? 0x1234 : value},{r::STATE_B,b ? value : 0x2345}});
		if (returning) initial.data = {{0x1000,0x700}};
		unsigned const point = returning ? std::array<unsigned,4>{3,4,5,7}[(condition + pattern) % 4] :
			std::array<unsigned,4>{5,6,7,9}[(condition + pattern) % 4];
		clocked_register_case c{{{0,std::move(initial)}},point,32};
		for (unsigned tick = 1; tick <= 32; ++tick) {
			auto state = observation("conditional-ownership", {});
			if (!taken) state.expected.push_back({r::STATE_SP,0x1000});
			if (returning) state.expected.push_back({r::STATE_IDLE,unsigned(tick >= (taken ? 11 : 9))});
			if (tick == 32) state.expected.insert(state.expected.end(), {
				{r::STATE_A,b ? 0x1234 : changed},{r::STATE_B,b ? changed : 0x2345},
				{STATE_GENPC,base + (returning ? taken ? 0x701 : 0x602 : 0x603)},
				{r::STATE_SP,0x1000 + unsigned(returning && taken)},
				{r::STATE_ST0,0xa608 | (tc ? (pattern ^ 1) << 12 : 0)},{r::STATE_ST1,0x800}});
			c.states.push_back({tick,std::move(state)});
		}
		c.actions = {{5,{}}};
		if (returning) {
			c.actions.back().data = {{0x1000,0xdead}};
			c.actions.back().values = {{int(b ? r::STATE_B : r::STATE_A),changed}};
		} else {
			c.actions.back().program = {{base + 0x601,0xdead}};
			c.actions.push_back({6,{{int(b ? r::STATE_B : r::STATE_A),changed}}});
			if (tc) c.actions.back().values.push_back({r::STATE_ST0,0xa608 | ((pattern ^ 1) << 12)});
		}
		c.program_events = {};
		if (!returning) c.program_events.push_back({false,base + 0x601,0x700,0,false});
		if (taken) c.program_events.push_back({false,base + 0x700,u16(returning ? 0xf4e1 : 0xf495),returning ? 7U : 8U});
		c.data_events = std::vector<clocked_data_event>{};
		if (taken) {
			if (!returning) c.data_events->push_back({true,0xfff,0x602,7});
			c.data_events->push_back({false,returning ? 0x1000U : 0xfffU,u16(returning ? 0x700 : 0x602),returning ? 5U : 0U,returning});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
