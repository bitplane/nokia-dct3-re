// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_XF
#define C54X_SECTION_XF_RESTORE
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	struct row { u16 opcode,st0,st1,out0,out1; };
	constexpr row rows[]{
		{0xf6bd,0x800,0x2000,0x800,0},{0xf6bf,0x800,0x8000,0x800,0},
		{0xf7bd,0x800,0,0x800,0x2000},{0xf7be,0x800,0,0x800,0x4000},
		{0xf5bc,0x800,0x100,0x1800,0x100},{0xf4bc,0x1800,0x100,0x800,0x100},
		{0xf6b8,0x1800,0x300,0x1800,0x200},{0xf7b8,0x1800,0x200,0x1800,0x300}
	};
	std::vector<clocked_register_case> cases;
	for (auto const &v : rows) {
		auto initial = initial_case("RSBX-SSBX-status-and-XF-publication", {0xf495,v.opcode,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_ST0,v.st0},{r::STATE_ST1,v.st1}});
		auto final = observation("only-selected-status-bit-and-XF-event-change", {{STATE_GENPC,0x603},{r::STATE_ST0,v.out0},{r::STATE_ST1,v.out1}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-status-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("status-running", {})},{24,std::move(final)}},1,24};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{}; c.xf_events = std::vector<bool>{};
		if ((v.st1 ^ v.out1) & 0x2000) c.xf_events->push_back(bool(v.out1 & 0x2000));
		cases.push_back(std::move(c));
	}
	return cases;
}
}
