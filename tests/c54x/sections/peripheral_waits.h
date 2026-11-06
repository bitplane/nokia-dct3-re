// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned family = 0; family != 4; ++family) for (unsigned boundary = 0; boundary != 2; ++boundary) {
		u32 const base = (family ^ boundary) & 1 ? 0x10000 : 0;
		std::vector<u16> words{std::array<u16,4>{0x1025,0x8025,0x6825,0x68f8}[family]};
		if (family >= 2) words.push_back(family == 3 ? 0x25 : 0xfffe);
		if (family == 3) words.push_back(0xfffe);
		auto initial = initial_case("single-paid-peripheral-wait-and-RMW",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[i].value = words[i];
		for (auto &word : initial.program) word.address += base;
		u16 const source = family == 0 ? 0x1234 : family == 1 ? 0xaaaa : 0xffff;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0},{r::STATE_ST1,0x800},
			{r::STATE_A,0x5678},{r::STATE_PRD,source}});
		auto final = observation("one-access-wait-without-RMW-double-charge",{{r::STATE_A,family == 0 ? 0x1234U : 0x5678U},
			{r::STATE_PRD,family == 0 ? 0x1234U : family == 1 ? 0x5678U : 0xfffeU}});
		unsigned const wait = family == 0 ? 5 : family == 3 ? 7 : 6;
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-peripheral-publication",{{r::STATE_A,0x5678}})}},wait+boundary,10};
		if (!family) c.states.push_back({7,observation("load-retires-after-paid-read",{{r::STATE_A,0x1234}})});
		c.states.push_back({10,std::move(final)});
		c.require_idle = false; c.check_pipeline_clock = true;
		c.data_events = std::vector<clocked_data_event>{};
		if (family != 1) c.data_events->push_back({false,0x25,source,family == 0 ? 6U : family == 2 ? 7U : 8U});
		if (family != 0) c.data_events->push_back({true,0x25,u16(family == 1 ? 0x5678 : 0xfffe),family == 1 ? 7U : family == 2 ? 8U : 9U});
		cases.push_back(std::move(c));
	}
	auto initial = initial_case("unsupported-adjacent-peripheral-arbitration",{0x8025,0x1025});
	for (auto &word : initial.program) word.address += 0x10000;
	initial.initial.insert(initial.initial.end(),{{STATE_GENPC,0x10600},{r::STATE_PRD,0xaaaa},{r::STATE_A,0x5678},
		{r::STATE_ST0,0},{r::STATE_ST1,0x800}});
	auto running = observation("arbitration-running",{}); running.expected.clear();
	auto final = observation("arbitration-rejects-before-either-bus-access",{{r::STATE_PRD,0xaaaa},{r::STATE_A,0x5678}});
	final.expected.back().value = 1;
	clocked_register_case c{{{0,std::move(initial)},{1,std::move(running)},{12,std::move(final)}},3,12};
	c.require_idle = false; c.final_pipeline_clock = 6;
	c.data_events = std::vector<clocked_data_event>{};
	cases.push_back(std::move(c));
	for (unsigned destination : {0U,1U}) for (unsigned completed : {0U,1U,2U}) {
		auto initial = initial_case("CLKMD-write-before-retained-read",{0x7758,2,u16(0x4858 | destination << 8),0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0x1234},{r::STATE_B,0x5678}});
		auto final = observation("one-write-one-deferred-read",{{r::STATE_A,destination ? 0x1234U : 3U},
			{r::STATE_B,destination ? 3U : 0x5678U}},{{0x58,3}});
		clocked_register_case pair{{{0,std::move(initial)},{24,std::move(final)}},5,24};
		// Capture before the write, between accesses, or after the deferred read.
		if (completed) pair.checkpoint_after_data_events = completed;
		pair.data_events = std::vector<clocked_data_event>{{true,0x58,2,0,false},{false,0x58,3,0,false,{},1}};
		cases.push_back(std::move(pair));
	}
	return cases;
}
}
