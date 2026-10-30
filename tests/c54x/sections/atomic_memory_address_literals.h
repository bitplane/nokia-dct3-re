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
		initial.initial.insert(initial.initial.end(),{{r::STATE_BK,5},{r::STATE_ST0,0},{r::STATE_ST1,0x100}});
		initial.initial.insert(initial.initial.end(),registers.begin(),registers.end()); initial.data = data;
		auto final = observation("explicit-address-literal-result",{{STATE_GENPC,0x602 + words.size()}});
		final.expected.insert(final.expected.end(),expected.begin(),expected.end()); final.expected_data = data;
		for (auto const &word : results) {
			auto at = std::find_if(final.expected_data.begin(),final.expected_data.end(),[&](auto const &v) { return v.address == word.address; });
			if (at == final.expected_data.end()) { initial.data.push_back({word.address,0xbeef}); final.expected_data.push_back(word); }
			else at->value = word.value;
		}
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-literal-snapshot",{{STATE_GENPC,0x601}})},
			{2,observation("literal-running",{})},{64,std::move(final)}},1,64};
		c.pipeline_frontend = false; c.data_events = std::move(events); cases.push_back(std::move(c));
	};
	add("dual-move-BK5-source-wrap",{0xe5c9},{{r::STATE_AR0,1},{r::STATE_AR2,0xe04},{r::STATE_AR3,0xe10},{r::STATE_ST1,0}},
		{{r::STATE_AR0,1},{r::STATE_AR2,0xe00},{r::STATE_AR3,0xe11}},{{0xe04,0x2468},{0xe10,0}},{{0xe10,0x2468}},
		{{false,0xe04,0x2468,0,false},{true,0xe10,0x2468,0,false}});
	add("dual-move-AR4-BK5-preserves-state",{0xe5e9},
		{{r::STATE_AR0,1},{r::STATE_AR2,0xe00},{r::STATE_AR4,0xe04},{r::STATE_AR3,0xe12},{r::STATE_A,0x12345678},{r::STATE_B,0x87654321},{r::STATE_ST0,0x1800}},
		{{r::STATE_AR0,1},{r::STATE_AR2,0xe00},{r::STATE_AR4,0xe00},{r::STATE_AR3,0xe13},{r::STATE_A,0x12345678},{r::STATE_B,0x87654321},{r::STATE_ST0,0x1800}},
		{{0xe04,0x1357},{0xe12,0}},{{0xe12,0x1357}},{{false,0xe04,0x1357,0,false},{true,0xe12,0x1357,0,false}});
	add("unsigned-AR3-load-clears-guard",{0x1293},{{r::STATE_A,0xffabcdef00ULL},{r::STATE_AR3,0xef0}},
		{{r::STATE_A,0xff80},{r::STATE_AR3,0xef1}},{{0xef0,0xff80}},{},{{false,0xef0,0xff80,0,false}});
	add("byte-immediate-zero-clears-accumulator",{0xe800},{{r::STATE_A,0xff12345678ULL}},{{r::STATE_A,0}},{},{},{});
	add("absolute-double-store-high-low",{0x4ef8,0xf00},{{r::STATE_A,0xffabcd1234ULL}},{{r::STATE_A,0xffabcd1234ULL}},
		{{0xf00,0},{0xf01,0}},{{0xf00,0xabcd},{0xf01,0x1234}},{{true,0xf00,0xabcd,0,false},{true,0xf01,0x1234,0,false}});
	add("odd-double-store-decrements-two",{0x4e8b},{{r::STATE_A,0xabcd1234},{r::STATE_AR3,0xf11}},{{r::STATE_A,0xabcd1234},{r::STATE_AR3,0xf0f}},
		{{0xf10,0},{0xf11,0}},{{0xf11,0xabcd},{0xf10,0x1234}},{{true,0xf11,0xabcd,0,false},{true,0xf10,0x1234,0,false}});
	add("odd-double-load-sign-extends-high",{0x5783},{{r::STATE_AR3,0xf21}},{{r::STATE_B,0xff80015678ULL},{r::STATE_AR3,0xf21}},
		{{0xf20,0x5678},{0xf21,0x8001}},{},{{false,0xf21,0x8001,0,false},{false,0xf20,0x5678,0,false}});
	add("signed-AR2-load-postincrement",{0x1092},{{r::STATE_AR2,0xf20}},{{r::STATE_A,0xffffff8001ULL},{r::STATE_AR2,0xf21}},
		{{0xf20,0x8001}},{},{{false,0xf20,0x8001,0,false}});
	add("store-low-AR1-postincrement",{0x8091},{{r::STATE_A,0x12345678},{r::STATE_AR1,0xf20}},{{r::STATE_A,0x12345678},{r::STATE_AR1,0xf21}},
		{{0xf20,0}},{{0xf20,0x5678}},{{true,0xf20,0x5678,0,false}});
	add("signed-AR6-load-without-markers",{0x1086},{{r::STATE_AR6,0xf20}},{{r::STATE_A,0xffffffff80ULL},{r::STATE_AR6,0xf20}},
		{{0xd00,0x1234},{0xf20,0xff80}},{},{{false,0xf20,0xff80,0,false}});
	add("extended-signed-load-decrement",{0x6f8b,0xc40},{{r::STATE_AR3,0xf21}},{{r::STATE_A,0xffffffff80ULL},{r::STATE_AR3,0xf20}},
		{{0xf21,0xff80}},{},{{false,0xf21,0xff80,0,false}});
	add("subtract-signed-immediate-from-B",{0xf310,0xff80},{{r::STATE_A,0x1234},{r::STATE_B,5}},{{r::STATE_A,0x1234},{r::STATE_B,133}},{},{},{});
	add("MAR-AR2-decrements-once",{0x6d8a},{{r::STATE_AR2,0xf21},{r::STATE_ST1,0}},{{r::STATE_AR2,0xf20}},{},{},{});
	add("absolute-signed-load-B-preserves-A",{0x11f8,0xf21},{{r::STATE_A,0x1234}},{{r::STATE_A,0x1234},{r::STATE_B,0xffffffff80ULL}},
		{{0xf21,0xff80}},{},{{false,0xf21,0xff80,0,false}});
	add("subtract-signed-immediate-A-to-B",{0xf110,0xff80},{{r::STATE_A,5}},{{r::STATE_A,5},{r::STATE_B,133}},{},{},{});
	add("absolute-double-load-A-high-low",{0x56f8,0xf20},{},{{r::STATE_A,0xffff801234ULL}},
		{{0xf20,0xff80},{0xf21,0x1234}},{},{{false,0xf20,0xff80,0,false},{false,0xf21,0x1234,0,false}});
	add("repeat-fourteen-adds-fifteen-times",{0xec0e,0x0082},{{r::STATE_AR2,0xf21}},{{r::STATE_A,15},{r::STATE_AR2,0xf21}},
		{{0xf21,1}},{},std::vector<clocked_data_event>(15,{false,0xf21,1,0,false}));
	add("MVMM-AR5-to-AR3",{0xe753},{{r::STATE_AR5,0x1234}},{{r::STATE_AR3,0x1234},{r::STATE_AR5,0x1234}},{},{},
		{{false,0x15,0x1234,0,false},{true,0x13,0x1234,0,false}});
	add("MVMM-AR2-to-AR3-retains-AR5",{0xe723},{{r::STATE_AR2,0xabcd},{r::STATE_AR5,0x1234}},
		{{r::STATE_AR2,0xabcd},{r::STATE_AR3,0xabcd},{r::STATE_AR5,0x1234}},{},{},{{false,0x12,0xabcd,0,false},{true,0x13,0xabcd,0,false}});
	add("AND-high-literal-preserves-B-carry",{0xf063,0xff0},{{r::STATE_A,0xff12345678ULL},{r::STATE_B,0x1234},{r::STATE_ST0,0x800},{r::STATE_ST1,0x20}},
		{{r::STATE_A,0x2300000},{r::STATE_B,0x1234},{r::STATE_ST0,0x800,0x800}},{},{},{});
	add("store-low-B-AR4-stationary",{0x8184},{{r::STATE_B,0x12345678},{r::STATE_AR4,0xf21},{r::STATE_ST1,0}},
		{{r::STATE_B,0x12345678},{r::STATE_AR4,0xf21}},{{0xf21,0}},{{0xf21,0x5678}},{{true,0xf21,0x5678,0,false}});
	add("store-low-B-AR0-preserves-neighbor",{0x8180},
		{{r::STATE_A,0xffabcd1234ULL},{r::STATE_B,0x12345678},{r::STATE_AR0,0xf23},{r::STATE_AR4,0xf21},{r::STATE_ST0,0x1800},{r::STATE_ST1,0}},
		{{r::STATE_A,0xffabcd1234ULL},{r::STATE_B,0x12345678},{r::STATE_AR0,0xf23},{r::STATE_AR4,0xf21},{r::STATE_ST0,0x1800}},
		{{0xf23,0},{0xf24,0xcafe}},{{0xf23,0x5678}},{{true,0xf23,0x5678,0,false}});
	add("immediate-AL-store-preserves-high-guard",{0x7708,0xbeef},{{r::STATE_A,0xffabcd1234ULL},{r::STATE_B,0x12345678},{r::STATE_ST0,0x1800}},
		{{r::STATE_A,0xffabcdbeefULL},{r::STATE_B,0x12345678},{r::STATE_ST0,0x1800}},{},{},{{true,8,0xbeef,0,false}});
	add("load-high-literal-B-signed",{0xf162,0xff80},{},{{r::STATE_B,0xffff800000ULL}},{},{},{});
	add("MAR-AR6-postincrement",{0x6d96},{{r::STATE_AR6,0xf42},{r::STATE_ST1,0}},{{r::STATE_AR6,0xf43}},{},{},{});
	add("MAR-AR5-retains-AR6-and-memory",{0x6d95},{{r::STATE_AR5,0xf50},{r::STATE_AR6,0xf43},{r::STATE_ST1,0}},
		{{r::STATE_AR5,0xf51},{r::STATE_AR6,0xf43}},{{0xf50,0xbeef}},{},{});
	add("MVMM-AR1-to-AR2-preserves-source",{0xe712},{{r::STATE_AR1,0x1a23}},{{r::STATE_AR2,0x1a23},{r::STATE_AR1,0x1a23}},{},{},
		{{false,0x11,0x1a23,0,false},{true,0x12,0x1a23,0,false}});
	add("MVMM-AR1-to-AR3-preserves-source",{0xe713},{{r::STATE_AR1,0x1a23}},{{r::STATE_AR3,0x1a23},{r::STATE_AR1,0x1a23}},{},{},
		{{false,0x11,0x1a23,0,false},{true,0x13,0x1a23,0,false}});
	return cases;
}
}
