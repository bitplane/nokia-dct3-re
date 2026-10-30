// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned repeat = 0; repeat != 2; ++repeat)
	for (unsigned to_mmr = 0; to_mmr != 2; ++to_mmr) {
		u16 const address = to_mmr ? 0xe00 : repeat ? 0xe10 : 0xe01;
		auto initial = initial_case(to_mmr ? "MVDM-DAR-source-progression" : "MVMD-DAR-destination-progression", {0xf495});
		unsigned at = 0;
		if (repeat) initial.program[at++].value = 0xec02;
		initial.program[at++].value = to_mmr ? 0x7212 : 0x7312;
		initial.program[at++].value = address; initial.program[at].value = 0xf4e1;
		initial.program.push_back({0x5ff,0xf495});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_AR2,to_mmr ? 0U : repeat ? 0x4455U : 0x1234U}});
		auto final = observation("move-completes-with-owned-pointer-and-count", {{STATE_GENPC,0x601 + at},
			{r::STATE_AR2,to_mmr ? repeat ? 0x3333U : 0x1234U : repeat ? 0x4455U : 0x1234U}});
		if (repeat) final.expected.push_back({r::STATE_RC,0});
		clocked_register_case c{{},1,48}; c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{};
		for (unsigned i = 0; i != (repeat ? 3U : 1U); ++i) {
			u16 const word = to_mmr ? repeat ? 0x1111 * (i + 1) : 0x1234 : repeat ? 0x4455 : 0x1234;
			initial.data.push_back({address + i,to_mmr ? word : u16(0)});
			final.expected_data.push_back({address + i,word});
			c.data_events->push_back({false,to_mmr ? address + i : 0x12,word,0,false});
			c.data_events->push_back({true,to_mmr ? 0x12 : address + i,word,0,false});
		}
		c.states = {{0,std::move(initial)},{1,observation("pre-move-snapshot", {{STATE_GENPC,0x600}})},
			{2,observation("move-running", {})},{48,std::move(final)}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
