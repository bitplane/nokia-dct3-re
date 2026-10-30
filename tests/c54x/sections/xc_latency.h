// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned staged = 0; staged < 2; ++staged)
	for (unsigned kind = 0; kind < 6; ++kind)
	for (unsigned gap = 0; gap < 4; ++gap)
	for (unsigned slots = 1; slots <= 2; ++slots)
	for (unsigned old = 0; old < 2; ++old)
	for (unsigned polarity = 0; polarity < 2; ++polarity)
	{
		bool const take = (gap < 2 ? old : !old) == polarity;
		unsigned const output = slots == 2 && take ? !polarity : !old;
		unsigned const prefix = kind >= 4 ? 2 : 1;
		u16 const status = 0x605 | (kind < 2 ? old << (kind ? 11 : 12) : 0);
		auto initial = initial_case("xc-latency", {0xf495});
		unsigned pc = 0;
		if (kind < 2) initial.program[pc++].value = (old ? 0xf4b0 : 0xf5b0) | (kind ? 11 : 12);
		else if (kind < 4) initial.program[pc++].value = 0xe800 | ((kind & 1) << 8) | !old;
		else { initial.program[pc++].value = 0xf020 | ((kind & 1) << 8); initial.program[pc++].value = !old; }
		pc += gap;
		u16 const predicate = kind == 0 ? polarity ? 0x30 : 0x20 : kind == 1 ? polarity ? 0x0c : 8 :
			0x40 | ((kind & 1) << 3) | (polarity ? 4 : 5);
		initial.program[pc++].value = (slots == 2 ? 0xff00 : 0xfd00) | predicate;
		if (slots == 2) initial.program[pc++].value = kind < 2 ?
			((polarity ? 0xf4b0 : 0xf5b0) | (kind ? 11 : 12)) : 0xe800 | ((kind & 1) << 8) | !polarity;
		initial.program[pc++].value = kind >= 2 && !(kind & 1) ? 0xe933 : 0xe833;
		initial.program[pc + 1].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,status},
			{r::STATE_A,kind >= 2 && !(kind & 1) ? old : 0},{r::STATE_B,kind >= 2 && (kind & 1) ? old : 0}});
		clocked_register_case c{{{0,std::move(initial)}},(kind + gap + slots + old + polarity) & 1 ? 5 + prefix : 1U,40};
		c.pipeline_frontend = staged;
		c.states.push_back({40,observation("one-retained-condition", {
			{r::STATE_A,kind >= 2 && !(kind & 1) ? output : take ? 0x33U : 0U},
			{r::STATE_B,kind >= 2 && (kind & 1) ? output : kind >= 2 && take ? 0x33U : 0U},
			{r::STATE_ST0,0x605 | (kind < 2 ? output << (kind ? 11 : 12) : 0)},
			{r::STATE_ST1,0x800},{STATE_GENPC,0x603 + prefix + gap + slots},{r::STATE_SP,0x1000}})});
		c.data_events = std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
