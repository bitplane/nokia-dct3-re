// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
namespace literal_tables {
inline void append_address_literals(std::vector<clocked_register_case> &cases)
{
	auto append = [&](char const *name, std::initializer_list<u16> words,
		std::initializer_list<register_setting> setup, std::initializer_list<register_check> checks,
		std::vector<clocked_data_event> events = {}) {
		auto initial = initial_case(name,{*words.begin()}); unsigned at = 0;
		for (u16 word : words) initial.program[at++].value = word;
		initial.program[at].value = 0xf4e1; initial.program.push_back({0x5ff,0xf495});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_ST0,0},{r::STATE_ST1,0},
			{r::STATE_AR0,1},{r::STATE_BK,4}});
		initial.initial.insert(initial.initial.end(),setup.begin(),setup.end());
		auto final = observation(name,checks);
		final.expected.push_back({STATE_GENPC,0x601 + unsigned(words.size())});
		for (auto const &e : events) { initial.data.push_back({e.address,e.value}); final.expected_data.push_back({e.address,e.value}); }
		auto c = literal_sequence(std::move(initial), observation("pre-address-operation", {{STATE_GENPC,0x5ff}}), std::move(final), 16, 1);
		c.data_events = std::move(events); cases.push_back(std::move(c));
	};
	append("MAR-signed-extension",{0x6ded,0xfff9},{{r::STATE_AR5,0x1000}},{{r::STATE_AR5,0xff9}});
	append("MAR-circular-AR0-no-extension",{0x6ddc},{{r::STATE_AR4,0xc03}},{{r::STATE_AR4,0xc00},{r::STATE_AR0,1},{r::STATE_BK,4}});
	append("SUB-memory-shift16",{0x4092},{{r::STATE_AR2,0xd00},{r::STATE_A,0x50000},{r::STATE_B,0x1234}},
		{{r::STATE_A,0x20000},{r::STATE_B,0x1234},{r::STATE_AR2,0xd01}},{{false,0xd00,3,0,false}});
	append("DLD-sign-extends-B-stationary",{0x5781},{{r::STATE_AR1,0xe00},{r::STATE_A,0x1234},{r::STATE_ST1,0x100}},
		{{r::STATE_B,0xff80012345ULL},{r::STATE_A,0x1234},{r::STATE_AR1,0xe00}},
		{{false,0xe00,0x8001,0,false},{false,0xe01,0x2345,0,false}});
}

inline void append_stack_literals(std::vector<clocked_register_case> &cases)
{
	auto append = [&](char const *name, std::initializer_list<u16> words,
		std::initializer_list<register_setting> setup, std::initializer_list<memory_word> data,
		std::initializer_list<register_check> checks, std::initializer_list<memory_word> memory,
		std::vector<clocked_data_event> events) {
		auto initial = initial_case(name,{*words.begin()}); unsigned at = 0;
		for (u16 word : words) initial.program[at++].value = word;
		initial.program[at].value = 0xf4e1; initial.program.push_back({0x5ff,0xf495});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_SP,0x300}});
		initial.initial.insert(initial.initial.end(),setup.begin(),setup.end()); initial.data = data;
		auto final = observation(name,checks,memory); final.expected.push_back({STATE_GENPC,0x601 + unsigned(words.size())});
		auto c = literal_sequence(std::move(initial), observation("pre-stack-snapshot", {{STATE_GENPC,0x5ff}}), std::move(final), 32, 1);
		c.data_events = std::move(events); cases.push_back(std::move(c));
	};
	append("SSBX-INTM-and-PSHM-AL",{0xf7bb,0x4a08},{{r::STATE_A,0x12345678},{r::STATE_ST1,0}},{{0x2ff,0}},
		{{r::STATE_A,0x12345678},{r::STATE_SP,0x2ff},{r::STATE_ST1,0x800}},{{0x2ff,0x5678}},
		{{false,8,0x5678,0,false},{true,0x2ff,0x5678,0,false}});
	append("POPM-AL-preserves-upper-fields",{0x8a08},{{r::STATE_A,0x12340000},{r::STATE_SP,0x2ff}},{{0x2ff,0x5678}},
		{{r::STATE_A,0x12345678},{r::STATE_SP,0x300}},{{0x2ff,0x5678}},
		{{false,0x2ff,0x5678,0,false},{true,8,0x5678,0,false}});
	append("PSHM-page-zero-pair",{0x4a60,0x4a61},{{r::STATE_A,0x12345678}},{{0x60,0x1234},{0x61,0xabcd},{0x2ff,0},{0x2fe,0}},
		{{r::STATE_A,0x12345678},{r::STATE_SP,0x2fe}},{{0x60,0x1234},{0x61,0xabcd},{0x2ff,0x1234},{0x2fe,0xabcd}},
		{{false,0x60,0x1234,6},{false,0x61,0xabcd,7},{true,0x2ff,0x1234,7},{true,0x2fe,0xabcd,8}});
	append("POPM-page-zero-reverse-pair",{0x8a61,0x8a60},{{r::STATE_A,0x12345678},{r::STATE_SP,0x2fe}},
		{{0x60,0},{0x61,0},{0x2fe,0xabcd},{0x2ff,0x1234}},{{r::STATE_A,0x12345678},{r::STATE_SP,0x300}},
		{{0x60,0x1234},{0x61,0xabcd}},{{false,0x2fe,0xabcd,6},{false,0x2ff,0x1234,7},
		{true,0x61,0xabcd,7},{true,0x60,0x1234,8}});
	append("RSBX-INTM",{0xf6bb},{{r::STATE_ST1,0x800}},{},{{r::STATE_ST1,0},{r::STATE_SP,0x300}},{},{});
	append("PSHM-AH-then-eight-bit-AG",{0x4a09,0x4a0a},{{r::STATE_A,0x123456789a}},{{0x2ff,0},{0x2fe,0}},
		{{r::STATE_A,0x123456789a},{r::STATE_SP,0x2fe}},{{0x2ff,0x3456},{0x2fe,0x12}},
		{{false,9,0x3456,6},{false,10,0x12,7},{true,0x2ff,0x3456,7},{true,0x2fe,0x12,8}});
	append("POPM-AG-AH-retains-AL",{0x8a0a,0x8a09},{{r::STATE_A,0xabcd987654},{r::STATE_SP,0x2fe}},
		{{0x2fe,0x12},{0x2ff,0x3456}},{{r::STATE_A,0x1234567654},{r::STATE_SP,0x300}},{},
		{{false,0x2fe,0x12,6},{false,0x2ff,0x3456,7},{true,10,0x12,7},{true,9,0x3456,8}});
	append("PSHM-BL-BH-BG",{0x4a0b,0x4a0c,0x4a0d},{{r::STATE_B,0x7f1234abcd}},{{0x2ff,0},{0x2fe,0},{0x2fd,0}},
		{{r::STATE_B,0x7f1234abcd},{r::STATE_SP,0x2fd}},{{0x2ff,0xabcd},{0x2fe,0x1234},{0x2fd,0x7f}},
		{{false,11,0xabcd,6},{false,12,0x1234,7},{true,0x2ff,0xabcd,7},
		{false,13,0x7f,8},{true,0x2fe,0x1234,8},{true,0x2fd,0x7f,9}});
	append("POPM-BG-BH-BL-preserves-A",{0x8a0d,0x8a0c,0x8a0b},
		{{r::STATE_B,0x5511223344},{r::STATE_A,0x1234567654},{r::STATE_SP,0x2fd}},{{0x2fd,0x7f},{0x2fe,0x1234},{0x2ff,0xabcd}},
		{{r::STATE_B,0x7f1234abcd},{r::STATE_A,0x1234567654},{r::STATE_SP,0x300}},{},
		{{false,0x2fd,0x7f,6},{false,0x2fe,0x1234,7},{true,13,0x7f,7},
		{false,0x2ff,0xabcd,8},{true,12,0x1234,8},{true,11,0xabcd,9}});
}

inline void append_move_modify_literals(std::vector<clocked_register_case> &cases)
{
	for (unsigned variant = 0; variant != 9; ++variant) {
		std::vector<u16> words{0xf495};
		switch (variant) {
		case 0: words.insert(words.end(),{0x730b,0xfa0}); break;
		case 1: words.insert(words.end(),{0x6882,0xf0f}); break;
		case 2: words.insert(words.end(),{0x6884,0xff0f}); break;
		case 3: words.push_back(0x6db1); break;
		case 4: words.push_back(0x6dc2); break;
		case 5: words.push_back(0xe900); break;
		case 6: words.push_back(0xe901); break;
		case 7: words.insert(words.end(),{0xec1f,0x7693,0x5a3c}); break;
		case 8: words.insert(words.end(),{0x7690,0xbeef}); break;
		}
		words.push_back(0xf4e1);
		auto initial = initial_case("MVMD-ANDM-MAR-LD-RPT-ST-literals", {0xf495}); initial.program.clear();
		for (unsigned i = 0; i != words.size(); ++i) initial.program.push_back({0x600 + i,words[i]});
		u64 const b = variant == 0 ? 0x1234 : variant == 5 || variant == 6 ? 0xffffffffffULL : 0x22222222;
		u16 const st1 = variant == 3 || variant == 4 ? 0 : 0x100;
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,0x11111111},{r::STATE_B,b},{r::STATE_T,0x3333},
			{r::STATE_AR0,variant == 8 ? 0xf60U : 3U},{r::STATE_AR1,0xf10},
			{r::STATE_AR2,variant == 1 ? 0xf90U : 0xf08U},{r::STATE_AR3,variant == 8 ? 0xd20U : 0xd00U},
			{r::STATE_AR4,0xf92},{r::STATE_BK,4},{r::STATE_ST0,0x800},{r::STATE_ST1,st1}});
		auto final = observation("literal-memory-state-and-selected-pointer", {{STATE_GENPC,0x600 + words.size()},
			{r::STATE_A,0x11111111},{r::STATE_B,variant == 5 ? 0U : variant == 6 ? 1U : b},{r::STATE_T,0x3333},
			{r::STATE_ST0,0x800},{r::STATE_ST1,st1},{r::STATE_BK,4},
			{r::STATE_AR0,variant == 8 ? 0xf61U : 3U},{r::STATE_AR1,variant == 3 ? 0xf13U : 0xf10U},
			{r::STATE_AR2,variant == 1 ? 0xf90U : variant == 4 ? 0xf0bU : 0xf08U},
			{r::STATE_AR3,variant >= 7 ? 0xd20U : 0xd00U},{r::STATE_AR4,0xf92}});
		std::vector<clocked_data_event> events;
		if (variant == 0) {
			initial.data = {{0xfa0,0}}; final.expected_data = {{0xfa0,0x1234}};
			events = {{false,0xb,0x1234,0,false},{true,0xfa0,0x1234,0,false}};
		} else if (variant == 1 || variant == 2) {
			u16 const address = variant == 1 ? 0xf90 : 0xf92, before = variant == 1 ? 0xff00 : 0x1234, after = variant == 1 ? 0xf00 : 0x1204;
			initial.data = {{address,before}}; final.expected_data = {{address,after}};
			events = {{false,address,before,0,false},{true,address,after,0,false}};
		} else if (variant == 7) {
			for (unsigned i = 0; i != 32; ++i) {
				initial.data.push_back({0xd00 + i,0}); final.expected_data.push_back({0xd00 + i,0x5a3c});
				events.push_back({true,0xd00 + i,0x5a3c,0,false});
			}
		} else if (variant == 8) {
			initial.data = {{0xf5f,0x1234},{0xf60,0},{0xf61,0xabcd}};
			final.expected_data = {{0xf5f,0x1234},{0xf60,0xbeef},{0xf61,0xabcd}};
			events = {{true,0xf60,0xbeef,0,false}};
		}
		auto c = literal_sequence(std::move(initial), observation("pre-operation-snapshot", {{STATE_GENPC,0x600}}), std::move(final), 128, 1);
		c.data_events = std::move(events); cases.push_back(std::move(c));
	}
}

inline void append_mmr_transfer_literals(std::vector<clocked_register_case> &cases)
{
	for (unsigned variant = 0; variant != 4; ++variant) {
		std::vector<u16> const words = variant == 0 ? std::vector<u16>{0x45f8,0xf24} :
			variant == 1 ? std::vector<u16>{0x4816,0x4913} : variant == 2 ?
			std::vector<u16>{0x7215,0xf60,0x7314,0xf62,0x7315,0xf63} :
			std::vector<u16>{0x8816,0x8817,0x8821,0x8910,0x8911,0x4920};
		auto initial = initial_case("high-load-and-mixed-MMR-transfer-literals", {0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1 + i].value = words[i];
		initial.program[1 + words.size()].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,variant == 3 ? 0x1234U : 0U},{r::STATE_B,variant == 3 ? 0x5678U : 0U},
			{r::STATE_ST0,0x800},{r::STATE_ST1,0x100},{r::STATE_AR0,0},{r::STATE_AR1,0},
			{r::STATE_AR3,0x8003},{r::STATE_AR4,0xabcd},{r::STATE_AR5,0},{r::STATE_AR6,0x8006},{r::STATE_AR7,0}});
		auto final = observation("fixed-results-and-unmodified-source-registers", {{STATE_GENPC,0x602 + words.size()},
			{r::STATE_A,variant == 1 ? 0x8006U : variant == 3 ? 0x1234U : 0U},
			{r::STATE_B,variant == 0 ? 0xff80010000ULL : variant == 1 ? 0x8003 : variant == 3 ? 0x9abc : 0},
			{r::STATE_ST0,0x800},{r::STATE_ST1,0x100},{r::STATE_AR3,0x8003},{r::STATE_AR4,0xabcd},
			{r::STATE_AR5,variant == 2 ? 0x4321U : 0U},{r::STATE_AR6,variant == 3 ? 0x1234U : 0x8006U},
			{r::STATE_AR7,variant == 3 ? 0x1234U : 0U},{r::STATE_AR0,variant == 3 ? 0x5678U : 0U},{r::STATE_AR1,variant == 3 ? 0x5678U : 0U}});
		std::vector<clocked_data_event> events;
		if (variant == 0) {
			initial.data = {{0xf24,0x8001}}; final.expected_data = initial.data;
			events = {{false,0xf24,0x8001,0,false}};
		} else if (variant == 1) events = {{false,0x16,0x8006,0,false},{false,0x13,0x8003,0,false}};
		else if (variant == 2) {
			initial.data = {{0xf60,0x4321},{0xf62,0},{0xf63,0}}; final.expected_data = {{0xf60,0x4321},{0xf62,0xabcd},{0xf63,0x4321}};
			events = {{false,0xf60,0x4321,0,false},{true,0x15,0x4321,0,false},{false,0x14,0xabcd,0,false},
				{true,0xf62,0xabcd,0,false},{false,0x15,0x4321,0,false},{true,0xf63,0x4321,0,false}};
		} else {
			initial.data = {{0x20,0x9abc},{0x21,0}}; final.expected_data = {{0x20,0x9abc},{0x21,0x1234}};
			events = {{true,0x16,0x1234,0,false},{true,0x17,0x1234,0,false},{true,0x21,0x1234,0,false},
				{true,0x10,0x5678,0,false},{false,0x20,0x9abc,13},{true,0x11,0x5678,13}};
		}
		auto c = literal_sequence(std::move(initial), observation("pre-transfer-snapshot", {{STATE_GENPC,0x600}}), std::move(final), 48, 1);
		c.data_events = std::move(events); cases.push_back(std::move(c));
	}
}

inline void append_transfer_literals(std::vector<clocked_register_case> &cases)
{
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
		auto c = literal_sequence(std::move(initial), observation("pre-transfer-snapshot", {{STATE_GENPC,0x5ff}}), std::move(final), 48, 1);
		c.data_events = std::move(events); cases.push_back(std::move(c));
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
		{{false,0xd90,0x1111,0,false},{false,0xd91,0x2222,0,false,{},1U},{true,0xdb0,0x1111,0,false,{},0U},
		{false,0xd92,0x3333,0,false,{},1U},{true,0xdb1,0x2222,0,false,{},0U},{true,0xdb2,0x3333,0,false,{},1U}});
	append("LD-high-absolute",{0x44f8,0xdc0},{},{{r::STATE_A,0x12340000}},{{false,0xdc0,0x1234,0,false}});
	append("ST-low-absolute",{0x80f8,0xdc1},{{r::STATE_A,0x12345678}},{},{{true,0xdc1,0x5678,0,false}});
	append("ST-high-absolute",{0x82f8,0xdc2},{{r::STATE_A,0x12345678}},{},{{true,0xdc2,0x1234,0,false}});
	append("ST-T-absolute",{0x8cf8,0xdc3},{{r::STATE_T,0x4321}},{},{{true,0xdc3,0x4321,0,false}});
	append("MVDK-absolute",{0x71f8,0xdd0,0xde0},{},{},{{false,0xdd0,0x4444,0,false},{true,0xde0,0x4444,0,false}});
	append("MVDK-repeat-three",{0xec02,0x7192,0xde1},{{r::STATE_AR2,0xdd0}},{{r::STATE_AR2,0xdd3},{r::STATE_RC,0}},
		{{false,0xdd0,0x1111,0,false},{false,0xdd1,0x2222,0,false,{},1U},{true,0xde1,0x1111,0,false,{},0U},
		{false,0xdd2,0x3333,0,false,{},1U},{true,0xde2,0x2222,0,false,{},0U},{true,0xde3,0x3333,0,false,{},1U}});
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
}
}
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (auto append : {
		literal_tables::append_address_literals,
		literal_tables::append_stack_literals,
		literal_tables::append_move_modify_literals,
		literal_tables::append_mmr_transfer_literals,
		literal_tables::append_transfer_literals}) append(cases);
	return cases;
}
}
