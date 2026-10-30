// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	{
		auto initial = initial_case("repeated-program-transfers-cross-page", {0xf495});
		initial.program = {{0x107ff,0xf495},{0x10800,0xec02},{0x10801,0x7f90},
			{0x10802,0xec02},{0x10803,0x7e90},{0x10804,0xf4e1},
			{0x2fffe,0},{0x2ffff,0},{0x30000,0},{0xfffe,0xaaaa},{0xffff,0xbbbb},{0,0xcccc},{0x20000,0xdddd}};
		initial.data = {{0x500,0x1234},{0x501,0x5678},{0x502,0x9abc},{0x503,0},{0x504,0},{0x505,0}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x107ff},{r::STATE_A,0x2fffe},{r::STATE_AR0,0x500}});
		auto final = observation("PAR-carries-without-changing-A-or-XPC", {{STATE_GENPC,0x10805},
			{r::STATE_A,0x2fffe},{r::STATE_AR0,0x506}},{{0x503,0x1234},{0x504,0x5678},{0x505,0x9abc}});
		final.expected_program = {{0x2fffe,0x1234},{0x2ffff,0x5678},{0x30000,0x9abc},
			{0xfffe,0xaaaa},{0xffff,0xbbbb},{0,0xcccc},{0x20000,0xdddd}};
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-repeat-snapshot", {{STATE_GENPC,0x10800}})},
			{2,observation("repeat-running", {})},{64,std::move(final)}},1,64};
		c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{{false,0x500,0x1234,0,false},{false,0x501,0x5678,0,false},
			{false,0x502,0x9abc,0,false},{true,0x503,0x1234,0,false},{true,0x504,0x5678,0,false},{true,0x505,0x9abc,0,false}};
		c.program_events = {{false,0x2fffe,0x1234,0,false},{false,0x2ffff,0x5678,0,false},{false,0x30000,0x9abc,0,false}};
		cases.push_back(std::move(c));
	}
	{
		auto initial = initial_case("OVLY-folds-common-window-not-upper-page", {0xf495});
		initial.program = {{0x600,0xf495},{0x601,0x7f90},{0x602,0x7e90},{0x603,0xf882},{0x604,0x8000},
			{0x28000,0xe909},{0x28001,0xf4e1},{0x20600,0xf4e1},{7,0},{0x20007,0xaaaa}};
		initial.data = {{0x500,0x1234},{0x501,0}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x20600},{r::STATE_PMST,0x20},
			{r::STATE_A,0x20007},{r::STATE_AR0,0x500}});
		auto final = observation("logical-XPC-survives-overlay", {{STATE_GENPC,0x28002},{r::STATE_B,9}},{{0x501,0x1234}});
		final.expected_program = {{7,0x1234},{0x20007,0xaaaa},{0x20600,0xf4e1}};
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-overlay-transfer-snapshot", {{STATE_GENPC,0x20601}})},
			{2,observation("overlay-transfer-running", {})},{48,std::move(final)}},1,48};
		c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{{false,0x500,0x1234,0,false},{true,0x501,0x1234,0,false}};
		c.program_events = {{false,7,0x1234,0,false}};
		cases.push_back(std::move(c));
	}
	for (unsigned arp = 0; arp != 8; ++arp)
	{
		auto initial = initial_case("immediate-ARP", {0xf495,u16(0xf4a0 | arp),0xf4e1});
		initial.initial.push_back({r::STATE_ST0,0x1abc});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-ARP", {{STATE_GENPC,0x601}})},
			{2,observation("ARP-loaded", {})},{16,observation("only-ARP-field-changed", {{STATE_GENPC,0x603},
				{r::STATE_ST0,0x1abc | (arp << 13)}})}},1,16};
		c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	}
	for (u16 value : {0xffff,0x0200,0x0155})
	{
		auto initial = initial_case("absolute-DP", {0xf495,0x46f8,0x500,0xf4e1});
		initial.initial.push_back({r::STATE_ST0,0xfe00});
		initial.data = {{0x500,value}};
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-DP", {{STATE_GENPC,0x601}})},
			{2,observation("DP-loading", {})},{16,observation("only-nine-DP-bits-loaded", {{STATE_GENPC,0x604},
				{r::STATE_ST0,u64(0xfe00 | (value & 0x1ff))}})}},1,16};
		c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{{false,0x500,value,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
