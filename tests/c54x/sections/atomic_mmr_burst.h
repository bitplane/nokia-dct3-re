// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	constexpr u16 mmrs[]{0x0e,0x10,0x11,0x13,0x14,0x15,0x16,0x17,0x19,0x1a,0x1b,0x1c};
	std::vector<clocked_register_case> cases;
	for (unsigned restore = 0; restore != 2; ++restore) {
		auto initial = initial_case(restore ? "MMR-burst-overwrite-pop-and-push" : "MMR-burst-initialize-and-push", {0xf495});
		initial.program.clear(); initial.program.push_back({0x5ff,0xf495});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_SP,restore ? 0x2f4U : 0x300U}});
		u32 pc = 0x600;
		std::vector<clocked_data_event> events;
		for (unsigned i = 0; i != std::size(mmrs); ++i) {
			u16 const word = (restore ? 0x5000 : 0x4000) + i;
			initial.program.push_back({pc++,u16(0x7700 | mmrs[i])}); initial.program.push_back({pc++,word});
			events.push_back({true,mmrs[i],word,0,false});
			initial.data.push_back({0x2ff - i,u16(restore ? 0x4000 + i : 0)});
		}
		if (restore) for (unsigned i = std::size(mmrs); i != 0; --i) {
			initial.program.push_back({pc++,u16(0x8a00 | mmrs[i - 1])});
			events.push_back({false,0x300 - i,u16(0x4000 + i - 1),0,false});
			events.push_back({true,mmrs[i - 1],u16(0x4000 + i - 1),0,false});
		}
		for (unsigned i = 0; i != std::size(mmrs); ++i) {
			initial.program.push_back({pc++,u16(0x4a00 | mmrs[i])});
			events.push_back({false,mmrs[i],u16(0x4000 + i),0,false});
			events.push_back({true,0x2ff - i,u16(0x4000 + i),0,false});
		}
		initial.program.push_back({pc++,0xf4e1});
		auto final = observation("twelve-owned-registers-and-descending-stack", {{STATE_GENPC,pc},{r::STATE_SP,0x2f4}});
		for (unsigned i = 0; i != std::size(mmrs); ++i) {
			final.expected_data.push_back({mmrs[i],u16(0x4000 + i)});
			final.expected_data.push_back({0x2ff - i,u16(0x4000 + i)});
		}
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-burst-snapshot", {{STATE_GENPC,0x600}})},
			{2,observation("burst-running", {})},{128,std::move(final)}},1,128};
		c.pipeline_frontend = false; c.data_events = std::move(events); cases.push_back(std::move(c));
	}
	return cases;
}
}
