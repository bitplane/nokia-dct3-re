// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned condition = 0; condition < 2; ++condition)
	for (unsigned tc = 0; tc < 2; ++tc)
	for (unsigned producer = 0; producer < 3; ++producer)
	for (unsigned gap = 0; gap < 2; ++gap)
	for (unsigned selector : {0U,15U})
	for (unsigned page = 0; page < 2; ++page)
	{
		unsigned const offset = producer + gap;
		bool const taken = tc == condition;
		u32 const base = page ? 0x20000 : 0;
		u16 const word = tc ? 1U << (15 - selector) : 0;
		u16 const source = producer == 2 ? 0x6d1 : 0x1201;
		auto initial = initial_case("tc-return", {0xf495}); initial.program.clear();
		for (unsigned i = 0; i < 16; ++i) initial.program.push_back({base + 0x9000 + i,0xf495});
		if (producer) initial.program[0].value = producer == 2 ? 0x34f8 : 0x3401;
		if (producer == 2) initial.program[1].value = 0x6d1;
		initial.program[offset].value = condition ? 0xfc30 : 0xfc20;
		initial.program[offset + 1].value = 0xf4e1;
		initial.program.push_back({base + 0x9100,0xf4e1});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x9000},
			{r::STATE_ST0,0xa824 | ((producer ? !tc : tc) ? 0x1000U : 0U)},
			{r::STATE_ST1,0xb00},{r::STATE_SP,0x2000},{r::STATE_T,0xa5f0 | selector},
			{r::STATE_A,0xff12345678ULL},{r::STATE_B,0x81abcdef01ULL}});
		initial.data = {{0x2000,0x9100},{source,word}};
		unsigned const point = std::array<unsigned,4>{3,4,5,7}[(condition + tc + producer + gap + selector + page) % 4] + offset;
		clocked_register_case c{{{0,std::move(initial)}},point,32};
		for (unsigned tick = 1; tick <= 32; ++tick) {
			auto state = observation("conditional-return-cadence", {{r::STATE_IDLE,unsigned(tick >= (taken ? 11 : 9) + offset)}});
			if (tick == 32) state.expected.insert(state.expected.end(), {
				{STATE_GENPC,base + (taken ? 0x9101 : 0x9002 + offset)},{r::STATE_SP,0x2000 + unsigned(taken)},
				{r::STATE_ST0,0xa824 | (!tc ? 0x1000U : 0U)},{r::STATE_ST1,0xb00},
				{r::STATE_A,0xff12345678ULL},{r::STATE_B,0x81abcdef01ULL},{r::STATE_T,0xa5f0 | selector}});
			c.states.push_back({tick,std::move(state)});
		}
		c.actions = {{5 + offset,{{r::STATE_ST0,0xa824 | (!tc ? 0x1000U : 0U)}},false,{}, {},{{0x2000,0xdead}}}};
		c.data_events = std::vector<clocked_data_event>{};
		if (producer) c.data_events->push_back({false,source,word,0,false});
		if (taken) c.data_events->push_back({false,0x2000,0x9100,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
