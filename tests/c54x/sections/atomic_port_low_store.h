// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	constexpr u16 opcodes[]{0x81f8,0x8192,0x8190,0x8191,0x8090,0x8194};
	constexpr u16 destinations[]{0x120,0x122,0x124,0x126,0x124,0x128};
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != std::size(opcodes); ++variant) {
		std::vector<u16> words{0x75f8,0x100,0x124,opcodes[variant]};
		if (!variant) words.push_back(0x120);
		words.insert(words.end(),{0x75f8,0x100,0x124,0xf4e1});
		auto initial = initial_case("atomic-low-store-source-pointer-and-status-selection",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1+i].value = words[i];
		u16 const st0 = variant == 5 ? 0xa608 : 0x1800, st1 = variant == 5 ? 0x800 : 0x100;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0x12345678},{r::STATE_B,0x1234beef},
			{r::STATE_AR0,0x124},{r::STATE_AR1,0x126},{r::STATE_AR2,0x122},{r::STATE_AR4,0x128},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1}});
		initial.data = {{0x100,0x1111}};
		for (unsigned address = 0x120; address != 0x12a; ++address) initial.data.push_back({address,u16(address & 1 ? 0xcafe : 0)});
		auto final = observation("exact-destination-preserves-neighbors-accumulators-and-status",{{STATE_GENPC,0x601+words.size()},
			{r::STATE_A,0x12345678},{r::STATE_B,0x1234beef},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_AR0,variant == 2 || variant == 4 ? 0x125U : 0x124U},
			{r::STATE_AR1,variant == 3 ? 0x127U : 0x126U},{r::STATE_AR2,variant == 1 ? 0x123U : 0x122U},
			{r::STATE_AR4,variant == 5 ? 0x129U : 0x128U}});
		final.expected_data = initial.data;
		for (auto &word : final.expected_data) if (word.address == destinations[variant]) word.value = variant == 4 ? 0x5678 : 0xbeef;
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-low-store-markers",{{STATE_GENPC,0x601}})},
			{2,observation("low-store-markers-running",{})},{48,std::move(final)}},3,48};
		c.pipeline_frontend = false; c.observed_data_space = AS_IO;
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},{true,0x124,0x1111,0,false,{},variant ? 4U : 5U}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
