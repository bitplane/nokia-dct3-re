// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 576; ++index) {
		unsigned v = index, condition = v % 4; v /= 4;
		bool const delayed = v % 2; v /= 2;
		unsigned const flags = v % 4; v /= 4;
		unsigned const producer = v % 3; v /= 3;
		unsigned const gap = v % 3; v /= 3;
		bool const mutate = v;
		unsigned const selected = condition & 1;
		u16 const mask = 1U << (10 - selected), before = 0x1805 | ((flags & 1) << 10) | ((flags & 2) << 8);
		u16 const sampled = producer == 0 ? before : producer == 1 ? before | mask : before & ~mask;
		bool const taken = bool(sampled & mask) == bool(condition & 2), slots = delayed || !taken;
		u16 const status = (sampled & ~mask) | (slots || mutate ? mask : 0);
		u32 const base = (condition + flags + producer + gap) & 1 ? 0x20000 : 0;
		u16 const opcode = 0xf860 | (delayed ? 0x200 : 0) | (selected << 3) | ((condition & 2) << 3);
		auto initial = initial_case("sticky-overflow-branch", {u16(producer == 0 ? 0xf495 : (producer == 1 ? 0xf5b0 : 0xf4b0) | (10 - selected))});
		for (auto &word : initial.program) word.address += base;
		initial.program[1 + gap].value = opcode; initial.program[2 + gap].value = 0x700;
		initial.program[3 + gap].value = 0xf5b0 | (10 - selected); initial.program[4 + gap].value = 0xe955;
		initial.program[5 + gap].value = 0xe811; initial.program[6 + gap].value = 0xf4e1;
		initial.program.insert(initial.program.end(),{{base + 0x700,0xe822},{base + 0x701,0xf4e1}});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,before},{r::STATE_SP,0x3000}});
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,4>{3,6 + gap,7 + gap,10 + gap}[(condition + flags + producer + gap) % 4],24};
		c.states.push_back({7 + gap,observation("selected-sticky-flag-consumed", {{r::STATE_ST0,u64(sampled & ~mask)}})});
		c.states.push_back({8 + gap,observation("decision-survives-flag-mutation", {})});
		c.states.push_back({24,observation("selected-path-and-independent-delay", {{STATE_GENPC,base + (taken ? 0x702 : 0x607 + gap)},
			{r::STATE_A,taken ? 0x22U : 0x11U},{r::STATE_B,slots ? 0x55U : 0U},{r::STATE_ST0,status},{r::STATE_SP,0x3000}})});
		if (mutate) c.actions = {{7 + gap,{{r::STATE_ST0,u64(sampled | mask)}},false}};
		c.program_events = {{false,base + 0x602 + gap,0x700,0,false}};
		if (taken) c.program_events.push_back({false,base + 0x700,0xe822,0,false});
		c.forbid_data_access = true; c.clear_pipeline_before_restore = true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
