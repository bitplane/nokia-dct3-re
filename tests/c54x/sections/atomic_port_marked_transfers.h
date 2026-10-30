// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	constexpr u16 opcodes[]{0x7213,0x720b,0x708a,0x7083,0x7092,0x7090,0x7088,0x7194,0x7184};
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != std::size(opcodes); ++variant) {
		bool const mvdk = variant >= 7;
		std::vector<u16> words = mvdk ? std::vector<u16>{0x75d6,0x124} : std::vector<u16>{0x75f8,0x100,0x124};
		words.insert(words.end(),{opcodes[variant],u16(mvdk ? 0x122 : 0x120)});
		if (mvdk) words.insert(words.end(),{0x75d6,0x124});
		else words.insert(words.end(),{0x75f8,0x100,0x124});
		words.push_back(0xf4e1);
		auto initial = initial_case("atomic-port-marked-MVDM-MVKD-MVDK",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1+i].value = words[i];
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0x11223344},{r::STATE_B,0xff1234abcdULL},
			{r::STATE_ST0,mvdk ? 0xaa5U : 0x1800U},{r::STATE_ST1,0x100},{r::STATE_AR0,0x123},
			{r::STATE_AR2,0x123},{r::STATE_AR3,0x123},{r::STATE_AR4,0x121},{r::STATE_AR6,0x103},{r::STATE_BK,4}});
		initial.data = {{0x100,0x1111},{0x103,0x2222},{0x120,u16(variant == 0 ? 0x1234 : variant == 1 ? 0xcafe : mvdk ? 0xbeef : 0x5678)},
			{0x121,0xabcd},{0x122,0},{0x123,u16(mvdk ? 0xcafe : 0)}};
		auto final = observation("transfer-result-neighbors-status-and-addressing",{{STATE_GENPC,0x601+words.size()},
			{r::STATE_A,0x11223344},{r::STATE_B,variant == 1 ? 0xff1234cafeULL : 0xff1234abcdULL},
			{r::STATE_ST0,mvdk ? 0xaa5U : 0x1800U},{r::STATE_ST1,0x100},
			{r::STATE_AR0,variant == 5 ? 0x124U : variant == 6 ? 0x122U : 0x123U},
			{r::STATE_AR2,variant == 2 ? 0x122U : variant == 4 ? 0x124U : 0x123U},
			{r::STATE_AR3,variant == 0 ? 0x1234U : 0x123U},{r::STATE_AR4,variant == 7 ? 0x122U : 0x121U},
			{r::STATE_AR6,mvdk ? 0x101U : 0x103U}});
		final.expected_data = initial.data;
		if (mvdk) final.expected_data[4].value = 0xabcd;
		else if (variant >= 2) final.expected_data[5].value = 0x5678;
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-transfer-markers",{{STATE_GENPC,0x601}})},
			{2,observation("transfer-markers-running",{})},{48,std::move(final)}},3,48};
		c.pipeline_frontend = false; c.observed_data_space = AS_IO;
		c.data_events = mvdk ? std::vector<clocked_data_event>{{true,0x124,0x2222,0,false},{true,0x124,0x1111,0,false,{},4}} :
			std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},{true,0x124,0x1111,0,false,{},5}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
