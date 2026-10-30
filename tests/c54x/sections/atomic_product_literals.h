// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto append = [&](char const *name,std::initializer_list<u16> words,std::initializer_list<register_setting> setup,
			std::initializer_list<register_check> checks,std::vector<clocked_data_event> events = {}) {
		auto initial = initial_case(name,{*words.begin()}); unsigned at = 0;
		for (u16 word : words) initial.program[at++].value = word;
		initial.program[at].value = 0xf4e1; initial.program.push_back({0x5ff,0xf495});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_ST0,0},{r::STATE_ST1,0},{r::STATE_T,0},{r::STATE_AR3,0x200}});
		initial.initial.insert(initial.initial.end(),setup.begin(),setup.end());
		for (auto const &event : events) if (!event.write) initial.data.push_back({event.address,event.value});
		auto final = observation(name,checks); final.expected.push_back({STATE_GENPC,0x601 + unsigned(words.size())});
		for (auto const &event : events) if (event.write) final.expected_data.push_back({event.address,event.value});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-product-snapshot", {{STATE_GENPC,0x600}})},
			{2,observation("product-running", {})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false; c.data_events = std::move(events); cases.push_back(std::move(c));
	};
	append("MPYA signed17",{0xf58c},{{r::STATE_A,0x80000000},{r::STATE_B,0},{r::STATE_T,2}},
		{{r::STATE_A,0x80000000},{r::STATE_B,0x10000},{r::STATE_T,2}});
	append("MPYA publishes T",{0x3183},{{r::STATE_A,0x80000000},{r::STATE_B,0},{r::STATE_T,0x1234}},
		{{r::STATE_B,0x10000},{r::STATE_T,2}},{{false,0x200,2,0,false}});
	append("MACA old source",{0xf488},{{r::STATE_A,0x80000000},{r::STATE_B,0x1234},{r::STATE_T,1}},
		{{r::STATE_A,0x80008000},{r::STATE_B,0x1234}});
	for (unsigned fractional = 0; fractional < 2; ++fractional) append("MACAR rounding",{0xf689},
		{{r::STATE_A,0xfffffe0000ULL},{r::STATE_B,0x800c},{r::STATE_T,4},{r::STATE_ST0,0x1800},{r::STATE_ST1,fractional ? 0x40U : 0U}},
		{{r::STATE_A,fractional ? 0U : 0x10000U},{r::STATE_B,0x800c},{r::STATE_T,4},{r::STATE_ST0,0x1800},{r::STATE_ST1,fractional ? 0x40U : 0U}});
	append("MASAR negative product",{0xf48b},{{r::STATE_A,0x10000},{r::STATE_B,0x1234},{r::STATE_T,0x8000}},
		{{r::STATE_A,0x20000},{r::STATE_B,0x1234}});
	append("SQDST signed17",{0xe211},{{r::STATE_A,0x80010000},{r::STATE_B,0}},
		{{r::STATE_A,0},{r::STATE_B,0x40010001},{r::STATE_AR3,0x200}},{{false,0x200,0,0,false},{false,0x200,0,0,false}});
	append("SQUR signed17",{0xf48d},{{r::STATE_A,0x80010000}},{{r::STATE_A,0x40010001}});
	append("MAC immediate fractional",{0xf167,0x345},{{r::STATE_A,0x1000},{r::STATE_B,0},{r::STATE_T,0x400},{r::STATE_ST1,0x40}},
		{{r::STATE_A,0x1000},{r::STATE_B,0x1a3800},{r::STATE_T,0x400}});
	append("MAC memory immediate postincrement",{0x6495,0x1234},{{r::STATE_A,0x1000},{r::STATE_AR5,0x100}},
		{{r::STATE_A,0x6261060},{r::STATE_T,0x5678},{r::STATE_AR5,0x101}},{{false,0x100,0x5678,0,false}});
	append("MAC absolute extension order",{0x65f8,0x200,5},{{r::STATE_A,0x100},{r::STATE_B,0}},
		{{r::STATE_A,0x100},{r::STATE_B,0x10f},{r::STATE_T,3}},{{false,0x200,3,0,false}});
	append("ADD immediate shift16 carry",{0xf160,0xffff},{{r::STATE_A,0x10000},{r::STATE_B,0x1234},{r::STATE_ST1,0x100}},
		{{r::STATE_A,0x10000},{r::STATE_B,0},{r::STATE_ST0,0x800,0x800}});
	append("SUB immediate shift16 carry",{0xf361,1},{{r::STATE_A,0x1234},{r::STATE_B,0x20000},{r::STATE_ST1,0x100}},
		{{r::STATE_A,0x1234},{r::STATE_B,0x10000},{r::STATE_ST0,0x800,0x800}});
	append("ADD unsigned overflow",{0xf060,0xffff},{{r::STATE_A,0}},{{r::STATE_A,0xffff0000},{r::STATE_ST0,0x400,0x400}});
	append("MAC offset extension order",{0x65e3,4,2},{{r::STATE_A,0x10},{r::STATE_B,0}},
		{{r::STATE_A,0x10},{r::STATE_B,0x1c},{r::STATE_T,3},{r::STATE_AR3,0x200}},{{false,0x202,3,0,false}});
	append("MAC fractional saturation",{0xf167,0x8000},{{r::STATE_A,0},{r::STATE_B,0},{r::STATE_T,0x8000},{r::STATE_ST1,0x240}},
		{{r::STATE_A,0},{r::STATE_B,0x7fffffff},{r::STATE_ST0,0x200,0x200}});
	for (unsigned form = 0; form < 4; ++form) {
		u16 const source = form == 3 ? 0x202 : 0x200, word = std::array<u16,4>{0x6cac,0x2222,0x55aa,0xabcd}[form];
		u16 const opcode = std::array<u16,4>{0x4d83,0x4deb,0x4df8,0x4de3}[form];
		append("DELAY retains T and source",form == 0 ? std::initializer_list<u16>{opcode} :
			std::initializer_list<u16>{opcode,u16(form == 1 ? 1 : form == 2 ? 0x200 : 2)},
			{{r::STATE_T,0x1234},{r::STATE_AR3,form == 1 ? 0x1ffU : 0x200U},{r::STATE_ST1,0x240}},
			{{r::STATE_T,0x1234},{r::STATE_AR3,0x200}},{{false,source,word,0,false},{true,u32(source + 1),word,0,false}});
	}
	append("MPYU fractional saturation",{0x2483},{{r::STATE_A,0},{r::STATE_T,0xffff},{r::STATE_ST1,0x240}},
		{{r::STATE_A,0x7fffffff},{r::STATE_T,0xffff},{r::STATE_AR3,0x200},{r::STATE_ST0,0x400,0x400}},{{false,0x200,0xffff,0,false}});
	return cases;
}
}
