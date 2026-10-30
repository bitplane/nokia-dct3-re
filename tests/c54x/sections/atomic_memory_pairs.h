// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto add = [&](char const *name, std::vector<u16> words, std::vector<register_setting> registers,
		std::vector<register_check> expected, std::vector<memory_word> data,
		std::vector<memory_word> results, std::vector<clocked_data_event> events) {
		auto initial = initial_case(name,{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1 + i].value = words[i];
		initial.program[1 + words.size()].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_BK,4},{r::STATE_ST0,0},{r::STATE_ST1,0x100}});
		initial.initial.insert(initial.initial.end(),registers.begin(),registers.end()); initial.data = data;
		for (auto const &word : results)
			if (std::none_of(initial.data.begin(),initial.data.end(),[&](auto const &v) { return v.address == word.address; })) initial.data.push_back({word.address,0xbeef});
		auto final = observation("paired-pointer-data-and-source-results",{{STATE_GENPC,0x602 + words.size()}});
		final.expected.insert(final.expected.end(),expected.begin(),expected.end()); final.expected_data = initial.data;
		for (auto const &word : results)
			for (auto &value : final.expected_data) if (value.address == word.address) value.value = word.value;
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-pair-snapshot",{{STATE_GENPC,0x601}})},
			{2,observation("pair-running",{})},{48,std::move(final)}},1,48};
		c.pipeline_frontend = false; c.data_events = std::move(events); cases.push_back(std::move(c));
	};
	add("store-decrement-feeds-store-increment",{0x768a,0xabcd,0x8092},
		{{r::STATE_A,0x12345678},{r::STATE_AR2,0xd80},{r::STATE_ST1,0}},{{r::STATE_A,0x12345678},{r::STATE_AR2,0xd80}},
		{},{{0xd80,0xabcd},{0xd7f,0x5678}},{{true,0xd80,0xabcd,0,false},{true,0xd7f,0x5678,0,false}});
	add("dual-move-increment-feeds-circular-source",{0xe5a8,0xe5ca},
		{{r::STATE_AR0,1},{r::STATE_AR2,0xd02},{r::STATE_AR4,0xe00}},{{r::STATE_AR0,1},{r::STATE_AR2,0xd00},{r::STATE_AR4,0xe02}},
		{{0xe00,0x1357},{0xd03,0x2468}},{{0xd02,0x1357},{0xe01,0x2468}},
		{{false,0xe00,0x1357,0,false},{true,0xd02,0x1357,0,false},{false,0xd03,0x2468,0,false},{true,0xe01,0x2468,0,false}});
	add("unsigned-absolute-and-decrement-loads",{0x12f8,0xd40,0x138b},
		{{r::STATE_AR3,0xd50}},{{r::STATE_A,0x8001},{r::STATE_B,0xff80},{r::STATE_AR3,0xd4f}},
		{{0xd40,0x8001},{0xd50,0xff80}},{},{{false,0xd40,0x8001,0,false},{false,0xd50,0xff80,0,false}});
	add("fixed-copy-then-low-B-store",{0x7182,0xd71,0x8183},
		{{r::STATE_B,0x1234abcd},{r::STATE_AR2,0xd70},{r::STATE_AR3,0xd72},{r::STATE_ST1,0}},
		{{r::STATE_B,0x1234abcd},{r::STATE_AR2,0xd70},{r::STATE_AR3,0xd72}},{{0xd70,0xaaa5}},{{0xd71,0xaaa5},{0xd72,0xabcd}},
		{{false,0xd70,0xaaa5,0,false},{true,0xd71,0xaaa5,0,false},{true,0xd72,0xabcd,0,false}});
	add("signed-high-loads-opposite-updates",{0x4492,0x458b},
		{{r::STATE_AR2,0xd00},{r::STATE_AR3,0xd10}},{{r::STATE_A,0xff80010000ULL},{r::STATE_B,0xffff800000ULL},{r::STATE_AR2,0xd01},{r::STATE_AR3,0xd0f}},
		{{0xd00,0x8001},{0xd10,0xff80}},{},{{false,0xd00,0x8001,0,false},{false,0xd10,0xff80,0,false}});
	add("positive-high-loads-opposite-pointers",{0x4493,0x458a},
		{{r::STATE_AR2,0xd01},{r::STATE_AR3,0xd11}},{{r::STATE_A,0x7fff0000},{r::STATE_B,0x800000},{r::STATE_AR3,0xd12},{r::STATE_AR2,0xd00}},
		{{0xd11,0x7fff},{0xd01,0x80}},{},{{false,0xd11,0x7fff,0,false},{false,0xd01,0x80,0,false}});
	add("OR-through-AR2-A-and-AR3-B",{0x1a82,0x1b83},
		{{r::STATE_A,0x12340000},{r::STATE_B,0xaaaa0000},{r::STATE_AR2,0xd20},{r::STATE_AR3,0xd30},{r::STATE_ST1,0}},
		{{r::STATE_A,0x123400f0},{r::STATE_B,0xaaaa0f0f},{r::STATE_AR2,0xd20},{r::STATE_AR3,0xd30}},
		{{0xd20,0xf0},{0xd30,0xf0f}},{},{{false,0xd20,0xf0,0,false},{false,0xd30,0xf0f,0,false}});
	add("OR-through-AR3-A-and-AR2-B",{0x1a83,0x1b82},
		{{r::STATE_A,0xaaaa0000},{r::STATE_B,0xcccc0000},{r::STATE_AR2,0xd20},{r::STATE_AR3,0xd30},{r::STATE_ST1,0}},
		{{r::STATE_A,0xaaaa5555},{r::STATE_B,0xcccc3333},{r::STATE_AR2,0xd20},{r::STATE_AR3,0xd30}},
		{{0xd20,0x3333},{0xd30,0x5555}},{},{{false,0xd30,0x5555,0,false},{false,0xd20,0x3333,0,false}});
	add("MAR-decrement-feeds-XOR",{0x6d8c,0x1c84},{{r::STATE_A,0x12340000},{r::STATE_AR4,0xd41}},
		{{r::STATE_A,0x12340f0f},{r::STATE_AR4,0xd40}},{{0xd40,0xf0f},{0xd41,0xf0f0}},{},{{false,0xd40,0xf0f,0,false}});
	add("signed-load-decrement-feeds-low-store",{0x108a,0x8084},{{r::STATE_AR2,0xd51},{r::STATE_AR4,0xd60}},
		{{r::STATE_A,0xffffff8001ULL},{r::STATE_AR2,0xd50},{r::STATE_AR4,0xd60}},{{0xd51,0x8001},{0xd60,0}},{{0xd60,0x8001}},
		{{false,0xd51,0x8001,0,false},{true,0xd60,0x8001,0,false}});
	add("OR-decrement-feeds-dual-move",{0x1a8b,0xe598},{{r::STATE_A,0x100000},{r::STATE_AR2,0xd80},{r::STATE_AR3,0xd70},{r::STATE_ST1,0}},
		{{r::STATE_A,0x1000f0},{r::STATE_AR3,0xd70},{r::STATE_AR2,0xd81}},{{0xd6f,0x5555},{0xd70,0xf0},{0xd80,0}},{{0xd80,0x5555}},
		{{false,0xd70,0xf0,0,false},{false,0xd6f,0x5555,0,false},{true,0xd80,0x5555,0,false}});
	add("MVMM-one-source-two-destinations",{0xe742,0xe743},{{r::STATE_AR2,0},{r::STATE_AR3,0},{r::STATE_AR4,0xd90}},
		{{r::STATE_AR2,0xd90},{r::STATE_AR3,0xd90},{r::STATE_AR4,0xd90}},{},{},
		{{false,0x14,0xd90,0,false},{true,0x12,0xd90,0,false},{false,0x14,0xd90,0,false},{true,0x13,0xd90,0,false}});
	add("MVMM-stack-pointer-round-trip",{0xe748,0xe782},{{r::STATE_AR2,0},{r::STATE_AR4,0xd90},{r::STATE_SP,0x300}},
		{{r::STATE_SP,0xd90},{r::STATE_AR2,0xd90},{r::STATE_AR4,0xd90}},{},{},
		{{false,0x14,0xd90,0,false},{true,0x18,0xd90,0,false},{false,0x18,0xd90,0,false},{true,0x12,0xd90,0,false}});
	return cases;
}
}
