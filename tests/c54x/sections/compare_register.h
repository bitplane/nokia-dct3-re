// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 left[]{0,0,0xffff,0x8000}, right[]{0,0xffff,0,0x7fff};
	for (unsigned ar = 0; ar < 8; ++ar) for (unsigned condition = 0; condition < 4; ++condition)
	for (unsigned pair = 0; pair < 4; ++pair)
	{
		u16 const lhs = ar ? left[pair] : right[pair];
		bool const result = std::array<bool,4>{lhs == right[pair],lhs < right[pair],lhs > right[pair],lhs != right[pair]}[condition];
		u16 const old = 0xa608 | ((ar + condition + pair) % 2 ? 0x1000 : 0);
		auto initial = initial_case("unsigned-register-compare", {u16(0xf4a8 | (condition << 8) | ar)});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,old},{r::STATE_ST1,0x800},{r::STATE_AR0,right[pair]}});
		if (ar) initial.initial.push_back({int(r::STATE_AR0) + int(ar),lhs});
		initial.expected.push_back({r::STATE_ST0,old});
		auto final = observation("tc-published", {{r::STATE_ST0,unsigned(0xa608 | (result ? 0x1000 : 0))},{r::STATE_ST1,0x800}});
		clocked_register_case c{{{0,std::move(initial)},{6,std::move(final)}},4 + (ar + condition + pair) % 3,10};
		c.require_idle = false; c.check_pipeline_clock = true;
		c.actions = {{4,{{r::STATE_AR0,0x4321}}}};
		if (ar) c.actions.front().values.push_back({int(r::STATE_AR0) + int(ar),0x9876});
		c.data_events = std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	}
	// SPRU131G table7-7 Category I: CMPR requires 0/0/1/2 NOPs
	// after STM/MVDK/POPM/store, absent an extra preceding DAGEN conflict.
	for (unsigned producer = 0; producer < 4; ++producer) for (unsigned replay = 0; replay < 3; ++replay)
	{
		unsigned const prefix = producer < 2 ? 2 : 1;
		unsigned const gap = std::array<unsigned,4>{0,0,1,2}[producer], access = 4 + prefix + gap;
		auto initial = initial_case("compare-after-register-producer", {u16(producer == 0 ? 0x7713 : producer == 1 ? 0x7170 : producer == 2 ? 0x8a13 : 0x8013)});
		if (prefix == 2) initial.program[1].value = producer ? 0x13 : 0xbeef;
		initial.program[prefix + gap].value = 0xf4ab;
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0xa600},{r::STATE_ST1,0x800},
			{r::STATE_AR0,0xbeef},{r::STATE_AR3,0x1234},{r::STATE_A,0xbeef}});
		initial.data = {{0x70,0xbeef},{0x1000,0xbeef}};
		initial.expected.push_back({r::STATE_ST0,0xa600});
		auto final = observation("producer-compared", {{r::STATE_ST0,0xb600},{r::STATE_ST1,0x800},{r::STATE_AR3,0xbeef}});
		clocked_register_case c{{{0,std::move(initial)},{access + 2,std::move(final)}},access + replay,access + 6};
		c.require_idle = false; c.check_pipeline_clock = true;
		c.data_events = std::vector<clocked_data_event>{};
		if (producer == 1 || producer == 2) c.data_events->push_back({false,producer == 1 ? 0x70U : 0x1000U,0xbeef,0,false});
		c.data_events->push_back({true,0x13,0xbeef,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
