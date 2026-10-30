// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form = 0; form < 8; ++form) for (unsigned pattern = 0; pattern < 4; ++pattern)
	for (unsigned mask_index = 0; mask_index < 4; ++mask_index) for (unsigned after = 0; after < 2; ++after) {
		u16 const mode = std::array<u16,8>{5,5,0x83,0x93,0x8b,0xdb,0xeb,0xf8}[form];
		u16 const address = std::array<u16,8>{0x1505,0x1505,0x1500,0x1500,0x1500,0x1503,0x1502,0x1507}[form];
		u16 const next = std::array<u16,8>{0x1500,0x1500,0x1500,0x1501,0x14ff,0x1501,0x1502,0x1500}[form];
		u16 const source = std::array<u16,4>{0,0xffff,0xff,0x8000}[pattern];
		u16 const mask = std::array<u16,4>{0,0xffff,0x5555,0x8001}[mask_index];
		unsigned const flags = (form + pattern + mask_index) % 4, words = form >= 6 ? 3 : 2;
		u16 const st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0) | (form == 1 ? 0x4000 : 0);
		auto initial = initial_case("atomic-memory-XOR", {u16(0x6a00 | mode),form == 7 ? address : mask,
			u16(form >= 6 ? form == 7 ? mask : 2 : 0xf4e1),0xf4e1});
		initial.data = {{address,source}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,0x123456789ULL},{r::STATE_B,0xff76543210ULL},
			{r::STATE_T,0x1357},{r::STATE_AR0,2},{r::STATE_AR3,form == 5 ? 0x1503U : 0x1500U},
			{r::STATE_SP,form == 1 ? 0x1500U : 0x1000U},{r::STATE_BK,4},{r::STATE_ST0,0x1e2a},{r::STATE_ST1,st1}});
		auto final = observation("single-resolved-transaction", {{STATE_GENPC,0x601 + words},{r::STATE_AR3,next},
			{r::STATE_AR0,2},{r::STATE_SP,form == 1 ? 0x1500U : 0x1000U},{r::STATE_A,0x123456789ULL},
			{r::STATE_B,0xff76543210ULL},{r::STATE_T,0x1357},{r::STATE_ST0,0x1e2a},{r::STATE_ST1,st1}},{{address,u16(source ^ mask)}});
		clocked_register_case c{{{0,std::move(initial)},{32,std::move(final)}},after ? 12U : 1U,32};
		c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{{false,address,source,0,false},{true,address,u16(source ^ mask),0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
