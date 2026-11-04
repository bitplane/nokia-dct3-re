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
	for (unsigned operation=0;operation<3;++operation) for (bool b : {false,true})
	for (unsigned count : {0U,1U,3U,7U}) for (bool long_count : {false,true}) {
		u64 const source=0xa512345678ULL, other=0x8182838485ULL;
		u16 const opcode=u16(0x1800+operation*0x200+(b ? 0x100 : 0)+0x91);
		auto initial=initial_case("repeated-logic-captures-every-RAM-word",{u16(long_count ? 0xf070 : 0xec00|count)});
		initial.program.erase(initial.program.begin()+1,initial.program.end());
		if (long_count) initial.program.push_back({0x601,u16(count)});
		initial.program.push_back({u32(long_count ? 0x602 : 0x601),opcode});
		initial.program.push_back({u32(long_count ? 0x603 : 0x602),0xf4e1});
		u32 const base=(b^long_count) ? 0x20000 : 0;
		for (auto &word : initial.program) word.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,b ? other : source},
			{r::STATE_B,b ? source : other},{r::STATE_AR1,0x5000},{r::STATE_ST0,0xb601},{r::STATE_ST1,0x900}});
		u64 result=source;
		std::vector<clocked_data_event> reads;
		for (unsigned i=0;i<=count;++i) {
			u16 const operand=u16(0x8421U^(i*0x1357U));
			initial.data.push_back({0x5000+i,operand});
			reads.push_back({false,0x5000+i,operand,0,false});
			result=operation==0 ? result&operand : operation==1 ? result|operand : result^operand;
		}
		auto final=observation("logical-reduction-pointer-flags-and-repeat-count",{{r::STATE_A,b ? other : result},
			{r::STATE_B,b ? result : other},{r::STATE_AR1,0x5001+count},{r::STATE_RC,0},
			{r::STATE_ST0,0xb601},{r::STATE_ST1,0x900}});
		clocked_register_case c{{{0,std::move(initial)},{64,std::move(final)}},6+(count%3),64};
		c.data_events=std::move(reads);
		c.actions={{7,{},false,{}, {},{{0x5000,0xdead}}}};
		cases.push_back(std::move(c));
	}
	for (unsigned operation=0;operation<3;++operation) for (bool b : {false,true}) for (u16 address : {u16(8),u16(0x25)}) {
		auto initial=initial_case("repeated-logic-rejects-unqualified-mapped-source",{0xec01,u16(0x1800+operation*0x200+(b ? 0x100 : 0)+0x91)});
		initial.initial.insert(initial.initial.end(),{{r::STATE_AR1,address},{r::STATE_A,0x1234},{r::STATE_B,0x5678}});
		initial.expected.clear();
		auto final=observation("no-mapped-read-or-pointer-update",{{r::STATE_AR1,address},{r::STATE_A,0x1234},{r::STATE_B,0x5678}});
		final.expected.back().value=1;
		clocked_register_case c{{{0,std::move(initial)},{16,std::move(final)}},3,16};
		c.require_idle=false;c.forbid_data_access=true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
