// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned returning = 0; returning < 2; ++returning) {
		u32 const origin = returning ? 0x30600 : 0x10600;
		unsigned const checkpoint = returning ? 6 : 4;
		u32 const paused = origin + (returning ? 2 : 3);
		auto initial = initial_case("atomic-pending-far-transfer-replay", {0xf495});
		initial.program = {{origin - 1,0xf495},{origin,u16(returning ? 0xf6e4 : 0xfb82)},
			{0x20700,0xe807},{0x20701,0xf4e4},{0x700,0xe80f},{0x701,0xf4e4},{0x10700,0xe80e},{0x10701,0xf4e4}};
		unsigned cursor = origin + 1;
		if (!returning) initial.program.push_back({cursor++,0x700});
		initial.program.push_back({cursor++,0xe801}); initial.program.push_back({cursor++,0xe905}); initial.program.push_back({cursor,0xf4e1});
		if (returning) { initial.program.push_back({0x10604,0xe807}); initial.program.push_back({0x10605,0xf4e1}); }
		initial.data = {{0x3fe,1},{0x3ff,0x604}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,origin - 1},{r::STATE_SP,returning ? 0x3feU : 0x400U},
			{r::STATE_ST0,0},{r::STATE_ST1,0x800}});
		auto final = observation("pending-transfer-completes-on-original-page", {{STATE_GENPC,returning ? 0x10606U : 0x10605U},
			{r::STATE_A,7},{r::STATE_B,5},{r::STATE_SP,0x400}},{{0x3fe,1},{0x3ff,0x604}});
		clocked_register_case c{{{0,std::move(initial)},
			{checkpoint,observation("exactly-one-delay-word-retired", {{STATE_GENPC,paused},{r::STATE_A,1},{r::STATE_B,0},
				{r::STATE_SP,returning ? 0x400U : 0x3feU}},{{0x3fe,1},{0x3ff,0x604}})},
			{checkpoint + 1,observation("remaining-delay-running", {})},{48,std::move(final)}},checkpoint,48};
		c.pipeline_frontend = false;
		// Poison completed CPU/stack state before automatic restore of the pending transfer.
		c.actions = {{48,{{STATE_GENPC,0x10900},{r::STATE_SP,0x500},{r::STATE_A,99},{r::STATE_B,88}},false,{},{},{{0x3fe,0x7f},{0x3ff,0xbad}}}};
		c.data_events = std::vector<clocked_data_event>{};
		if (!returning) { c.data_events->push_back({true,0x3ff,0x604,0,false}); c.data_events->push_back({true,0x3fe,1,0,false}); }
		c.data_events->push_back({false,0x3fe,1,0,false}); c.data_events->push_back({false,0x3ff,0x604,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
