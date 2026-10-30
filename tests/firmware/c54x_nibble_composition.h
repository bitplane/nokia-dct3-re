// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (u16 input : std::array<u16,8>{0,0xffff,0xc410,0x1234,0x8001,0xabcd,0x00f0,0xf00f})
		for (unsigned index = 0; index != 4; ++index) for (unsigned sxm = 0; sxm != 2; ++sxm) {
			u32 const base = (index+sxm)%2 ? 0x20000 : 0;
			auto initial = initial_case("relocated-nibble-helper-composition",{0xf020,0x2000,u16(0xf980 | (base >> 16)),0x700,0xf4e1});
			for (auto &word : initial.program) word.address += base;
			constexpr std::array<u16,23> helper{0x4a11,0xeeff,0xf495,0x8000,0xe803,0xf6b8,0x0804,0x8004,0x7100,0x0011,
				0x6f04,0x0c82,0x1004,0xf484,0x880e,0xee01,0x1481,0x8a11,0xf030,0x000f,0xf495,0xf495,0xf4e4};
			for (unsigned i = 0; i != helper.size(); ++i) initial.program.push_back({base+0x700+i,helper[i]});
			initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,0},{r::STATE_B,0},
				{r::STATE_ST0,0},{r::STATE_ST1,0x4800U | (sxm ? 0x100U : 0U)},{r::STATE_SP,0x1400},{r::STATE_AR1,0x1357}});
			initial.data = {{0x1400,u16(index)},{0x2000,input}};
			clocked_register_case c{{{0,std::move(initial)},{1,observation("nibble-helper-running",{})},
				{64,observation("selected-nibble-and-restored-frame",{{r::STATE_A,(input >> (4*(3-index))) & 15U},
					{r::STATE_SP,0x1400},{r::STATE_AR1,0x1357}})}},14,64};
			// Preserve the original selected-bus contract, not an invented full frame trace.
			c.data_ranges = {{0x1400,0x1400},{0x2000,0x2000}};
			c.data_events = std::vector<clocked_data_event>{{false,0x1400,u16(index),0,false},
				{true,0x1400,u16(3-index),0,false},{true,0x1400,u16(4*(3-index)),0,false},{false,0x2000,input,0,false}};
			cases.push_back(std::move(c));
		}
	return cases;
}
}
