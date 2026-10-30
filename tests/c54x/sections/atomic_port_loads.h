// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 6; ++variant) {
		bool const port_read = variant < 2;
		bool const circular = variant >= 4;
		bool const sign = variant >= 2 && !(variant & 1);
		std::vector<u16> words = port_read ? std::vector<u16>{0x75d6,0x124,0xf495,
			u16(variant ? 0x7483 : 0x7482),0x123,u16(variant ? 0x7483 : 0x7482),0x123,0xf4e1} :
			std::vector<u16>{0x75f8,0x100,0x124,u16(circular ? 0x56d3 : 0x5685),0x75f8,0x100,0x124,0xf4e1};
		auto initial = initial_case("atomic-stationary-port-or-doubleword-load",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1+i].value = words[i];
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,port_read ? 5U : 0U},{r::STATE_B,port_read ? 133U : 0x76543210U},
			{r::STATE_AR2,0x120},{r::STATE_AR3,port_read ? 0x122U : 0x124U},{r::STATE_AR5,0x120},{r::STATE_AR6,0x103},
			{r::STATE_BK,port_read ? 4U : 5U},{r::STATE_ST0,0xaa5},{r::STATE_ST1,port_read || sign ? 0x100U : 0U}});
		initial.data = port_read ? std::vector<memory_word>{{0x103,0x1234},{0x120,u16(variant ? 0xabcd : 0)},
			{0x121,0xbeef},{0x122,0},{0x123,0xcafe}} : std::vector<memory_word>{{0x100,0x1111},
			{0x120,0xff80},{0x121,0x1234},{0x124,0xff80},{0x125,0x1234}};
		auto final = observation("load-result-neighbors-status-and-independent-pointer",{{STATE_GENPC,0x609},
			{r::STATE_A,port_read ? 5ULL : sign ? 0xffff801234ULL : 0xff801234ULL},
			{r::STATE_B,port_read ? 133U : 0x76543210U},{r::STATE_AR2,0x120},
			{r::STATE_AR3,port_read ? 0x122U : circular ? 0x121U : 0x124U},{r::STATE_AR5,0x120},
			{r::STATE_ST0,0xaa5},{r::STATE_ST1,port_read || sign ? 0x100U : 0U}});
		final.expected_data = initial.data;
		if (port_read) final.expected_data[variant ? 3 : 1].value = 0xabcd;
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-load-markers",{{STATE_GENPC,0x601}})},
			{2,observation("load-markers-running",{})},{48,std::move(final)}},3,48};
		c.pipeline_frontend = false; c.observed_data_space = AS_IO; c.initial_io = {{0x123,0xabcd}};
		c.data_events = port_read ? std::vector<clocked_data_event>{{true,0x124,0x1234,0,false},
			{false,0x123,0xabcd,0,false},{false,0x123,0xabcd,0,false,{},2}} :
			std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},{true,0x124,0x1111,0,false,{},4}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
