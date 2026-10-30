// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned low = 0; low != 2; ++low) for (unsigned delayed = 0; delayed != 2; ++delayed)
		for (unsigned level = 0; level != 2; ++level) for (unsigned transition = 0; transition != 4; ++transition) {
			bool const sampled = transition == 1 || transition == 2 ? !level : bool(level);
			bool const taken = low ? !sampled : sampled;
			u32 const base = (low ^ level) ? 0x20000 : 0;
			auto initial = initial_case("pipeline-BIO-sampled-once-before-or-after-pin-change",{u16(0xf802 | low | (delayed << 9)),0x700,0xe811,0xe922,0xf4e1});
			for (auto &word : initial.program) word.address += base;
			initial.program.insert(initial.program.end(),{{base+0x700,0xe855},{base+0x701,0xf4e1}});
			initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,0},{r::STATE_B,0},
				{r::STATE_ST0,0xa608},{r::STATE_ST1,0x800}});
			auto final = observation("sampled-condition-selects-real-plain-or-delayed-path",{{r::STATE_A,taken ? 0x55U : 0x11U},
				{r::STATE_B,delayed || !taken ? 0x22U : 0U},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x800}});
			unsigned const checkpoint = std::array<unsigned,4>{4,5,6,8}[(low+2*delayed+level+transition)%4];
			clocked_register_case c{{{0,std::move(initial)},{1,observation("pipeline-branch-running",{})},{20,std::move(final)}},checkpoint,20};
			c.initial_bio = level; c.bio_samples = std::vector<clocked_line_sample>{{6,sampled}};
			c.forbid_data_access = true; // The BIO sample is clock6; the counter stops once IDLE retires.
			c.program_events = {{false,base+0x601,0x700,0,false}};
			if (taken) c.program_events.push_back({false,base+0x700,0xe855,0,false});
			if (transition) {
				c.actions = {{transition+3,{}}}; c.actions.back().bio_level = !level;
			}
			cases.push_back(std::move(c));
		}
	return cases;
}
}
