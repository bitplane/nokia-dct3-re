// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr std::array<u16,12> opcodes{0x7100,0x7d00,0x7500,0x7400,0x7e00,0x7000,0x6000,0x6100,0x8b00,0x7f00,0x7c00,0xe001};
	constexpr std::array<u16,8> operands{0x700,0x700,0x124,0x123,0,0x700,0xbeef,0x8000};
	for (unsigned row = 0; row != 46; ++row) {
		unsigned const family = row < 44 ? row/4 : row == 44 ? 11 : 10, form = row < 44 ? row%4 : row == 44 ? 0 : 4;
		u16 const opcode = opcodes[family] | (family == 11 ? 0 : std::array<u16,5>{0,0x92,0xe2,0xf8,0x9a}[form]);
		std::vector<u16> extensions;
		if (family == 11) extensions = {0x900};
		else if (form == 2) {
			if (family == 10) extensions = {0x900,5};
			else if (family >= 1 && family <= 3) extensions = {operands[family],5};
			else { extensions = {5}; if (family < 8 && family != 4) extensions.push_back(operands[family]); }
		} else {
			if (form == 3) extensions.push_back(0x600);
			if (family == 10) extensions.push_back(0x900);
			if (family < 8 && family != 4) extensions.push_back(operands[family]);
		}
		auto initial = initial_case("extensions-before-data-IO-and-program-operands",{opcode});
		for (unsigned i = 0; i != extensions.size(); ++i) initial.program[i+1].value = extensions[i];
		initial.program[1+extensions.size()].value = 0xf4e1;
		for (auto &word : initial.program) word.address += 0x200;
		initial.program.insert(initial.program.end(),{{0x900,u16(family == 9 ? 0xdead : 0xbeef)},{0x700,0}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,0x800},{r::STATE_A,family == 11 ? 0U : 0x900U},{r::STATE_B,0},
			{r::STATE_AR2,form == 4 ? 0x5ffU : form == 2 ? 0x5fbU : 0x600U},{r::STATE_AR3,0x601},
			{r::STATE_SP,family == 8 ? 0x700U : 0x1000U},{r::STATE_ST0,0xc},{r::STATE_ST1,0}});
		initial.data = {{0x600,u16(family == 8 || family == 10 ? 0xdead : 0xbeef)},{0x601,0xcafe},{0x5ff,0xcafe},
			{0x700,u16(family == 5 || family == 8 ? 0xbeef : 0)}};
		auto final = observation("operand-value-and-address-modifier-preserved",{{r::STATE_A,family == 11 ? u64(0xbeef+0xcafe) << 16 : 0x900U},{r::STATE_B,0}});
		if (family != 11) final.expected.push_back({r::STATE_AR2,form == 1 ? 0x601U : form == 2 ? 0x5fbU : 0x600U});
		if (family == 6 || family == 7) final.expected.push_back({r::STATE_ST0,0x1000,0x1000});
		if (family == 8) final.expected.push_back({r::STATE_SP,0x701});
		if (family == 0) final.expected_data.push_back({0x700,0xbeef});
		if (family >= 3 && family != 11) final.expected_data.push_back({0x600,u16(family == 3 ? 0xabcd : 0xbeef)});
		if (family == 1) final.expected_program.push_back({0x700,0xbeef});
		if (family == 9 || family == 10) final.expected_program.push_back({0x900,0xbeef});
		if (family == 10) final.expected_data.push_back({0x5ff,0xcafe});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("operand-order-replay-checkpoint",{})},{24,std::move(final)}},1,24};
		c.pipeline_frontend = false;
		c.initial_io = {{0x123,0xabcd},{0x124,0xaaaa}};
		c.data_events = std::vector<clocked_data_event>{}; c.io_events = std::vector<clocked_data_event>{};
		for (unsigned i = 0; i != extensions.size(); ++i) c.program_events.push_back({false,0x801+i,extensions[i],0,false});
		auto data = [&](bool write,u16 at,u16 value) { c.data_events->push_back({write,at,value,0,false}); };
		if (family == 0 || family == 1 || family == 2 || family == 6 || family == 7 || family == 9 || family == 11) data(false,0x600,0xbeef);
		if (family == 5 || family == 8) data(false,0x700,0xbeef);
		if (family == 11) data(false,0x601,0xcafe);
		if (family == 0) data(true,0x700,0xbeef);
		if (family == 3 || family == 4 || family == 5 || family == 8 || family == 10) data(true,0x600,family == 3 ? 0xabcd : 0xbeef);
		if (family == 1 || family == 9) c.program_events.push_back({true,u32(family == 1 ? 0x700 : 0x900),0xbeef,0,false});
		if (family == 4 || family == 10 || family == 11) c.program_events.push_back({false,0x900,0xbeef,0,false});
		if (family == 2 || family == 3) c.io_events->push_back({family == 2,u32(family == 2 ? 0x124 : 0x123),u16(family == 2 ? 0xbeef : 0xabcd),0,false});
		for (auto &event : *c.data_events) event.minimum_program_events = extensions.size();
		for (auto &event : *c.io_events) event.minimum_program_events = extensions.size();
		cases.push_back(std::move(c));
	}
	return cases;
}
}
