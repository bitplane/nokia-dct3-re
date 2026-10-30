// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	{
		auto initial = initial_case("MVDK-ST-MVPD-MVDP-STL-distinct-memory-spaces", {0xf495,0x71da,0xf40,0x7692,0xabcd,0x7c8b,0x700,0x7d82,0x710,0x8182,0xf4e1});
		initial.program.insert(initial.program.end(),{{0x700,0xbeef},{0x710,0}});
		initial.data = {{0xf21,0},{0xf22,0x1234},{0xf23,0x5a3c},{0xf40,0},{0xf50,0}};
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0x1234},{r::STATE_B,0xcafe},{r::STATE_AR0,2},
			{r::STATE_AR2,0xf23},{r::STATE_AR3,0xf50},{r::STATE_BK,4},{r::STATE_ST0,0x800},{r::STATE_ST1,0x100}});
		auto final = observation("program-copy-retains-data-before-later-overwrite", {{STATE_GENPC,0x60b},{r::STATE_A,0x1234},
			{r::STATE_B,0xcafe},{r::STATE_AR0,2},{r::STATE_AR2,0xf22},{r::STATE_AR3,0xf4f},{r::STATE_BK,4},
			{r::STATE_ST0,0x800},{r::STATE_ST1,0x100}},{{0xf21,0xabcd},{0xf22,0xcafe},{0xf23,0x5a3c},{0xf40,0x5a3c},{0xf50,0xbeef}});
		final.expected_program = {{0x700,0xbeef},{0x710,0x1234}};
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-transfer-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("transfers-running", {})},{64,std::move(final)}},1,64};
		c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{{false,0xf23,0x5a3c,0,false},{true,0xf40,0x5a3c,0,false},
			{true,0xf21,0xabcd,0,false},{true,0xf50,0xbeef,0,false},{false,0xf22,0x1234,0,false},{true,0xf22,0xcafe,0,false}};
		c.program_events = {{false,0x700,0xbeef,0,false},{true,0x710,0x1234,0,false}}; cases.push_back(std::move(c));
	}
	{
		auto initial = initial_case("MVMM-AR6-to-AR2-without-source-marker", {0xf495,0xe762,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_AR6,0xabcd},{r::STATE_AR2,0},{r::STATE_ST0,0x800}});
		auto final = observation("mapped-source-preserved", {{STATE_GENPC,0x603},{r::STATE_AR6,0xabcd},{r::STATE_AR2,0xabcd},{r::STATE_ST0,0x800}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-copy-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("copy-running", {})},{24,std::move(final)}},1,24};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{{false,0x16,0xabcd,0,false},{true,0x12,0xabcd,0,false}};
		cases.push_back(std::move(c));
	}
	for (u16 opcode : {0xe80d,0xe820,0xe905,0xe906,0xe90c,0xe90f,0xe97c}) {
		bool const b = (opcode & 0xff00) == 0xe900;
		auto initial = initial_case("LD-short-immediate-destination-literals", {0xf495,opcode,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_ST0,0x800},{r::STATE_ST1,0x100}});
		auto final = observation("only-destination-changes", {{STATE_GENPC,0x603},{r::STATE_A,b ? 0U : opcode & 0xffU},
			{r::STATE_B,b ? opcode & 0xffU : 0U},{r::STATE_ST0,0x800},{r::STATE_ST1,0x100}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-load-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("load-running", {})},{24,std::move(final)}},1,24};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{}; cases.push_back(std::move(c));
	}
	for (unsigned count = 0; count != 256; ++count) {
		auto initial = initial_case("RPT-every-eight-bit-count-with-MAR-body", {0xf495,u16(0xec00 | count),0x6d91,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_AR1,0x120},{r::STATE_ST0,0x800},{r::STATE_ST1,0x100}});
		auto final = observation("count-plus-one-body-effects-and-continuation", {{STATE_GENPC,0x604},
			{r::STATE_AR1,0x121 + count},{r::STATE_RC,0},{r::STATE_ST0,0x800},{r::STATE_ST1,0x100}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-repeat-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("repeat-running", {})},{count + 32,std::move(final)}},1,count + 32};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{}; cases.push_back(std::move(c));
	}
	return cases;
}
}
