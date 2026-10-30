// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u64 results[] = {0x80000000,0x7fffffff,0x7fffffff,0x7ffffffe,0x7fffffff,0x7fffffff};
	for (unsigned variant = 0; variant != 6; ++variant) {
		bool const fractional = variant == 2 || variant == 3;
		u16 const opcode = variant == 4 ? 0xb03a : variant == 5 ? 0xd6e1 : 0x2883;
		auto initial = initial_case("atomic-MAC-overflow-product-saturation-and-parallel-store",{0xf495,opcode,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,fractional ? 0xffffffffffULL : 0x7fffffff},
			{r::STATE_B,0x12340000},{r::STATE_T,fractional ? 0x8000U : 1U},
			{r::STATE_AR3,0x100},{r::STATE_AR4,0x110},{r::STATE_AR5,0x120},{r::STATE_AR0,0},{r::STATE_BK,0},
			{r::STATE_ST0,variant < 2 ? 0x800U : 0U},{r::STATE_ST1,variant == 0 ? 0U : fractional ? 0x240U : 0x200U},
			{r::STATE_PMST,variant == 3 ? 2U : 0U}});
		initial.data = {{0x100,u16(fractional ? 0x8000 : 1)},{0x110,1},{0x120,1}};
		auto final = observation("MAC-fixed-numeric-result-and-flag-ownership",{{STATE_GENPC,0x603},
			{r::STATE_A,results[variant]},{r::STATE_B,0x12340000},
			{r::STATE_ST0,variant < 2 ? 0xc00U : fractional ? 0U : 0x400U,0xc00},
			{r::STATE_AR3,0x100},{r::STATE_AR4,variant == 4 ? 0x111U : 0x110U},{r::STATE_AR5,0x120}});
		final.expected_data = {{0x100,u16(variant == 5 ? 0x1234 : fractional ? 0x8000 : 1)},{0x110,1},{0x120,1}};
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-MAC",{{STATE_GENPC,0x601}})},
			{2,observation("MAC-running",{})},{24,std::move(final)}},1,24};
		c.pipeline_frontend = false;
		if (variant == 4) c.data_events = std::vector<clocked_data_event>{{false,0x120,1,0,false},{false,0x110,1,0,false}};
		else if (variant == 5) c.data_events = std::vector<clocked_data_event>{{false,0x110,1,0,false},{true,0x100,0x1234,0,false}};
		else c.data_events = std::vector<clocked_data_event>{{false,0x100,u16(fractional ? 0x8000 : 1),0,false}};
		cases.push_back(std::move(c));
	}
	auto initial = initial_case("atomic-low-word-postincrement-store",{0xf495,0x8093,0xf4e1});
	initial.initial.insert(initial.initial.end(),{{r::STATE_A,0x12345678},{r::STATE_AR3,0x100},{r::STATE_ST1,0}});
	initial.data = {{0x100,0xbeef},{0x101,0xdead}};
	auto final = observation("low-word-and-pointer",{{STATE_GENPC,0x603},{r::STATE_A,0x12345678},{r::STATE_AR3,0x101}});
	final.expected_data = {{0x100,0x5678},{0x101,0xdead}};
	clocked_register_case c{{{0,std::move(initial)},{1,observation("before-store",{{STATE_GENPC,0x601}})},
		{2,observation("store-running",{})},{24,std::move(final)}},1,24};
	c.pipeline_frontend = false;
	c.data_events = std::vector<clocked_data_event>{{true,0x100,0x5678,0,false}};
	cases.push_back(std::move(c));
	return cases;
}
}
