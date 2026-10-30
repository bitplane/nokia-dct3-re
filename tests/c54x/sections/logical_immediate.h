// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned operation = 0; operation < 3; ++operation)
	for (unsigned selector = 0; selector < 4; ++selector)
	for (unsigned shift : {0U,1U,7U,15U,16U})
	for (unsigned pattern = 0; pattern < 2; ++pattern)
	for (unsigned flags = 0; flags < 4; ++flags) {
		bool const source_b = selector & 1, destination_b = selector & 2;
		u16 const literal = pattern ? 0xf495 : 0x8001;
		u64 const source = pattern ? 0xff89abcdefULL : 0x12345678, operand = u64(literal) << shift;
		u64 const result = operation == 0 ? source & operand : operation == 1 ? source | operand : source ^ operand;
		u64 const a = source_b ? 0x1111222233ULL : source, b = source_b ? source : 0x4444555566ULL;
		u16 const opcode = (shift == 16 ? 0xf063 + operation : 0xf030 + (operation << 4) + shift) |
			(source_b ? 0x200 : 0) | (destination_b ? 0x100 : 0);
		u16 const st1 = 0x800 | ((flags & 1) << 8) | ((flags & 2) << 8);
		auto initial = initial_case("logical-immediate", {opcode,literal});
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,0xefff},{r::STATE_ST1,st1}});
		clocked_register_case c{{{0,std::move(initial)}},3 + ((operation + selector + pattern + flags) & 1),9};
		for (unsigned tick = 1; tick <= 9; ++tick) c.states.push_back({tick,observation("unsigned-logical-publication", {
			{r::STATE_A,tick >= 7 && !destination_b ? result : a},{r::STATE_B,tick >= 7 && destination_b ? result : b},
			{r::STATE_ST0,0xefff},{r::STATE_ST1,st1}})});
		c.actions = {{3,{},false,{{0x601,0}}}};
		c.program_events = {{false,0x601,literal,3}};
		c.forbid_data_access = true; c.require_idle = false; c.check_pipeline_clock = true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
