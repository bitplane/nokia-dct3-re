// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	struct row { u16 opcode; bool b,returning; u64 input; unsigned output; bool taken; };
	constexpr row rows[]{
		{0xfe44,false,true,1,2,true},{0xfe44,false,true,0,3,false},
		{0xfe4c,true,true,1,2,true},{0xfe4c,true,true,0,3,false},
		{0xfa43,false,false,0xffffffffffULL,2,true},{0xfa43,false,false,1,3,false},
		{0xfa4f,true,false,0xffffffffffULL,2,true},{0xfa4f,true,false,0,2,true},{0xfa4f,true,false,1,3,false}
	};
	std::vector<clocked_register_case> cases;
	for (auto const &v : rows) {
		auto initial = initial_case("atomic-near-control-retains-delay-decision", {v.opcode});
		unsigned cursor = 1;
		if (!v.returning) initial.program[cursor++].value = 0x700;
		for (unsigned value : {v.returning ? 0U : 1U,2U,3U}) initial.program[cursor++].value = (v.b ? 0xe900 : 0xe800) | value;
		initial.program[cursor].value = 0xf4e1;
		initial.program.push_back({0x700,0xf4e1}); initial.program.push_back({0x5ff,0xf495});
		initial.data = {{0x3000,0x700}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_A,v.b ? 0x1234 : v.input},
			{r::STATE_B,v.b ? v.input : 0x1234},{r::STATE_SP,0x3000},{r::STATE_ST0,0},{r::STATE_ST1,0x800}});
		auto final = observation("delay-overwrite-does-not-change-decision", {{STATE_GENPC,v.taken ? 0x701U : 0x601 + cursor},
			{r::STATE_A,v.b ? 0x1234U : v.output},{r::STATE_B,v.b ? v.output : 0x1234U},
			{r::STATE_SP,0x3000U + unsigned(v.returning && v.taken)}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-control-snapshot", {{STATE_GENPC,0x600}})},
			{2,observation("control-running", {})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{};
		if (v.returning && v.taken) c.data_events->push_back({false,0x3000,0x700,0,false});
		cases.push_back(std::move(c));
	}
	{
		auto initial = initial_case("atomic-two-push-two-pop-page-zero", {0x4a62,0x4a63,0x8a62,0x8a63,0xf4e1});
		initial.program.push_back({0x5ff,0xf495}); initial.data = {{0x62,0x1234},{0x63,0x5678}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_SP,0x3000}});
		auto final = observation("stack-order-swaps-encoded-RAM-words", {{STATE_GENPC,0x605},{r::STATE_SP,0x3000}},
			{{0x2fff,0x1234},{0x2ffe,0x5678},{0x62,0x5678},{0x63,0x1234}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-stack-snapshot", {{r::STATE_SP,0x3000}})},
			{2,observation("stack-running", {})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{{false,0x62,0x1234,0,false},{true,0x2fff,0x1234,0,false},
			{false,0x63,0x5678,0,false},{true,0x2ffe,0x5678,0,false},{false,0x2ffe,0x5678,0,false},
			{true,0x62,0x5678,0,false},{false,0x2fff,0x1234,0,false},{true,0x63,0x1234,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
