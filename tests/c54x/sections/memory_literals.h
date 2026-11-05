// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
namespace literal_tables {
inline void append_memory_literals(std::vector<clocked_register_case> &cases)
{
	auto append = [&](char const *name, std::initializer_list<u16> words,
		std::initializer_list<register_setting> setup, std::initializer_list<register_check> checks,
		std::vector<clocked_data_event> events = {}, std::initializer_list<memory_word> decoys = {}) {
		auto initial = initial_case(name,{*words.begin()}); unsigned at = 0;
		for (u16 word : words) initial.program[at++].value = word;
		initial.program[at].value = 0xf4e1; initial.program.push_back({0x5ff,0xf495});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_ST0,0},{r::STATE_ST1,0x100},
			{r::STATE_T,0},{r::STATE_AR2,0xf00},{r::STATE_AR3,0xf90},{r::STATE_AR4,0xf91},{r::STATE_AR5,0xf90}});
		initial.initial.insert(initial.initial.end(),setup.begin(),setup.end());
		initial.data = decoys;
		auto final = observation(name,checks,decoys);
		final.expected.push_back({STATE_GENPC,0x601 + unsigned(words.size())});
		for (auto const &e : events) if (e.write) {
			initial.data.push_back({e.address,0});
			final.expected_data.push_back({e.address,e.value});
		}
		// A read-modify-write source must start with its read operand, not the destination poison.
		for (auto const &e : events) if (!e.write) {
			initial.data.push_back({e.address,e.value});
			if (std::none_of(events.begin(),events.end(),[&](auto const &other) { return other.write && other.address == e.address; }))
				final.expected_data.push_back({e.address,e.value});
		}
		auto c = literal_sequence(std::move(initial), observation("pre-memory-snapshot", {{STATE_GENPC,0x5ff}}), std::move(final), 32, 1);
		c.data_events = std::move(events); cases.push_back(std::move(c));
	};
	for (unsigned fractional = 0; fractional != 2; ++fractional)
		append("ABDST-old-A-high",{0xe33a},{{r::STATE_A,0xffabcd0000ULL},{r::STATE_T,0x4444},
			{r::STATE_ST1,fractional ? 0x140U : 0x100U}},
			{{r::STATE_A,fractional ? 0xfffffb0000ULL : 0xffffab0000ULL},{r::STATE_B,fractional ? 0xa866U : 0x5433U},
			{r::STATE_T,0x4444},{r::STATE_AR4,0xf92},{r::STATE_AR5,0xf90}},
			{{false,0xf90,u16(fractional ? 0xfffe : 0x55),0,false},{false,0xf91,u16(fractional ? 3 : 0xaa),0,false}});
	for (unsigned form = 0; form != 5; ++form) {
		u16 const opcode = std::array<u16,5>{0x4c83,0x4c93,0x4cf8,0x4cea,0x4c45}[form];
		u32 const source = form == 3 ? 0xf05 : form == 4 ? 0xf45 : 0xf90;
		u16 const value = form == 0 || form == 2 ? 0x6cac : form == 4 ? 0x5678 : 0x1234;
		append("LTD-source-successor-and-address-update",form < 2 || form == 4 ? std::initializer_list<u16>{opcode} :
			std::initializer_list<u16>{opcode,u16(form == 2 ? 0xf90 : 5)},
			{{r::STATE_ST1,form == 4 ? 0x4100U : 0x100U},{r::STATE_SP,0xf00}},
			{{r::STATE_T,value},{r::STATE_AR3,form == 1 ? 0xf91U : 0xf90U},
			{r::STATE_AR2,form == 3 ? 0xf05U : 0xf00U},{r::STATE_AR5,0xf90},{r::STATE_SP,0xf00}},
			{{false,source,value,0,false},{true,source + 1,value,0,false}});
	}
	append("LTD-younger-Read-wins-older-STLM-T",{0x880e,0x4c83},{{r::STATE_A,0x1234},{r::STATE_T,0x4444}},
		{{r::STATE_A,0x1234},{r::STATE_T,0x6cac},{r::STATE_AR3,0xf90}},
		{{false,0xf90,0x6cac,7},{true,0xe,0x1234,7},{true,0xf91,0x6cac,8}});
	cases.back().checkpoint = 7;
	cases.back().states.insert(cases.back().states.end()-1,
		{7,observation("LTD-Read-publishes-T-after-older-STLM",{{r::STATE_T,0x6cac}})});
	// STLM's T write is intermediate; the younger LTD owns final T.
	auto &memory = cases.back().states.back().observation.expected_data;
	memory.erase(std::remove_if(memory.begin(),memory.end(),[](auto const &word) { return word.address == 0xe; }),memory.end());
	for (unsigned logical = 0; logical != 2; ++logical)
		append("SUB-half-arithmetic-versus-logical",{0xf43f},{{r::STATE_A,0xfffffffffcULL},{r::STATE_ST1,logical ? 0U : 0x100U}},
			{{r::STATE_A,logical ? 0x7ffffffffeULL : 0xfffffffffeULL}});
	append("XOR-long-shift16-other",{0xf165,0xff},{{r::STATE_A,0xff12340000ULL},{r::STATE_B,0x1234},{r::STATE_ST0,0x800}},
		{{r::STATE_A,0xff12340000ULL},{r::STATE_B,0xff12cb0000ULL},{r::STATE_ST0,0x800,0x800}});
	for (unsigned below = 0; below != 2; ++below)
		append("MACR-negative-half-boundary",{0xb43a},{{r::STATE_A,below ? 0xffffff7fffULL : 0xffffff8000ULL},
			{r::STATE_B,0x1234},{r::STATE_ST0,0x800}},{{r::STATE_A,below ? 0xffffff0000ULL : 0ULL},
			{r::STATE_B,0x1234},{r::STATE_T,0},{r::STATE_AR4,0xf92},{r::STATE_ST0,0x800,0x800}},
			{{false,0xf90,0,0,false},{false,0xf91,1,0,false}});
	for (unsigned cpl = 0; cpl != 2; ++cpl) {
		u32 const source = cpl ? 0xf45 : 0x945; u16 const value = cpl ? 0x5678 : 0x1234;
		append("direct-load-store-DP-versus-SP",{0x1045,0x8046},{{r::STATE_ST0,0x12},
			{r::STATE_ST1,cpl ? 0x4100U : 0x100U},{r::STATE_SP,0xf00}},
			{{r::STATE_A,value},{r::STATE_AR5,0xf90},{r::STATE_SP,0xf00}},
			{{false,source,value,0,false},{true,source + 1,value,0,false}},{{0xf90,0xabcd},{cpl ? 0x946U : 0xf46U,0xbeef}});
	}
	append("ADDM-direct-DP",{0x6b45,1},{{r::STATE_ST0,0x12}},{{r::STATE_AR5,0xf90}},
		{{false,0x945,0x1234,0,false},{true,0x945,0x1235,0,false}});
	append("MPY-direct-DP",{0x2045},{{r::STATE_ST0,0x12},{r::STATE_T,3}},{{r::STATE_A,12},{r::STATE_AR5,0xf90}},
		{{false,0x945,4,0,false}});
	append("MAC-direct-SP",{0x2945},{{r::STATE_ST0,0x12},{r::STATE_ST1,0x4100},{r::STATE_SP,0xf00},
		{r::STATE_A,12},{r::STATE_B,16},{r::STATE_T,3}},{{r::STATE_B,4},{r::STATE_A,12},{r::STATE_AR5,0xf90},{r::STATE_SP,0xf00}},
		{{false,0xf45,0xfffc,0,false}});
	append("SQUR-offset-preincrement",{0x26ea,5},{},{{r::STATE_A,9},{r::STATE_T,0xfffd},{r::STATE_AR2,0xf05}},
		{{false,0xf05,0xfffd,0,false}});
	append("SQUR-direct-B",{0x271e},{{r::STATE_ST0,6},{r::STATE_T,3},{r::STATE_B,0x1f4}},
		{{r::STATE_B,0xe1},{r::STATE_T,0xf}},{{false,0x31e,0xf,0,false}});
	append("SQURA-direct-B",{0x391e},{{r::STATE_ST0,6},{r::STATE_T,3},{r::STATE_B,0x3200000}},
		{{r::STATE_B,0x32000e1},{r::STATE_T,0xf}},{{false,0x31e,0xf,0,false}});
	append("SQURS-direct-A",{0x3a09},{{r::STATE_ST0,6},{r::STATE_A,0x14b5db0},{r::STATE_T,0x8765}},
		{{r::STATE_A,0x320},{r::STATE_T,0x1234}},{{false,0x309,0x1234,0,false}});
	append("ADDC-direct-unsigned-SXM",{0x0605},{{r::STATE_ST0,0x808}},{{r::STATE_A,0x10000},{r::STATE_ST0,0,0x800}},
		{{false,0x405,0xffff,0,false}});
	append("SUBB-direct-inverted-carry",{0x0e05},{{r::STATE_ST0,8},{r::STATE_A,6}},
		{{r::STATE_A,0xffffffffffULL},{r::STATE_ST0,0,0x800}},{{false,0x405,6,0,false}});
	append("ADDM-negative-saturation",{0x6b94,0xfff8},{{r::STATE_AR4,0x100},{r::STATE_ST1,0x300}},
		{{r::STATE_AR4,0x101},{r::STATE_ST0,0x400,0x400}},{{false,0x100,0x8007,0,false},{true,0x100,0x8000,0,false}});
}

inline void append_memory_alu_literals(std::vector<clocked_register_case> &cases)
{
	struct row { u16 opcode,address,operand,st1; u64 a,b,out_a,out_b; u16 flags; bool increment; };
	constexpr row rows[]{
		{0x0882,0xf22,0x8001,0x100,0x10000,0x12345678,0x17fff,0x12345678,0,false},
		{0x0881,0xf21,0x8001,0x100,0x10000,0x12345678,0x17fff,0x12345678,0,false},
		{0x0881,0xf21,0x8001,0,0x10000,0x12345678,0x7fff,0x12345678,0x800,false},
		{0x0092,0xf22,0xffff,0x100,1,0x12345678,0,0x12345678,0x800,true},
		{0x0092,0xf22,0xffff,0,1,0x12345678,0x10000,0x12345678,0,true},
		{0x0192,0xf22,0xffff,0x100,0x12345678,1,0x12345678,0,0x800,true},
		{0x0192,0xf22,0xffff,0,0x12345678,1,0x12345678,0x10000,0,true},
		{0x1b84,0xf24,0xf0,0x100,0x7fff,0xff0000000fULL,0x7fff,0xff000000ffULL,0x800,false},
		{0x1bf8,0xf24,0xf0,0x100,0x7fff,0xff0000000fULL,0x7fff,0xff000000ffULL,0x800,false}
	};
	for (auto const &v : rows) {
		bool const extended = v.opcode == 0x1bf8;
		auto initial = initial_case("memory-SUB-ADD-OR-SXM-and-pointer-literals", {0xf495,v.opcode});
		if (extended) initial.program[2].value = v.address;
		initial.program[2 + extended].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,v.a},{r::STATE_B,v.b},{r::STATE_AR1,0xf21},
			{r::STATE_AR2,0xf22},{r::STATE_AR4,0xf24},{r::STATE_ST0,v.opcode == 0x1b84 || extended ? 0x800U : 0U},{r::STATE_ST1,v.st1}});
		initial.data = {{0xf21,0x8001},{0xf22,0x1234},{0xf24,0xf0}};
		for (auto &word : initial.data) if (word.address == v.address) word.value = v.operand;
		auto final = observation("signedness-carry-pointer-and-unrelated-source", {{STATE_GENPC,0x603 + unsigned(extended)},
			{r::STATE_A,v.out_a},{r::STATE_B,v.out_b},{r::STATE_AR1,0xf21},{r::STATE_AR2,v.increment ? 0xf23U : 0xf22U},
			{r::STATE_AR4,0xf24},{r::STATE_ST0,v.flags},{r::STATE_ST1,v.st1}});
		final.expected_data = initial.data;
		auto c = literal_sequence(std::move(initial), observation("pre-ALU-snapshot", {{STATE_GENPC,0x600}}), std::move(final), 32, 1);
		c.data_events = std::vector<clocked_data_event>{{false,v.address,v.operand,0,false}};
		cases.push_back(std::move(c));
	}
}

inline void append_memory_address_literals(std::vector<clocked_register_case> &cases)
{
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
		auto c = literal_sequence(std::move(initial), observation("pre-literal-snapshot",{{STATE_GENPC,0x600}}), std::move(final), 64, 1);
		c.data_events = std::move(events); cases.push_back(std::move(c));
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
}

inline void append_memory_route_literals(std::vector<clocked_register_case> &cases)
{
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
		auto final = observation("memory-route-result-and-marker-interval",{});
		final.expected.insert(final.expected.end(),expected.begin(),expected.end());
		final.expected.push_back({STATE_GENPC,0x601+program.size()});
		final.expected_data = std::move(expected_data);
		auto c = literal_sequence(std::move(initial), observation("before-route-markers",{{STATE_GENPC,0x600}}), std::move(final), 48, 3);
		c.observed_data_space = AS_IO;
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},{true,0x124,0x1111,0,false,{},interval}};
		cases.push_back(std::move(c));
	};
		add("bitf-long-preupdate-retains-carry", {0x61ea, 5, 0x8000}, {{r::STATE_AR2, 0x0f00}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_ST0, 0x1800, 0x1800}, {r::STATE_AR2, 0x0f05}}, {{0x0f05, 0x8001}}, 6);
		add("cmpm-offset-before-immediate", {0x60e2, 5, 0x1235}, {{r::STATE_AR2, 0x0f00}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_ST0, 0x0800, 0x1800}, {r::STATE_AR2, 0x0f00}}, {{0x0f05, 0x1234}}, 6);
		add("bitf-ar0-absent-bit", {0x6180, 1}, {{r::STATE_AR0, 0x0f20}, {r::STATE_ST0, 0x1800}},
			{{r::STATE_ST0, 0x0800, 0x1800}, {r::STATE_AR0, 0x0f20}}, {{0x0f20, 4}});
		add("mvmm-ar3-to-ar6", {0xe736}, {{r::STATE_AR3, 0x1357}, {r::STATE_AR6, 0}},
			{{r::STATE_AR6, 0x1357}, {r::STATE_AR3, 0x1357}}, {}, 4);
		add("push-long-preupdated-source", {0x4bea, 5}, {{r::STATE_ST1, 0}, {r::STATE_SP, 0x0300}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_SP, 0x02ff}, {r::STATE_AR2, 0x0f05}}, {{0x0f05, 0x1234}, {0x02ff, 0}}, 5, {{0x02ff, 0x1234}});
		add("pop-long-preupdated-destination", {0x8bea, 5}, {{r::STATE_SP, 0x02ff}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_SP, 0x0300}, {r::STATE_AR2, 0x0f05}}, {{0x02ff, 0xbeef}, {0x0f05, 0}}, 5, {{0x0f05, 0xbeef}});
		add("double-store-b-long-high-low", {0x4fea, 6}, {{r::STATE_B, 0x12345678}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_AR2, 0x0f06}}, {{0x0f06, 0}, {0x0f07, 0}}, 6, {{0x0f06, 0x1234}, {0x0f07, 0x5678}});
		add("double-load-b-long-signed", {0x57e2, 6}, {{r::STATE_B, 0}, {r::STATE_AR2, 0x0f00}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_B, 0xff80012345ULL}, {r::STATE_AR2, 0x0f00}}, {{0x0f06, 0x8001}, {0x0f07, 0x2345}});
		add("sub-high-long-preupdated-source", {0x40ea, 5}, {{r::STATE_A, 0x30000}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_A, 0x20000}, {r::STATE_AR2, 0x0f05}}, {{0x0f05, 1}});
		add("bitt-long-stationary-source", {0x34e2, 5}, {{r::STATE_T, 0}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_ST0, 0x1000, 0x1000}, {r::STATE_AR2, 0x0f00}}, {{0x0f05, 0x8000}});
		add("multiply-long-preupdated-source", {0x20ea, 5}, {{r::STATE_T, 2}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_A, 6}, {r::STATE_AR2, 0x0f05}}, {{0x0f05, 3}});
		add("load-ar2-unsigned-with-sxm", {0x1282}, {{r::STATE_A, 0}, {r::STATE_AR2, 0x0f00}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 0x8001}, {r::STATE_AR2, 0x0f00}}, {{0x0f00, 0x8001}}, 4);
		add("load-ar2-signed-same-source", {0x1082}, {{r::STATE_A, 0}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_A, 0xffffff8001ULL}, {r::STATE_AR2, 0x0f00}}, {{0x0f00, 0x8001}}, 4);
		add("store-bl-to-ar4", {0x8914}, {{r::STATE_B, 0x5678cafe}, {r::STATE_AR4, 0}},
			{{r::STATE_AR4, 0xcafe}}, {}, 4);
		add("mvdk-offset-before-destination", {0x71ea, 5, 0x0f20}, {{r::STATE_AR2, 0x0f00}},
			{{r::STATE_AR2, 0x0f05}}, {{0x0f05, 0xbeef}, {0x0f20, 0}}, 6, {{0x0f20, 0xbeef}});
		add("mvkd-offset-before-source", {0x70ea, 5, 0x0f20}, {{r::STATE_AR2, 0x0f00}},
			{{r::STATE_AR2, 0x0f05}}, {{0x0f20, 0x5678}, {0x0f05, 0}}, 6, {{0x0f05, 0x5678}});
		add("reada-long-preupdated-destination", {0x7eea, 5}, {{r::STATE_A, 0x9000}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_AR2, 0x0f05}}, {{0x0f05, 0}}, 9, {{0x0f05, 0xabcd}});
		cases.back().states.front().observation.program.emplace_back(0x9000, 0xabcd);
		add("mvpd-offset-before-program-address", {0x7cea, 5, 0x9000}, {{r::STATE_AR2, 0x0f00}},
			{{r::STATE_AR2, 0x0f05}}, {{0x0f05, 0}}, 7, {{0x0f05, 0xbeef}});
		cases.back().states.front().observation.program.emplace_back(0x9000, 0xbeef);
		add("load-shift-zero-offset-before-operation", {0x6fea, 5, 0x0c40}, {{r::STATE_A, 0}, {r::STATE_AR2, 0x0f00}},
			{{r::STATE_A, 0x2345}, {r::STATE_AR2, 0x0f05}}, {{0x0f05, 0x2345}}, 6);
		add("mvdd-ar2-to-ar5-postincrement", {0xe58b}, {{r::STATE_AR2, 0x0d20}, {r::STATE_AR5, 0x0e20}},
			{{r::STATE_AR2, 0x0d21}, {r::STATE_AR5, 0x0e21}}, {{0x0d20, 0x4321}, {0x0e20, 0}}, 4, {{0x0e20, 0x4321}});
		add("ldu-b-ar2-circular-wrap", {0x13d2}, {{r::STATE_BK, 4}, {r::STATE_B, 0}, {r::STATE_AR2, 0x0f23}},
			{{r::STATE_B, 0x8001}, {r::STATE_AR2, 0x0f20}}, {{0x0f23, 0x8001}}, 4);
		add("and-ar2-stationary-source", {0x1882}, {{r::STATE_A, 0x1234}, {r::STATE_AR2, 0x0f20}},
			{{r::STATE_A, 0x0204}, {r::STATE_AR2, 0x0f20}}, {{0x0f20, 0x0f0f}}, 4);
		add("load-ar1-signed-stationary-source", {0x1081}, {{r::STATE_A, 0}, {r::STATE_AR1, 0x0f20}, {r::STATE_ST1, 0x0100}},
			{{r::STATE_A, 0xffffff8001ULL}, {r::STATE_AR1, 0x0f20}}, {{0x0f20, 0x8001}}, 4);
		add("frame-increment-crosses-page", {0xee01}, {{r::STATE_ST1, 0x0100}, {r::STATE_SP, 0x03ff}}, {{r::STATE_SP, 0x0400}}, {}, 4);
		add("or-immediate-b-retains-upper-bits", {0xf340, 0x00f0}, {{r::STATE_B, 0xab00123400ULL}},
			{{r::STATE_B, 0xab001234f0ULL}});
		add("and-immediate-b-to-a-retains-b", {0xf230, 0x0ff0}, {{r::STATE_A, 0x123456}, {r::STATE_B, 0xffabcdf123ULL}},
			{{r::STATE_A, 0x0120}, {r::STATE_B, 0xffabcdf123ULL}});

}

inline void append_absolute_literals(std::vector<clocked_register_case> &cases)
{
	auto append = [&](char const *name,std::initializer_list<u16> words,std::initializer_list<register_setting> setup,
		std::initializer_list<register_check> checks,std::vector<clocked_data_event> events = {}) {
		auto initial = initial_case(name,{*words.begin()}); unsigned at = 0;
		for (u16 word : words) initial.program[at++].value = word;
		initial.program[at].value = 0xf4e1; initial.program.push_back({0x5ff,0xf495});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_ST0,0},{r::STATE_ST1,0x100},{r::STATE_T,0}});
		initial.initial.insert(initial.initial.end(),setup.begin(),setup.end());
		auto final = observation(name,checks); final.expected.push_back({STATE_GENPC,0x601 + unsigned(words.size())});
		for (auto const &e : events) if (e.write) { if (e.address >= 0x60) initial.data.push_back({e.address,0}); final.expected_data.push_back({e.address,e.value}); }
		for (auto const &e : events) if (!e.write && e.address >= 0x60) {
			initial.data.push_back({e.address,e.value});
			if (std::none_of(events.begin(),events.end(),[&](auto const &w) { return w.write && w.address == e.address; })) final.expected_data.push_back({e.address,e.value});
		}
		auto c = literal_sequence(std::move(initial), observation("pre-absolute-literal-snapshot", {{STATE_GENPC,0x5ff}}), std::move(final), 48, 1);
		c.data_events = std::move(events); cases.push_back(std::move(c));
	};
	append("LD-small-B-retains-A",{0xe908},{{r::STATE_A,0x123456},{r::STATE_B,0xffffffffffULL}},{{r::STATE_B,8},{r::STATE_A,0x123456}});
	append("LDM-AR4-B-unsigned",{0x4914},{{r::STATE_AR4,0x8001},{r::STATE_A,0x123456}},
		{{r::STATE_B,0x8001},{r::STATE_AR4,0x8001},{r::STATE_A,0x123456}},{{false,0x14,0x8001,0,false}});
	append("SRL-B-carry",{0xf3ff},{{r::STATE_B,0xff80000003ULL},{r::STATE_A,0x123456}},
		{{r::STATE_B,0x40000001},{r::STATE_A,0x123456},{r::STATE_ST0,0x800,0x800}});
	append("CMPM-absolute",{0x60f8,0xf24,0x1234},{{r::STATE_ST0,0x800}},{{r::STATE_ST0,0x1800,0x1800}},{{false,0xf24,0x1234,0,false}});
	append("SUB-signed-postincrement",{0x0892},{{r::STATE_AR2,0xf20}},{{r::STATE_A,1},{r::STATE_AR2,0xf21}},{{false,0xf20,0xffff,0,false}});
	append("STL-retains-source-pointer",{0x8082},{{r::STATE_A,0xabcd1234},{r::STATE_AR2,0xf30}},
		{{r::STATE_A,0xabcd1234},{r::STATE_AR2,0xf30}},{{true,0xf30,0x1234,0,false}});
	append("LD-signed-circular-wrap",{0x10d2},{{r::STATE_BK,4},{r::STATE_AR2,0xf23}},
		{{r::STATE_A,0xffffff8001ULL},{r::STATE_AR2,0xf20}},{{false,0xf23,0x8001,0,false}});
	append("dual-copy-Y-postincrement",{0xe50b},{{r::STATE_AR2,0xf20},{r::STATE_AR5,0xf33}},
		{{r::STATE_AR2,0xf20},{r::STATE_AR5,0xf34}},{{false,0xf20,0x5a3c,0,false},{true,0xf33,0x5a3c,0,false}});
	append("LD-signed-stationary",{0x1083},{{r::STATE_AR3,0xf35}},{{r::STATE_A,0xffffff8002ULL},{r::STATE_AR3,0xf35}},{{false,0xf35,0x8002,0,false}});
	append("LDU-stationary",{0x1284},{{r::STATE_AR4,0xf36}},{{r::STATE_A,0x8002},{r::STATE_AR4,0xf36}},{{false,0xf36,0x8002,0,false}});
	append("STLM-B-to-T",{0x890e},{{r::STATE_B,0x12345678}},{{r::STATE_T,0x5678},{r::STATE_B,0x12345678}},{{true,0xe,0x5678,0,false}});
	append("SRL-cross-accumulator-carry",{0xf1f4},{{r::STATE_A,0x12345e78},{r::STATE_B,0xffffffff}},
		{{r::STATE_B,0x12345},{r::STATE_A,0x12345e78},{r::STATE_ST0,0x800,0x800}});
	append("AND-register-preserves-B",{0xf280},{{r::STATE_A,0xf0f0},{r::STATE_B,0xff0}},{{r::STATE_A,0xf0},{r::STATE_B,0xff0}});
	append("XOR-literal-preserves-A",{0xf350,0xff},{{r::STATE_A,0xabcd},{r::STATE_B,0x12345678}},{{r::STATE_B,0x12345687},{r::STATE_A,0xabcd}});
	append("MVDK-circular-source-wrap",{0x71d2,0xf40},{{r::STATE_BK,4},{r::STATE_AR2,0xf23}},
		{{r::STATE_AR2,0xf20}},{{false,0xf23,0x5a3c,0,false},{true,0xf40,0x5a3c,0,false}});
	append("ADD-signed-stationary",{0x0083},{{r::STATE_A,1},{r::STATE_AR3,0xf52}},{{r::STATE_A,0xffffffffffULL},{r::STATE_AR3,0xf52}},{{false,0xf52,0xfffe,0,false}});
	append("MVMM-auxiliary",{0xe726},{{r::STATE_AR2,0x4567},{r::STATE_AR6,0}},{{r::STATE_AR2,0x4567},{r::STATE_AR6,0x4567}},
		{{false,0x12,0x4567,0,false},{true,0x16,0x4567,0,false}});
	// Check completed TCR write/read semantics, not unqualified simultaneous peripheral access.
	append("TCR-store-readback",{0x7726,0x10,0xf495,0x4826},{},{{r::STATE_A,0x10}},{{true,0x26,0x10,0,false},{false,0x26,0x10,0,false}});
	append("MAC-fractional-negative",{0x2883},{{r::STATE_A,5},{r::STATE_AR3,0xf52},{r::STATE_T,0xfffe},{r::STATE_ST1,0x40}},
		{{r::STATE_A,0xfffffffff9ULL},{r::STATE_AR3,0xf52},{r::STATE_T,0xfffe}},{{false,0xf52,3,0,false}});
	append("MPY-dual-fractional",{0xa43a},{{r::STATE_AR5,0xf60},{r::STATE_AR4,0xf70},{r::STATE_ST1,0x40}},
		{{r::STATE_A,0xfffffffff4ULL},{r::STATE_T,0xfffe},{r::STATE_AR5,0xf60},{r::STATE_AR4,0xf71}},
		{{false,0xf60,0xfffe,0,false},{false,0xf70,3,0,false}});
	append("SQDST-fractional",{0xe201},{{r::STATE_A,0x20000},{r::STATE_B,3},{r::STATE_AR2,0xf80},{r::STATE_AR3,0xf81},{r::STATE_ST1,0x140}},
		{{r::STATE_A,0xfffffe0000ULL},{r::STATE_B,11},{r::STATE_AR2,0xf80},{r::STATE_AR3,0xf81}},{{false,0xf80,0xffff,0,false},{false,0xf81,1,0,false}});
	append("MPYR-negative-retains-carry",{0x2282},{{r::STATE_T,0xfffd},{r::STATE_AR2,0xf90},{r::STATE_ST0,0x800},{r::STATE_ST1,0}},
		{{r::STATE_A,0xffffff0000ULL},{r::STATE_T,0xfffd},{r::STATE_AR2,0xf90},{r::STATE_ST0,0x800,0x800}},{{false,0xf90,0x4000,0,false}});
	append("MACR-after-accumulation",{0x2a82},{{r::STATE_A,0x4000},{r::STATE_T,1},{r::STATE_AR2,0xf90},{r::STATE_ST0,0x800},{r::STATE_ST1,0}},
		{{r::STATE_A,0x10000},{r::STATE_T,1},{r::STATE_AR2,0xf90},{r::STATE_ST0,0x800,0x800}},{{false,0xf90,0x4000,0,false}});
	append("MASR-negative-retains-carry",{0x2e82},{{r::STATE_T,3},{r::STATE_AR2,0xf90},{r::STATE_ST0,0x800},{r::STATE_ST1,0}},
		{{r::STATE_A,0xffffff0000ULL},{r::STATE_T,3},{r::STATE_AR2,0xf90},{r::STATE_ST0,0x800,0x800}},{{false,0xf90,0x4000,0,false}});
	append("MPYU-fractional",{0x2482},{{r::STATE_T,0xffff},{r::STATE_AR2,0xf90},{r::STATE_ST1,0x40}},
		{{r::STATE_A,0x1fffc0002ULL},{r::STATE_T,0xffff},{r::STATE_AR2,0xf90}},{{false,0xf90,0xffff,0,false}});
	append("LD-high-ignores-OVM",{0x4482},{{r::STATE_AR2,0xf90},{r::STATE_ST0,0x400},{r::STATE_ST1,0x200}},
		{{r::STATE_A,0x80000000},{r::STATE_AR2,0xf90},{r::STATE_ST0,0x400,0x600}},{{false,0xf90,0x8000,0,false}});
	append("LDM-AR5-A-unsigned",{0x4815},{{r::STATE_A,0xff12345678ULL},{r::STATE_AR5,0x8001},{r::STATE_ST0,0x400}},
		{{r::STATE_A,0x8001},{r::STATE_AR5,0x8001},{r::STATE_ST0,0x400}},{{false,0x15,0x8001,0,false}});
	append("LDM-AR5-B-unsigned",{0x4915},{{r::STATE_A,0x8001},{r::STATE_B,0xff12345678ULL},{r::STATE_AR5,0x8001},{r::STATE_ST0,0x400}},
		{{r::STATE_B,0x8001},{r::STATE_A,0x8001},{r::STATE_AR5,0x8001},{r::STATE_ST0,0x400}},{{false,0x15,0x8001,0,false}});
	append("LDM-indirect-masks-MMR-address",{0x4882},{{r::STATE_AR2,0xff15},{r::STATE_AR5,0x8001}},
		{{r::STATE_A,0x8001},{r::STATE_AR2,0x15}},{{false,0x15,0x8001,0,false}});
	append("STLM-indirect-MMR-postincrement",{0x8892},{{r::STATE_A,0x3456},{r::STATE_AR2,0xff17},{r::STATE_AR7,0}},
		{{r::STATE_AR7,0x3456},{r::STATE_AR2,0x18}},{{true,0x17,0x3456,0,false}});
}

inline void append_long_offset_literals(std::vector<clocked_register_case> &cases)
{
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
		auto c = literal_sequence(std::move(initial), observation("before-offset-markers",{{STATE_GENPC,0x600}}), std::move(final), 48, 3);
		c.observed_data_space = AS_IO;
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
		add("andm-long-offset-before-immediate", {0x68e2, 5, 0x0f0f}, {{r::STATE_AR2, 0x0f00}, {r::STATE_ST0, 0x0800}},
			{{r::STATE_AR2, 0x0f00}, {r::STATE_ST0, 0x0800, 0x0800}}, {{0x0f05, 0xff00}}, 6, {{0x0f05, 0x0f00}});
		add("orm-long-circular-retains-carry", {0x69f2, 2, 0x8000}, {{r::STATE_AR2, 0x0f03}, {r::STATE_BK, 4}, {r::STATE_ST0, 0x0800}},
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

}
}
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (auto append : {
		literal_tables::append_memory_literals,
		literal_tables::append_memory_alu_literals,
		literal_tables::append_memory_address_literals,
		literal_tables::append_memory_route_literals,
		literal_tables::append_absolute_literals,
		literal_tables::append_long_offset_literals}) append(cases);
	return cases;
}
}
