// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned layout = 0; layout < 6; ++layout)
	for (unsigned tc = 0; tc < 2; ++tc)
	for (unsigned condition = 0; condition < 2; ++condition)
	for (unsigned span = 0; span < (layout == 1 ? 1U : 2U); ++span)
	{
		bool const producer = layout >= 2, multiword = layout == 1;
		unsigned const gap = producer ? layout - 2 : 0, offset = producer ? 1 + gap : 0;
		bool const sampled = producer && gap < 2 ? tc : !tc;
		bool const execute = sampled == condition;
		auto initial = initial_case("xc-condition", {0xf495});
		if (producer) initial.program[0].value = tc ? 0xf4bc : 0xf5bc;
		initial.program[offset].value = 0xfd20 | ((multiword || span) << 9) | (condition << 4);
		initial.program[offset + 1].value = multiword ? 0x7600 : 0x8000;
		initial.program[offset + 2].value = multiword ? 0xfd30 : 0x8001;
		if (multiword) initial.program[3].value = 0x8001;
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0xa60e | (tc << 12)},{r::STATE_A,0x1234}});
		unsigned const point = producer ? 4 + gap + ((tc + condition + span) & 1) :
			std::array<unsigned,4>{3,4,6,8}[(tc + 2 * condition + span + layout) % 4];
		clocked_register_case c{{{0,std::move(initial)}},point,12};
		if (!producer) c.actions = {{3,{{r::STATE_ST0,0xa60e | ((!tc) << 12)}}},
			{4,{{r::STATE_ST0,0xa60e | (tc << 12)}}}};
		c.require_idle = false; c.check_pipeline_clock = true;
		c.data_events = std::vector<clocked_data_event>{};
		if (execute) c.data_events->push_back({true,0x700,u16(multiword ? 0xfd30 : 0x1234),(multiword ? 8U : 7U) + offset});
		if (execute || multiword || !span) c.data_events->push_back({true,0x701,0x1234,(multiword ? 9U : 8U) + offset});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
