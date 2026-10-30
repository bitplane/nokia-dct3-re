// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto add = [&](char const *id, std::vector<u16> words, std::vector<register_setting> initial_regs,
		std::vector<register_check> expected, std::vector<memory_word> data = {},
		unsigned interval = 5, std::vector<memory_word> expected_data = {}) {
		auto initial = initial_case(id,{0xf495});
		initial.initial.insert(initial.initial.end(),{{r::STATE_ST0,0},{r::STATE_ST1,0x100}});
		initial.initial.insert(initial.initial.end(),initial_regs.begin(),initial_regs.end());
		std::vector<u16> program{0x75f8,0x100,0x124};
		program.insert(program.end(),words.begin(),words.end());
		program.insert(program.end(),{0x75f8,0x100,0x124,0xf4e1});
		for (unsigned i = 0; i != program.size(); ++i) initial.program[1+i].value = program[i];
		initial.data = std::move(data); initial.data.push_back({0x100,0x1111});
		auto final = observation("long-offset-result-and-marker-interval",{});
		final.expected.insert(final.expected.end(),expected.begin(),expected.end());
		final.expected.push_back({STATE_GENPC,0x601+program.size()});
		final.expected_data = std::move(expected_data);
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-offset-markers",{{STATE_GENPC,0x601}})},
			{2,observation("offset-markers-running",{})},{48,std::move(final)}},3,48};
		c.pipeline_frontend = false; c.observed_data_space = AS_IO;
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},{true,0x124,0x1111,0,false,{},interval}};
		cases.push_back(std::move(c));
	};
		add("addc-long-preupdate-clear-carry", {0x06ea, 5}, {{r::STATE_A, 0x13}, {r::STATE_AR2, 0x0f00}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_A, 0x18}, {r::STATE_AR2, 0x0f05}, {r::STATE_ST0, 0, 0x0800}}, {{0x0f05, 4}});
		add("adds-long-offset-stationary", {0x02e2, 5}, {{r::STATE_A, 1}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_A, 0x10000}, {r::STATE_AR2, 0x0f00}}, {{0x0f05, 0xffff}});
		add("adds-long-circular-preupdate", {0x02f2, 2}, {{r::STATE_A, 1}, {r::STATE_AR2, 0x0f03}, {r::STATE_BK, 4}},
			{{r::STATE_A, 4}, {r::STATE_AR2, 0x0f01}}, {{0x0f01, 3}});
		add("store-low-long-negative-preupdate", {0x80ea, 0xfffe}, {{r::STATE_A, 0x12345678}, {r::STATE_AR2, 0x0f10}},
			{{r::STATE_AR2, 0x0f0e}}, {{0x0f0e, 0}}, 5, {{0x0f0e, 0x5678}});
		add("addm-long-offset-before-immediate", {0x6bea, 5, 1}, {{r::STATE_AR2, 0x0f00}, {r::STATE_ST0, 0}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_AR2, 0x0f05}}, {{0x0f05, 3}}, 6, {{0x0f05, 4}});
		add("andm-immediate-before-long-offset", {0x68e2, 0x0f0f, 5}, {{r::STATE_AR2, 0x0f00}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_AR2, 0x0f00}, {r::STATE_ST0, 0x0800, 0x0800}}, {{0x0f05, 0xff00}}, 6, {{0x0f05, 0x0f00}});
		add("orm-long-circular-retains-carry", {0x69f2, 0x8000, 2}, {{r::STATE_AR2, 0x0f03}, {r::STATE_BK, 4}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_AR2, 0x0f01}, {r::STATE_ST0, 0x0800, 0x0800}}, {{0x0f01, 1}}, 6, {{0x0f01, 0x8001}});
		add("store-long-offset-before-literal", {0x76ea, 5, 0xcafe}, {{r::STATE_AR2, 0x0f00}},
			{{r::STATE_AR2, 0x0f05}}, {{0x0f05, 0}}, 6, {{0x0f05, 0xcafe}});
		add("store-high-long-preupdate", {0x82ea, 5}, {{r::STATE_A, 0x12345678}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_AR2, 0x0f05}}, {{0x0f05, 0}}, 5, {{0x0f05, 0x1234}});
		add("store-high-b-long-stationary", {0x83e2, 5}, {{r::STATE_B, 0x56789abc}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_AR2, 0x0f00}}, {{0x0f05, 0}}, 5, {{0x0f05, 0x5678}});
		add("store-t-long-preupdate", {0x8cea, 5}, {{r::STATE_T, 0x4321}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_AR2, 0x0f05}}, {{0x0f05, 0}}, 5, {{0x0f05, 0x4321}});
		add("load-signed-long-preupdate", {0x10ea, 5}, {{r::STATE_ST1, 0x0100}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_A, 0xffffff8001ULL}, {r::STATE_AR2, 0x0f05}}, {{0x0f05, 0x8001}});
		add("load-unsigned-long-stationary", {0x12e2, 5}, {{r::STATE_AR2, 0x0f00}},
			{{r::STATE_A, 0x8001}, {r::STATE_AR2, 0x0f00}}, {{0x0f05, 0x8001}});
		add("load-high-b-long-preupdate", {0x45ea, 5}, {{r::STATE_AR2, 0x0f00}},
			{{r::STATE_B, 0x12340000}, {r::STATE_AR2, 0x0f05}}, {{0x0f05, 0x1234}});

	return cases;
}
}
