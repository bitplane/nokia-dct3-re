// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned destination = 0; destination < 2; ++destination)
	for (u16 value : {0U,1U,0x5a5aU,0xffffU}) for (u16 pending : {0U,4U,0xa5a5U,0xffffU})
	for (unsigned point : {4U,5U,6U})
	{
		auto initial = initial_case("irq-register-pop", {u16(0x8a00 | destination)});
		initial.initial.insert(initial.initial.end(), {{r::STATE_IMR,0x1357},{r::STATE_IFR,pending}});
		initial.expected.insert(initial.expected.end(), {{r::STATE_IMR,0x1357},{r::STATE_IFR,pending},
			{r::STATE_SP,0x1000},{r::STATE_ST0,0xe},{r::STATE_ST1,0x800}});
		initial.data = {{0x1000,value}};
		auto read = observation("stack-read", {}); read.expected = initial.expected; read.expected[3].value = 0x1001;
		auto written = read;
		written.expected[destination ? 2 : 1].value = destination ? u16(pending & ~value) : value;
		clocked_register_case c{{{0,std::move(initial)},{5,std::move(read)},{6,std::move(written)}},point,10};
		c.require_idle = false; c.check_pipeline_clock = true;
		clocked_register_action poison{5,{}}; poison.data = {{0x1000,0xdead}};
		c.actions.push_back(std::move(poison));
		c.data_events = std::vector<clocked_data_event>{{false,0x1000,value,5},{true,destination,value,6}};
		cases.push_back(std::move(c));
	}
	for (unsigned conflict = 0; conflict < 2; ++conflict) for (unsigned point : {5U,6U,7U})
	{
		auto initial = initial_case("pop-bank-conflict", {u16(conflict ? 0x8011 : 0x8070),0x8a12});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0},{r::STATE_A,0x7777},
			{r::STATE_AR1,0x1111},{r::STATE_AR2,0x1234}});
		initial.data = {{0x1000,0xbeef}};
		auto final = observation("committed", {{r::STATE_SP,0x1001},{r::STATE_AR2,0xbeef},
			{r::STATE_AR1,conflict ? 0x7777U : 0x1111U},{r::STATE_ST0,0},{r::STATE_ST1,0x800}});
		clocked_register_case c{{{0,std::move(initial)},{10,std::move(final)}},point,10};
		c.require_idle = false; c.check_pipeline_clock = true;
		clocked_register_action poison{6,{}}; poison.data = {{0x1000,0xdead}};
		c.actions.push_back(std::move(poison));
		c.data_events = std::vector<clocked_data_event>{{false,0x1000,0xbeef,6}};
		// Early DAGEN publication precedes same-clock RAM Execute in this model.
		if (!conflict) c.data_events->push_back({true,0x12,0xbeef,6});
		c.data_events->push_back({true,conflict ? 0x11U : 0x70U,0x7777,6});
		if (conflict) c.data_events->push_back({true,0x12,0xbeef,7});
		cases.push_back(std::move(c));
	}
	// Active-repeat POPM BRC/RSA/REA is a staged-model admission limit.
	for (unsigned target = 0; target < 3; ++target) for (unsigned point : {3U,9U})
	{
		auto initial = initial_case("active-repeat-pop-limit", {0xf072,0x603,u16(0x8a1a + target),0xf495,0xf4e1});
		initial.initial.insert(initial.initial.end(), {{r::STATE_BRC,1},{r::STATE_ST0,0}});
		initial.expected.clear();
		auto rejected = observation("rejected", {{r::STATE_ILLEGAL,1},{r::STATE_SP,0x1000},
			{r::STATE_BRC,1},{r::STATE_RSA,0x602},{r::STATE_REA,0x603}});
		rejected.expected.pop_back();
		clocked_register_case c{{{0,std::move(initial)},{16,std::move(rejected)}},point,16};
		c.require_idle = false; c.forbid_data_access = true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
