// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 192; ++index) {
		unsigned const kind = index % 6, form = (index / 6) % 4, pair = (index / 24) % 4;
		bool const sxm = index / 96, b = kind & 1;
		u16 const address = form == 0 ? 0xa0 : form == 1 ? 0x1020 : 0x470;
		u16 const operand = std::array<u16,4>{0xffff,0x8000,0xff,0x8001}[pair];
		u64 const source = std::array<u64,4>{0,0xffff,0x123456789aULL,0xff80000000ULL}[pair];
		u64 const result = kind < 2 ? source & operand : kind < 4 ? source | operand : source ^ operand;
		u32 const base = (kind + form + pair) & 1 ? 0x20000 : 0;
		u16 const opcode = (0x1800 + (kind / 2) * 0x200) | (b ? 0x100 : 0) | std::array<u16,4>{0x20,0x20,0x92,0x8a}[form];
		u16 const st1 = 0x800 | (sxm ? 0x100 : 0) | (form == 1 ? 0x4000 : 0);
		auto initial = initial_case("memory-logic", {opcode});
		for (auto &word : initial.program) word.address += base;
		initial.data = {{address,operand}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,b ? 0x2233445566ULL : source},
			{r::STATE_B,b ? source : 0x2233445566ULL},{r::STATE_AR2,0x470},{r::STATE_ST0,0xb601},{r::STATE_ST1,st1}});
		clocked_register_case c{{{0,std::move(initial)}},3 + (kind + form + pair) % 3,10};
		for (unsigned tick = 1; tick <= 10; ++tick) {
			u64 const selected = tick >= 6 ? result : source;
			c.states.push_back({tick,observation("zero-extended-retained-operand", {{r::STATE_A,b ? 0x2233445566ULL : selected},
				{r::STATE_B,b ? selected : 0x2233445566ULL},{r::STATE_AR2,tick >= 4 ? form == 2 ? 0x471U : form == 3 ? 0x46fU : 0x470U : 0x470U},
				{r::STATE_ST0,0xb601},{r::STATE_ST1,st1}})});
		}
		c.actions = {{5,{},false,{}, {},{{address,0xdead}}}};
		c.data_events = std::vector<clocked_data_event>{{false,address,operand,5}};
		c.require_idle = false; c.check_pipeline_clock = true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
