// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 128; ++index) {
		bool const b = index & 16, decrement = index & 32, sst = index & 64;
		u64 const source = b ? 0xff87654321ULL : 0x12345678, shifted = (source << (index & 15)) & 0xffffffffffULL;
		s64 const signed_value = shifted & 0x8000000000ULL ? s64(shifted) - (s64(1) << 40) : s64(shifted);
		u16 const word = sst && signed_value > 2147483647LL ? 0x7fff : sst && signed_value < -2147483648LL ? 0x8000 : u16(shifted >> 16);
		auto initial = initial_case("shifted-high-store", {u16(0x9a00 | (b ? 0x100 : 0) | ((decrement ? 5 : 1) << 4) | (index & 15)),0xf4e1});
		initial.data = {{0x507,0xbeef},{0x508,0xabcd}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,0x12345678},{r::STATE_B,0xff87654321ULL},
			{r::STATE_AR3,0x507},{r::STATE_ST0,0xaa5},{r::STATE_ST1,0x900},{r::STATE_PMST,sst ? 1U : 0U}});
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 3U : 1U,24};
		c.pipeline_frontend = false;
		c.states.push_back({24,observation("shift-before-store-saturation", {{r::STATE_A,0x12345678},{r::STATE_B,0xff87654321ULL},
			{r::STATE_AR3,decrement ? 0x506U : 0x507U},{r::STATE_ST0,0xaa5},{r::STATE_ST1,0x900},
			{r::STATE_PMST,sst ? 1U : 0U},{STATE_GENPC,0x602}},{{0x507,word},{0x508,0xabcd}})});
		c.data_events = std::vector<clocked_data_event>{{true,0x507,word,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
