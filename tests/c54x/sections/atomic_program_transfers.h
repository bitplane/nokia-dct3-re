// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	struct row { u16 opcode,data,program,word; unsigned count; bool to_program,absolute,accumulator; };
	constexpr row rows[]{
		{0x7d92,0xd00,0xb00,0x2468,1,true,false,false},
		{0x7c92,0xd10,0xb00,0x2468,1,false,false,false},
		{0x7df8,0xd00,0xb01,0x2468,1,true,true,false},
		{0x7cf8,0xd11,0xb01,0x2468,1,false,true,false},
		{0x7c92,0xd20,0xb10,0,3,false,false,false},
		{0x7f92,0xd30,0xb20,0xabcd,1,true,false,true},
		{0x7ef8,0xd31,0xb20,0xabcd,1,false,true,true},
		{0x7f92,0xd40,0xb30,0,3,true,false,true}
	};
	std::vector<clocked_register_case> cases;
	for (auto const &v : rows) {
		auto initial = initial_case("atomic-program-data-transfer-literal", {0xf495}); unsigned at = 0;
		if (v.count == 3) initial.program[at++].value = 0xec02;
		initial.program[at++].value = v.opcode;
		if (v.absolute) initial.program[at++].value = v.data;
		if (!v.accumulator) initial.program[at++].value = v.program;
		initial.program[at].value = 0xf4e1; initial.program.push_back({0x5ff,0xf495});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_AR2,v.data},{r::STATE_A,v.program}});
		auto final = observation("independent-address-spaces-and-retained-A", {{STATE_GENPC,0x601 + at},
			{r::STATE_AR2,v.absolute ? v.data : unsigned(v.data + v.count)},{r::STATE_A,v.program}});
		if (v.count == 3) final.expected.push_back({r::STATE_RC,0});
		clocked_register_case c{{},1,48}; c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{};
		for (unsigned i = 0; i < v.count; ++i) {
			u16 const word = v.count == 3 ? std::array<u16,3>{0x1357,0x2468,0x369a}[i] : v.word;
			initial.data.push_back({v.data + i,v.to_program ? word : u16(0)});
			initial.program.push_back({v.program + i,v.to_program ? u16(0) : word});
			final.expected_data.push_back({v.data + i,word}); final.expected_program.push_back({v.program + i,word});
			c.data_events->push_back({!v.to_program,v.data + i,word,0,false});
			c.program_events.push_back({v.to_program,v.program + i,word,0,false});
		}
		c.states = {{0,std::move(initial)},{1,observation("pre-transfer-snapshot", {{STATE_GENPC,0x600}})},
			{2,observation("transfer-running", {})},{48,std::move(final)}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
