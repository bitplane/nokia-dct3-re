// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form = 0; form < 5; ++form) for (unsigned trn = 0; trn < 2; ++trn)
	for (u16 word : {0U,1U,0x8000U,0xffffU}) for (unsigned flags = 0; flags < 4; ++flags)
	for (unsigned gap = 0; gap < 2; ++gap) {
		unsigned const seed = form + trn + word + flags + gap, words = form >= 3 ? 3 : 2;
		unsigned const read = 5 + unsigned(form >= 3), consume = read + (trn ? 2 : 3) + gap;
		u16 const source = std::array<u16,5>{0x16,0x400,0x5000,0x5003,0x5000}[form];
		u16 const st1 = 0x900 | (flags & 1 ? 0x40 : 0) | (flags & 2 ? 0x200 : 0);
		u32 const base = seed & 1 ? 0x20000 : 0;
		auto initial = initial_case("word-move-to-MPY-or-LDM", {std::array<u16,5>{0x7316,0x7100,0x7182,0x71e2,0x71f8}[form]});
		if (form >= 3) initial.program[1].value = form == 3 ? 3 : source;
		initial.program[words - 1].value = 0xe + trn;
		initial.program[words + gap].value = trn ? 0x480f : 0xf066;
		if (!trn) initial.program[words + gap + 1].value = 0x8000;
		initial.program[words + gap + (trn ? 1 : 2)].value = 0xf4e1;
		for (auto &entry : initial.program) entry.address += base;
		initial.data = {{source,word}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa008},{r::STATE_ST1,st1},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfedcba9876ULL},{r::STATE_SP,0x3000},
			{r::STATE_T,0xbeef},{r::STATE_TRN,0x2468},{r::STATE_AR2,0x5000}});
		s64 const product = s64(s16(word)) * -32768 * (flags & 1 ? 2 : 1);
		bool const overflow = !trn && product > 0x7fffffffLL;
		u64 const result = trn ? word : u64(overflow && (flags & 2) ? 0x7fffffffLL : product) & 0xffffffffffULL;
		auto final = observation("retained-move-and-consumer-operands", {{r::STATE_A,result},{r::STATE_B,0xfedcba9876ULL},
			{r::STATE_T,trn ? 0xbeefU : 0xdeadU},{r::STATE_TRN,trn ? 0xdeadU : 0x2468U},
			{r::STATE_ST0,0xa008U | (overflow ? 0x400U : 0U)},{r::STATE_ST1,st1},
			{STATE_GENPC,base + 0x600 + words + gap + (trn ? 2 : 3)}});
		clocked_register_case c{{{0,std::move(initial)},{24,std::move(final)}},std::array<unsigned,3>{3,read,consume}[seed % 3],24};
		c.actions = {{read,{},false,{},{},{{source,0xdead}}},{consume,{{trn ? r::STATE_TRN : r::STATE_T,0xdead}}}};
		c.data_events = std::vector<clocked_data_event>{{false,source,word,read},{true,0xe + trn,word,read + 1}};
		if (trn) c.data_events->push_back({false,0xf,word,consume});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
