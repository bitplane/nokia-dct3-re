// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned n = 0; n < 32; ++n)
	{
		unsigned const mode = n % 4;
		bool const mapped = n & 4, high = n & 8;
		u32 const base = (n & 16 ? 3 : 2) << 16, origin = base + 0x9000;
		u16 const source = high ? 0x8700 : 0x700, value = mapped && !high ? 0x1111 : 0x4444;
		u32 const physical = mapped && !high ? source : base + source;
		auto initial = initial_case("short-mvpd", {u16(0x7c00 | (mode < 2 ? 0x20 : mode == 2 ? 0x82 : 0x92)),source});
		for (auto &word : initial.program) word.address += origin - 0x600;
		initial.program.insert(initial.program.end(), {{source,0x1111},{base + source,0x4444}});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,origin},{r::STATE_PMST,mapped ? 0x20 : 0},
			{r::STATE_ST0,0x608},{r::STATE_ST1,mode == 1 ? 0x4800 : 0x800},{r::STATE_SP,0x400},{r::STATE_AR2,0x420}});
		initial.data = {{0x420,0xffff}};
		clocked_register_case c{{{0,std::move(initial)},
			{20,observation("transferred", {{r::STATE_AR2,mode == 3 ? 0x421U : 0x420U},{r::STATE_SP,0x400},
				{r::STATE_ST1,mode == 1 ? 0x4800U : 0x800U}}, {{0x420,value}})}},
			std::array<unsigned,4>{4,6,7,9}[(mode + n / 4) % 4],20};
		c.require_idle = false;
		c.actions = {{4,{},false,{{origin + 1,0x321}}},{7,{},false,{{physical,0xaaaa}}}};
		c.program_events = {{false,origin + 1,source,3},{false,physical,value,7}};
		c.data_events = std::vector<clocked_data_event>{{true,0x420,value,8}};
		cases.push_back(std::move(c));
	}
	// Absolute destination plus all8 selectors for each of three AR forms.
	for (unsigned route = 0; route < 25; ++route) for (unsigned pattern = 0; pattern < 4; ++pattern)
	for (unsigned mapped = 0; mapped < 2; ++mapped) for (unsigned high = 0; high < 2; ++high)
	for (unsigned cmpt = 0; cmpt < 2; ++cmpt)
	{
		unsigned const form = route ? 1 + (route - 1) / 8 : 0, reg = route ? (route - 1) % 8 : 0;
		unsigned const selected = cmpt && !reg ? 5 : reg;
		u32 const origin = 0x29000;
		u16 const source = high ? 0xff87 : 0x2e00;
		u32 const physical = mapped && !high ? source : 0x20000 + source;
		u16 const destination = std::array<u16,4>{0x802,0x122f,0x1231,0x1201}[form];
		u16 const value = std::array<u16,4>{0,0xffff,0x2468,0xf495}[pattern];
		u16 const mode = form ? 0xe0 + (form - 1) * 8 + reg : 0xf8;
		u16 const first = form ? source : destination, second = form ? form == 1 ? 0xffff : form == 2 ? 1 : 2 : source;
		auto initial = initial_case("extended-mvpd", {u16(0x7c00 | mode),first,second,0xf4e1});
		for (auto &word : initial.program) word.address += origin - 0x600;
		initial.program.insert(initial.program.end(), {{physical ^ 0x20000,u16(value ^ 0xffff)},{physical,value}});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,origin},{r::STATE_PMST,mapped ? 0x20 : 0},
			{r::STATE_ST0,cmpt ? 0xa624 : 0x624},{r::STATE_ST1,cmpt ? 0x820 : 0x800},{r::STATE_BK,16}});
		for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({r::STATE_AR0 + ar,
			ar == selected ? form == 3 ? 0x120f : 0x1230 : 0x3000 + ar});
		initial.data = {{destination,0xbeef}};
		auto address = observation("address", {{int(r::STATE_AR0) + int(selected),form == 3 ? 0x1201U : form == 2 ? 0x1231U : 0x1230U}});
		auto final = observation("transferred", {}, {{destination,value}});
		for (unsigned ar = 0; ar < 8; ++ar) final.expected.push_back({int(r::STATE_AR0) + int(ar),ar == selected ? 0x4000 : 0x3000 + ar});
		clocked_register_case c{{{0,std::move(initial)},{7,std::move(address)},
			{8,observation("poisoned-pointer", {{int(r::STATE_AR0) + int(selected),0x4000}})},{24,std::move(final)}},
			std::array<unsigned,3>{6,8,9}[(route + pattern + mapped + high + cmpt) % 3],24};
		c.actions = {{7,{{r::STATE_AR0 + selected,0x4000}}},
			{8,{{r::STATE_PMST,mapped ? 0 : 0x20}},false,{{physical,0xdead}}}};
		c.program_events = {{false,origin + 1,first,3},{false,origin + 2,second,4},{false,physical,value,8}};
		c.data_events = std::vector<clocked_data_event>{{true,destination,value,9}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
