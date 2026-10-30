// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form = 0; form < 8; ++form) for (u16 value : {0U,1U,0x8000U,0xffffU})
	for (unsigned compiler = 0; compiler < 2; ++compiler) {
		unsigned const read = form == 5 || form == 6 ? 7 : 6, words = read == 7 ? 3 : 2;
		unsigned const seed = form + value + compiler;
		u32 const base = seed & 1 ? 0x20000 : 0;
		u16 const destination = std::array<u16,8>{u16(compiler ? 0x1002 : 0x402),0x1400,0x1400,0x1400,0x1401,0x1500,0x1403,0x13}[form];
		u16 const literal = form == 5 ? destination : form == 6 ? 3 : 0x18;
		u16 const next = form == 2 || form == 4 ? 0x1401 : form == 3 ? 0x13ff : 0x1400;
		auto initial = initial_case("move-live-SP-to-retained-destination",
			{std::array<u16,8>{0x7002,0x7082,0x7092,0x708a,0x709a,0x70f8,0x70e2,0x7213}[form],literal});
		if (words == 3) initial.program[2].value = 0x18;
		initial.program[words].value = 0xf4e1;
		for (auto &entry : initial.program) entry.address += base;
		if (destination >= 0x60) initial.data = {{destination,0xaaaa}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa608},
			{r::STATE_ST1,0x900U | (compiler ? 0x4000U : 0U)},{r::STATE_SP,0x1000},{r::STATE_AR2,0x1400},{r::STATE_AR3,0x1357}});
		auto final = observation("captured-SP-and-address-survive-live-replacement", {{STATE_GENPC,base + 0x601 + words},
			{r::STATE_SP,0xbeef},{r::STATE_AR2,0x2468},{r::STATE_ST0,0xa608},
			{r::STATE_ST1,0x900U | (compiler ? 0U : 0x4000U)}},{{destination,value}});
		clocked_register_case c{{{0,std::move(initial)},
			{read - 1,observation("single-address-update", {{r::STATE_AR2,next}})},
			{read,observation("Read-samples-live-SP", {})},{20,std::move(final)}},
			std::array<unsigned,3>{4,read,read + 1}[seed % 3],20};
		c.actions = {{read - 2,{},false,{{base + 0x601,0xdead}}},
			{read - 1,{{r::STATE_SP,value},{r::STATE_ST1,0x900U | (compiler ? 0U : 0x4000U)}}},
			{read,{{r::STATE_SP,0xbeef},{r::STATE_AR2,0x2468}}}};
		if (words == 3) c.actions.front().program.push_back({base + 0x602,0xbeef});
		c.program_events = {{false,base + 0x601,literal,0,false}};
		if (words == 3) c.program_events.push_back({false,base + 0x602,0x18,0,false});
		c.data_events = std::vector<clocked_data_event>{{false,0x18,value,read},
			{true,destination,value,read + unsigned(form != 7)}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
