// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u64 values[]{0,1,0xffffffffffULL,0x7fffffff,0xff80000000ULL,0x0100000000ULL};
	for (unsigned condition = 0; condition < 12; ++condition)
	for (unsigned pattern = 0; pattern < 6; ++pattern)
	for (unsigned mode = 0; mode < 16; ++mode)
	for (unsigned brc = 0; brc < 2; ++brc)
	{
		u64 const tested = values[pattern];
		s64 const value = tested & 0x8000000000ULL ? s64(tested) - (s64(1) << 40) : s64(tested);
		bool const take = std::array<bool,6>{value >= 0,value < 0,value != 0,value == 0,value > 0,value <= 0}[condition % 6];
		u64 const a = condition < 6 ? tested : 0x23456789, b = condition < 6 ? 0x23456789 : tested;
		u16 const source = std::array<u16,4>{0,0xffff,0x1234,0x8000}[(condition + pattern + mode + brc) % 4];
		u16 const address = 0x1403 + (mode & 3) * 16;
		// BK=4 starts at the final circular word; AR0=2 is the indexed decrement.
		u16 const next = (mode >> 2) == 0 ? address : (mode >> 2) == 1 ? address - 1 : (mode >> 2) == 2 ? address + 1 : address - 2;
		auto initial = initial_case("conditional-register-store", {
			u16((brc ? 0x9d00 : 0x9c00) | (mode << 4) | (condition < 6 ? condition + 2 : condition + 4)),0xf4e1});
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,brc ? 0xbeefU : unsigned(source)},
			{r::STATE_BRC,brc ? unsigned(source) : 0x55aaU},{r::STATE_AR0,2},{r::STATE_BK,4},{r::STATE_ST0,0x1e05}});
		for (unsigned ar = 2; ar < 6; ++ar) initial.initial.push_back({r::STATE_AR0 + ar,0x1403 + (ar - 2) * 16});
		initial.data = {{address,0xa55a}};
		clocked_register_case c{{{0,std::move(initial)}},(condition + pattern + mode + brc) & 1 ? 4U : 1U,24};
		c.pipeline_frontend = false;
		auto final = observation("conditional-read-writeback", {{STATE_GENPC,0x602},{r::STATE_A,a},{r::STATE_B,b},
			{r::STATE_T,brc ? 0xbeefU : unsigned(source)},{r::STATE_BRC,brc ? unsigned(source) : 0x55aaU},
			{r::STATE_ST0,0x1e05},{r::STATE_ST1,0x800}},{{address,u16(take ? source : 0xa55a)}});
		for (unsigned ar = 2; ar < 6; ++ar) final.expected.push_back({int(r::STATE_AR0 + ar),
			ar == 2 + (mode & 3) ? unsigned(next) : 0x1403 + (ar - 2) * 16});
		c.states.push_back({24,std::move(final)});
		c.data_events = std::vector<clocked_data_event>{{false,address,0xa55a,0,false},
			{true,address,u16(take ? source : 0xa55a),0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
