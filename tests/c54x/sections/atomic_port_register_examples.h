// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	constexpr u16 opcodes[]{0x8814,0x8813,0x8811,0x8912};
	constexpr unsigned destinations[]{4,3,1,2};
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 6; ++variant) {
		bool const repeat = variant == 4, load = variant == 5;
		std::vector<u16> words{0x75f8,0x100,0x124};
		if (repeat) words.insert(words.end(),{0xec0c,0x6d10,0xe80f});
		else if (load) words.insert(words.end(),{0x13f8,0x120});
		else words.push_back(opcodes[variant]);
		words.insert(words.end(),{0x75f8,0x100,0x124,0xf4e1});
		auto initial = initial_case("atomic-port-marked-register-store-repeat-and-LDU",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1+i].value = words[i];
		u64 const a = repeat ? 0 : load ? 0x12345678 : 0x1234beef;
		u64 const b = repeat || load ? 0 : 0x5678cafe;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_RC,0},{r::STATE_ST0,0},{r::STATE_ST1,repeat ? 0U : 0x100U}});
		for (unsigned ar = 0; ar != 8; ++ar) initial.initial.push_back({int(r::STATE_AR0+ar),0});
		initial.data = {{0x100,0x1111},{0x120,0x8001}};
		auto final = observation("selected-register-or-repeat-result-and-port-interval",{{STATE_GENPC,0x601+words.size()},
			{r::STATE_A,repeat ? 15 : a},{r::STATE_B,load ? 0x8001 : b},{r::STATE_RC,0}});
		for (unsigned ar = 0; ar != 8; ++ar) final.expected.push_back({int(r::STATE_AR0+ar),
			repeat && !ar ? 13U : !repeat && !load && ar == destinations[variant] ? variant == 3 ? 0xcafeU : 0xbeefU : 0U});
		final.expected_data = initial.data;
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-register-markers",{{STATE_GENPC,0x601}})},
			{2,observation("register-markers-running",{})},{64,std::move(final)}},3,64};
		c.pipeline_frontend = false; c.observed_data_space = AS_IO;
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},{true,0x124,0x1111,0,false,{},repeat ? 18U : load ? 5U : 4U}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
