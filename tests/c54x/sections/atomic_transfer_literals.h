// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto append = [&](char const *name,std::initializer_list<u16> words,std::initializer_list<register_setting> setup,
		std::initializer_list<register_check> checks,std::vector<clocked_data_event> events) {
		auto initial = initial_case(name,{*words.begin()}); unsigned at = 0;
		for (u16 word : words) initial.program[at++].value = word;
		initial.program[at].value = 0xf4e1; initial.program.push_back({0x5ff,0xf495});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_ST1,0},{r::STATE_T,0},{r::STATE_REA,0xffff}});
		initial.initial.insert(initial.initial.end(),setup.begin(),setup.end());
		auto final = observation(name,checks); final.expected.push_back({STATE_GENPC,0x601 + unsigned(words.size())});
		for (auto const &e : events) if (e.write) {
			if (e.address >= 0x60) initial.data.push_back({e.address,0});
			auto found = std::find_if(final.expected_data.begin(),final.expected_data.end(),[&](auto const &word) { return word.address == e.address; });
			if (found == final.expected_data.end()) final.expected_data.push_back({e.address,e.value}); else found->value = e.value;
		}
		for (auto const &e : events) if (!e.write && e.address >= 0x60) {
			initial.data.push_back({e.address,e.value});
			if (std::none_of(events.begin(),events.end(),[&](auto const &other) { return other.write && other.address == e.address; }))
				final.expected_data.push_back({e.address,e.value});
		}
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-transfer-snapshot", {{STATE_GENPC,0x600}})},
			{2,observation("transfer-running", {})},{48,std::move(final)}},1,48};
		c.pipeline_frontend = false; c.data_events = std::move(events); cases.push_back(std::move(c));
	};
	append("STM-BRC-literal",{0x771a,0x42},{},{{r::STATE_BRC,0x42}},{{true,0x1a,0x42,0,false}});
	append("ST-literal-absolute",{0x76f8,0xd50,0x5678},{},{},{{true,0xd50,0x5678,0,false}});
	append("ST-literal-indirect",{0x7682,0x9abc},{{r::STATE_AR2,0xd51}},{{r::STATE_AR2,0xd51}},{{true,0xd51,0x9abc,0,false}});
	append("PSHD-absolute",{0x4bf8,0xd60},{{r::STATE_SP,0xe00}},{{r::STATE_SP,0xdff}},
		{{false,0xd60,0xabcd,0,false},{true,0xdff,0xabcd,0,false}});
	append("POPD-absolute",{0x8bf8,0xd61},{{r::STATE_SP,0xdff}},{{r::STATE_SP,0xe00}},
		{{false,0xdff,0xabcd,0,false},{true,0xd61,0xabcd,0,false}});
	append("PSHM-ST0",{0x4a06},{{r::STATE_ST0,5},{r::STATE_SP,0xe00}},{{r::STATE_ST0,5},{r::STATE_SP,0xdff}},
		{{false,6,5,0,false},{true,0xdff,5,0,false}});
	append("POPM-ST0",{0x8a06},{{r::STATE_ST0,0},{r::STATE_SP,0xdff}},{{r::STATE_ST0,5},{r::STATE_SP,0xe00}},
		{{false,0xdff,5,0,false},{true,6,5,0,false}});
	append("ADDC-absolute-B",{0x07f8,0xd70},{{r::STATE_B,5},{r::STATE_ST0,0x800}},{{r::STATE_B,8}},{{false,0xd70,2,0,false}});
	append("OR-absolute-A",{0x1af8,0xd70},{{r::STATE_A,0xf00}},{{r::STATE_A,0xff0}},{{false,0xd70,0xf0,0,false}});
	append("SUB-absolute-A",{0x08f8,0xd70},{{r::STATE_A,10}},{{r::STATE_A,7}},{{false,0xd70,3,0,false}});
	append("MPYU-postincrement",{0x2494},{{r::STATE_T,0xfffe},{r::STATE_AR4,0xd80}},
		{{r::STATE_A,0x2fffa},{r::STATE_AR4,0xd81}},{{false,0xd80,3,0,false}});
	append("MPYU-absolute",{0x24f8,0xd80},{{r::STATE_T,0xfffe}},{{r::STATE_A,0x2fffa}},{{false,0xd80,3,0,false}});
	append("MVDK-postincrement",{0x7192,0xda0},{{r::STATE_AR2,0xd90}},{{r::STATE_AR2,0xd91}},
		{{false,0xd90,0x1234,0,false},{true,0xda0,0x1234,0,false}});
	append("MVKD-repeat-three",{0xec02,0x7093,0xd90},{{r::STATE_AR3,0xdb0}},{{r::STATE_AR3,0xdb3},{r::STATE_RC,0}},
		{{false,0xd90,0x1111,0,false},{true,0xdb0,0x1111,0,false},{false,0xd91,0x2222,0,false},
		{true,0xdb1,0x2222,0,false},{false,0xd92,0x3333,0,false},{true,0xdb2,0x3333,0,false}});
	append("LD-high-absolute",{0x44f8,0xdc0},{},{{r::STATE_A,0x12340000}},{{false,0xdc0,0x1234,0,false}});
	append("ST-low-absolute",{0x80f8,0xdc1},{{r::STATE_A,0x12345678}},{},{{true,0xdc1,0x5678,0,false}});
	append("ST-high-absolute",{0x82f8,0xdc2},{{r::STATE_A,0x12345678}},{},{{true,0xdc2,0x1234,0,false}});
	append("ST-T-absolute",{0x8cf8,0xdc3},{{r::STATE_T,0x4321}},{},{{true,0xdc3,0x4321,0,false}});
	append("MVDK-absolute",{0x71f8,0xdd0,0xde0},{},{},{{false,0xdd0,0x4444,0,false},{true,0xde0,0x4444,0,false}});
	append("MVDK-repeat-three",{0xec02,0x7192,0xde1},{{r::STATE_AR2,0xdd0}},{{r::STATE_AR2,0xdd3},{r::STATE_RC,0}},
		{{false,0xdd0,0x1111,0,false},{true,0xde1,0x1111,0,false},{false,0xdd1,0x2222,0,false},
		{true,0xde2,0x2222,0,false},{false,0xdd2,0x3333,0,false},{true,0xde3,0x3333,0,false}});
	append("LD-extended-postdecrement",{0x6f8a,0xc40},{{r::STATE_AR2,0xdf0}},
		{{r::STATE_A,0x2345},{r::STATE_AR2,0xdef}},{{false,0xdf0,0x2345,0,false}});
	append("LD-extended-absolute",{0x6ff8,0xdf0,0xc40},{},{{r::STATE_A,0x2345}},{{false,0xdf0,0x2345,0,false}});
	append("ADD-extended-preserves-B",{0x6f82,0xc01},{{r::STATE_A,10},{r::STATE_B,0x8176543210ULL},{r::STATE_AR2,0xdf0}},
		{{r::STATE_A,14},{r::STATE_B,0x8176543210ULL},{r::STATE_AR2,0xdf0}},{{false,0xdf0,2,0,false}});
	append("SUB-extended-preserves-B",{0x6f83,0xc21},{{r::STATE_A,10},{r::STATE_B,0x8176543210ULL},{r::STATE_AR3,0xdf0}},
		{{r::STATE_A,6},{r::STATE_B,0x8176543210ULL},{r::STATE_AR3,0xdf0}},{{false,0xdf0,2,0,false}});
	append("ST-extended-low-postincrement",{0x6f92,0xc80},{{r::STATE_A,0x12345678},{r::STATE_AR2,0xdf1}},
		{{r::STATE_AR2,0xdf2}},{{true,0xdf1,0x5678,0,false}});
	append("ST-extended-high-postincrement",{0x6f93,0xc60},{{r::STATE_A,0x12345678},{r::STATE_AR3,0xdf2}},
		{{r::STATE_AR3,0xdf3}},{{true,0xdf2,0x1234,0,false}});
	append("STM-AR-literal",{0x7712,0x4567},{{r::STATE_AR2,0}},{{r::STATE_AR2,0x4567}},{{true,0x12,0x4567,0,false}});
	append("PSHM-AR",{0x4a12},{{r::STATE_AR2,0x4567},{r::STATE_SP,0x300}},{{r::STATE_SP,0x2ff}},
		{{false,0x12,0x4567,0,false},{true,0x2ff,0x4567,0,false}});
	append("POPM-AR",{0x8a12},{{r::STATE_AR2,0},{r::STATE_SP,0x2ff}},{{r::STATE_AR2,0x4567},{r::STATE_SP,0x300}},
		{{false,0x2ff,0x4567,0,false},{true,0x12,0x4567,0,false}});
	append("STLM-ST1-BRAF-does-not-revive-stale-endpoint",{0x8807},{{r::STATE_A,0x8123}},{{r::STATE_ST1,0x8123}},{{true,7,0x8123,0,false}});
	append("LDM-ST1-A",{0x4807},{{r::STATE_ST1,0x8123}},{{r::STATE_A,0x8123}},{{false,7,0x8123,0,false}});
	append("PSHM-ST1",{0x4a07},{{r::STATE_ST1,0x8123},{r::STATE_SP,0x300}},{{r::STATE_SP,0x2ff}},
		{{false,7,0x8123,0,false},{true,0x2ff,0x8123,0,false}});
	append("POPM-ST1",{0x8a07},{{r::STATE_SP,0x2ff}},{{r::STATE_ST1,0x8123},{r::STATE_SP,0x300}},
		{{false,0x2ff,0x8123,0,false},{true,7,0x8123,0,false}});
	append("LDM-ST1-B-unsigned",{0x4907},{{r::STATE_ST1,0x8123}},{{r::STATE_B,0x8123}},{{false,7,0x8123,0,false}});
	return cases;
}
}
