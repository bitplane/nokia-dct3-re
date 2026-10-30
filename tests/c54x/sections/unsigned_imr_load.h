// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form = 0; form != 4; ++form) for (unsigned pattern = 0; pattern != 4; ++pattern)
		for (unsigned sxm = 0; sxm != 2; ++sxm) for (unsigned b = 0; b != 2; ++b) {
			u16 const value = std::array<u16,4>{0,1,0x8000,0xffff}[pattern], st1 = sxm ? 0x900 : 0x800;
			unsigned const prefix = form == 3 ? 2 : 0, read = form == 3 ? 7 : form == 1 ? 6 : 5;
			u32 const base = (form+pattern+sxm+b)%2 ? 0x20000 : 0;
			u16 const opcode = 0x1200 | (b << 8) | (form == 1 ? 0xf8 : form >= 2 ? 0x82 : 0);
			auto initial = initial_case("unsigned-IMR-load-retains-mapped-word-and-pointer",{u16(prefix ? 0x7182 : opcode)});
			if (prefix) initial.program[1].value = 0x12;
			initial.program[prefix].value = opcode;
			if (form == 1) initial.program[1].value = 0;
			initial.program[prefix+1+(form == 1)].value = 0xf4e1;
			for (auto &word : initial.program) word.address += base;
			initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,0xabcdef1234ULL},{r::STATE_B,0x123456789aULL},
				{r::STATE_IMR,value},{r::STATE_ST0,0x1600},{r::STATE_ST1,st1},{r::STATE_AR2,prefix ? 0x1400U : 0U}});
			initial.data = {{0x1400,0}};
			auto final = observation("zero-extended-captured-IMR-not-new-mask-or-pointer",{{r::STATE_A,b ? 0xabcdef1234ULL : value},
				{r::STATE_B,b ? value : 0x123456789aULL},{r::STATE_IMR,u16(value ^ 0xffff)},
				{r::STATE_ST0,0x1600},{r::STATE_ST1,st1},{r::STATE_AR2,form >= 2 ? 0x2222U : 0U}});
			clocked_register_case c{{{0,std::move(initial)},{1,observation("mapped-load-running",{})},
				{read-1,observation("zero-pointer-before-later-mutation",{{r::STATE_AR2,0}})},
				{read,observation("IMR-sampled",{})},{read+1,std::move(final)}},
				std::array<unsigned,3>{3,read-1,read}[(form+pattern+sxm+b)%3],18};
			if (form == 1 || prefix) c.actions.push_back({4,{},false,{{base+0x601,0xdead}}});
			if (form >= 2) c.actions.push_back({read-1,{{r::STATE_AR2,0x2222}}});
			c.actions.push_back({read,{{r::STATE_IMR,u16(value ^ 0xffff)}}});
			c.data_events = std::vector<clocked_data_event>{};
			if (prefix) c.data_events->insert(c.data_events->end(),{{false,0x1400,0,5},{true,0x12,0,5}});
			c.data_events->push_back({false,0,value,read});
			if (form == 1 || prefix) c.retained_program_reads = {{base+0x601,u16(prefix ? 0x12 : 0)}};
			cases.push_back(std::move(c));
		}
	return cases;
}
}
