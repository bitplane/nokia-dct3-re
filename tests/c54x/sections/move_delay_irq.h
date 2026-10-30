// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned operation = 0; operation < 4; ++operation) for (unsigned reg = 0; reg < 8; ++reg)
	for (unsigned taken = 0; taken < 2; ++taken) for (u16 word : {0U,1U,0x8000U,0xffffU})
	for (unsigned interrupt = 0; interrupt < 3; ++interrupt) {
		bool const mvkd = operation != 0;
		unsigned const form = mvkd ? operation - 1 : 0, seed = operation + reg + taken + word + interrupt;
		u16 const source = mvkd && form == 0 ? 0x10 + (reg + 7) % 8 : mvkd && form == 2 ? 0x1400 : 0x500;
		u16 const destination = mvkd ? 0x1400 : 0x10 + reg;
		auto initial = initial_case("data-move-complete-branch-delay", {u16(mvkd ? 0xfa4d : 0xfa47),0x700,
			u16(mvkd ? 0x7000 | (form == 1 ? 0x90 : 0x80) | reg : 0x7210 | reg),source,0xe922});
		// Keep the legacy NOP continuation before IDLE; an adjacent IDLE may wake on the pending interrupt.
		initial.program[0x10].value = 0xf4e1;
		for (unsigned at = 0x700; at <= 0x710; ++at) initial.program.push_back({at,at == 0x700 ? 0xe911U : at == 0x710 ? 0xf4e1U : 0xf495U});
		initial.program.insert(initial.program.end(), {{4,0xf4eb},{0x40,0xf4eb}});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0xa608},{r::STATE_ST1,0},{r::STATE_IMR,1},
			{r::STATE_A,mvkd || taken ? 0U : 1U},{r::STATE_B,mvkd && !taken ? 1U : 0U}});
		for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({r::STATE_AR0 + ar,ar == reg && mvkd ? 0x1400U : 0x400U + ar});
		if (mvkd) initial.data.push_back({destination,0xaaaa});
		if (mvkd && form == 0) initial.initial.push_back({int(r::STATE_AR0 + (reg + 7) % 8),word});
		else initial.data.push_back({source,word});
		auto final = observation("move-retired-and-interrupt-returned", {{int(r::STATE_AR0 + reg),mvkd ? 0x1400U + unsigned(form == 1) : unsigned(word)},
			{r::STATE_B,taken ? 0x11U : 0x22U},{r::STATE_SP,0x1000},{STATE_GENPC,taken ? 0x711U : 0x611U}});
		if (mvkd) final.expected_data.push_back({0x1400,word});
		clocked_register_case c{{{0,std::move(initial)},{40,std::move(final)}},std::array<unsigned,3>{8,9,12}[seed % 3],40};
		if (interrupt) c.actions.push_back({5,{},false,{},{{interrupt == 2 ? INPUT_LINE_NMI : 0,ASSERT_LINE}}});
		c.actions.push_back({6,{},false,{{0x601,0xbeef},{0x603,0xdead}}});
		if (mvkd) {
			clocked_register_action poison{9,{}};
			if (form == 0) poison.values.push_back({int(r::STATE_AR0 + (reg + 7) % 8),0xdead});
			else poison.data.push_back({source,0xdead});
			c.actions.push_back(std::move(poison));
		}
		// Observe the move endpoints; stack correctness is checked by return PC/SP.
		c.data_ranges = {{source,source},{destination,destination}};
		c.data_events = std::vector<clocked_data_event>{{false,source,word,9},{true,destination,word,mvkd ? 10U : 9U}};
		c.program_events = {{false,0x601,0x700,0,false},{false,0x603,source,0,false}};
		if (interrupt) c.irqs = {{interrupt == 2 ? INPUT_LINE_NMI : 0,9,40,2,{}}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
