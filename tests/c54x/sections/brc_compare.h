// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	// SPRU172C 4-33: absolute CMPM samples Smem once and changes only TC.
	// BRC is an ordinary readable MMR (SPRU131G CPU register map).
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 8; ++index) {
		u16 const value = std::array<u16,4>{0,1,0x8000,0xffff}[index / 2];
		bool const equal = index & 1;
		u16 const literal = equal ? value : u16(value ^ 1);
		auto initial = initial_case("absolute-CMPM-BRC", {0x60f8,0x001a,literal,0xf4e1});
		initial.initial.insert(initial.initial.end(), {{r::STATE_BRC,0x1234},
			{r::STATE_ST0,u16(0xa600 | (!equal << 12))},{r::STATE_A,0x12345678},
			{r::STATE_B,0x87654321},{r::STATE_ST1,0x0800}});
		clocked_register_case c{{{0,std::move(initial)},
			{7,observation("Read-retains-BRC-without-publishing-TC", {{r::STATE_ST0,u16(0xa600 | (!equal << 12))}})},
			{8,observation("Execute-publishes-only-TC", {{r::STATE_ST0,u16(0xa600 | (equal << 12))}})},
			{20,observation("later-BRC-change-cannot-change-comparison", {{r::STATE_ST0,u16(0xa600 | (equal << 12))},
				{r::STATE_BRC,u16(value ^ 0xffff)},{r::STATE_A,0x12345678},{r::STATE_B,0x87654321},
				{r::STATE_ST1,0x0800},{STATE_GENPC,0x604}})}},6 + index % 3,20};
		c.actions = {{6,{{r::STATE_BRC,value}}},
			{7,{{r::STATE_BRC,u16(value ^ 0xffff)}},false,{{0x601,0xdead},{0x602,0xdead}}}};
		c.data_events = std::vector<clocked_data_event>{{false,0x001a,value,7}};
		c.program_events = {{false,0x601,0x001a,0,false},{false,0x602,literal,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
