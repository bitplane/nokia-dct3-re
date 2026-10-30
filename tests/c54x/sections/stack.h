// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
#define C54X_SECTION_OBSERVE_STORE
#define C54X_SECTION_XF
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	// Literal MMR values, not values read back from the CPU as an oracle.
	constexpr u16 source_values[]{0xe,0x800,0x789a,0x3456,0x12,0x5566,0x3344,0x22,
		0x5aa5,0x1357,0x5000,0x5001,0x5002,0x5003,0x5004,0x5005,0x5006,0x5007,0x1000,16};
	for (unsigned source = 0; source <= 0x1e; ++source)
	{
		if (source >= 2 && source <= 5) continue; // Reserved gaps are external RAM, not CPU registers.
		for (unsigned page = 0; page < (source == 0x1e ? 2U : 1U); ++page)
		for (unsigned point : {4U,5U,6U})
		{
			u16 const value = source <= 1 ? 0 : source == 0x1a ? 0x2468 : source == 0x1b ? 0x4567 :
				source == 0x1c ? 0x5678 : source == 0x1d ? 0 : source == 0x1e ? page : source_values[source - 6];
			auto initial = initial_case("push", {u16(0x4a00 | source)});
			for (auto &word : initial.program) word.address += page * 0x10000;
			initial.initial.insert(initial.initial.end(), {{STATE_GENPC,page * 0x10000 + 0x600},
				{r::STATE_A,0x123456789aULL},{r::STATE_B,0x2233445566ULL},
				{r::STATE_T,0x5aa5},{r::STATE_TRN,0x1357},{r::STATE_BK,16},
				{r::STATE_BRC,0x2468},{r::STATE_RSA,0x4567},{r::STATE_REA,0x5678}});
			for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({r::STATE_AR0 + ar,0x5000 + ar});
			initial.data = {{0xfff,0xaaaa}};
			clocked_register_case c{{{0,std::move(initial)},
				{5,observation("allocate", {{r::STATE_SP,0xfff}})},
				{6,observation("push", {{r::STATE_SP,0xfff}}, {{0xfff,value}})}},point,10};
			c.require_idle = false; c.check_pipeline_clock = true;
			c.data_events = std::vector<clocked_data_event>{{false,source,value,5},{true,0xfff,value,6}};
			cases.push_back(std::move(c));
		}
	}
	for (unsigned destination = 6; destination <= 0x19; ++destination)
	{
		if (destination == 0x18) continue; // POPM SP has its own alias/ordering contract.
		for (unsigned point : {4U,5U,6U})
		{
			auto initial = initial_case("pop", {u16(0x8a00 | destination)});
			initial.data = {{0x1000,0x6aa5}};
			unsigned const write = destination >= 0x10 ? 5 : 6;
			u16 const value = destination == 0xa || destination == 0xd ? 0xa5 : 0x6aa5;
			clocked_register_case c{{{0,std::move(initial)},
				{5,observation("pop-read", {{r::STATE_SP,0x1001}})},
				{6,observation("pop-write", {{r::STATE_SP,0x1001}}, {{destination,value}})}},point,10};
			c.require_idle = false; c.check_pipeline_clock = true;
			c.data_events = std::vector<clocked_data_event>{{false,0x1000,0x6aa5,5},{true,destination,0x6aa5,write}};
			cases.push_back(std::move(c));
		}
	}
	return cases;
}
}
