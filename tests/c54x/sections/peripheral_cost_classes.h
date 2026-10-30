// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	constexpr u16 targets[]{0x12,0x25,0x5f,0x60};
	std::vector<clocked_register_case> cases;
	for (unsigned family = 0; family != 5; ++family) for (unsigned kind = 0; kind != 4; ++kind) {
		bool const read = family == 4, rmw = family == 1 || family == 2;
		u16 const target = targets[kind];
		u16 const opcode = std::array<u16,5>{0x7700,0x6800,0x6900,0x7600,0x1000}[family] | target;
		std::vector<u16> words;
		for (unsigned i = 0; i != 2; ++i) { words.push_back(opcode); if (!read) words.push_back(0xff00); }
		words.push_back(0xf4e1);
		auto initial = initial_case("atomic-CPU-peripheral-and-scratch-cost-classes",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1+i].value = words[i];
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0x5678},{r::STATE_AR2,0xff00},{r::STATE_PRD,0xff00},
			{r::STATE_ST0,0},{r::STATE_ST1,0x800}});
		initial.data = {{target,0xff00}};
		auto final = observation("two-real-accesses-at-the-specified-cost",{{STATE_GENPC,0x601+words.size()},
			{r::STATE_A,read ? 0xff00U : 0x5678U},{r::STATE_AR2,0xff00},{r::STATE_PRD,0xff00}},{{target,0xff00}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-cost-program",{{STATE_GENPC,0x601}})},
			{2,observation("cost-program-running",{})},{48,std::move(final)}},(family+kind)&1 ? 3U : 1U,48};
		c.pipeline_frontend = false;
		unsigned const cost = (read ? 1U : 2U) + unsigned(kind == 1 || kind == 2);
		c.data_events = std::vector<clocked_data_event>{};
		for (unsigned i = 0; i != 2; ++i) {
			if (rmw) c.data_events->push_back({false,target,0xff00,0,false,{},i ? std::optional<unsigned>(cost) : std::nullopt});
			c.data_events->push_back({!read,target,0xff00,0,false,{},rmw ? std::optional<unsigned>(0) : i ? std::optional<unsigned>(cost) : std::nullopt});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
