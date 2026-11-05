// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
namespace literal_tables {
inline void append_product_literals(std::vector<clocked_register_case> &cases)
{
	auto append = [&](char const *name,std::initializer_list<u16> words,std::initializer_list<register_setting> setup,
			std::initializer_list<register_check> checks,std::vector<clocked_data_event> events = {}) {
		auto initial = initial_case(name,{*words.begin()}); unsigned at = 0;
		for (u16 word : words) initial.program[at++].value = word;
		initial.program[at].value = 0xf4e1; initial.program.push_back({0x5ff,0xf495});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_ST0,0},{r::STATE_ST1,0},{r::STATE_T,0},{r::STATE_AR3,0x200}});
		initial.initial.insert(initial.initial.end(),setup.begin(),setup.end());
		for (auto const &event : events) if (!event.write) initial.data.push_back({event.address,event.value});
		auto final = observation(name,checks); final.expected.push_back({STATE_GENPC,0x601 + unsigned(words.size())});
		for (auto const &event : events) if (event.write) final.expected_data.push_back({event.address,event.value});
		auto c = literal_sequence(std::move(initial), observation("pre-product-snapshot", {{STATE_GENPC,0x5ff}}), std::move(final), 32, 1);
		c.data_events = std::move(events); cases.push_back(std::move(c));
	};
	append("MPYA signed17",{0xf58c},{{r::STATE_A,0x80000000},{r::STATE_B,0},{r::STATE_T,2}},
		{{r::STATE_A,0x80000000},{r::STATE_B,0x10000},{r::STATE_T,2}});
	append("MPYA publishes T",{0x3183},{{r::STATE_A,0x80000000},{r::STATE_B,0},{r::STATE_T,0x1234}},
		{{r::STATE_B,0x10000},{r::STATE_T,2}},{{false,0x200,2,0,false}});
	append("MACA old source",{0xf488},{{r::STATE_A,0x80000000},{r::STATE_B,0x1234},{r::STATE_T,1}},
		{{r::STATE_A,0x80008000},{r::STATE_B,0x1234}});
	for (unsigned fractional = 0; fractional < 2; ++fractional) append("MACAR rounding",{0xf689},
		{{r::STATE_A,0xfffffe0000ULL},{r::STATE_B,0x800c},{r::STATE_T,4},{r::STATE_ST0,0x1800},{r::STATE_ST1,fractional ? 0x40U : 0U}},
		{{r::STATE_A,fractional ? 0U : 0x10000U},{r::STATE_B,0x800c},{r::STATE_T,4},{r::STATE_ST0,0x1800},{r::STATE_ST1,fractional ? 0x40U : 0U}});
	append("MASAR negative product",{0xf48b},{{r::STATE_A,0x10000},{r::STATE_B,0x1234},{r::STATE_T,0x8000}},
		{{r::STATE_A,0x20000},{r::STATE_B,0x1234}});
	append("SQDST signed17",{0xe211},{{r::STATE_A,0x80010000},{r::STATE_B,0}},
		{{r::STATE_A,0},{r::STATE_B,0x40010001},{r::STATE_AR3,0x200}},{{false,0x200,0,0,false},{false,0x200,0,0,false}});
	append("SQUR signed17",{0xf48d},{{r::STATE_A,0x80010000}},{{r::STATE_A,0x40010001}});
	append("MAC immediate fractional",{0xf167,0x345},{{r::STATE_A,0x1000},{r::STATE_B,0},{r::STATE_T,0x400},{r::STATE_ST1,0x40}},
		{{r::STATE_A,0x1000},{r::STATE_B,0x1a3800},{r::STATE_T,0x400}});
	append("MAC memory immediate postincrement",{0x6495,0x1234},{{r::STATE_A,0x1000},{r::STATE_AR5,0x100}},
		{{r::STATE_A,0x6261060},{r::STATE_T,0x5678},{r::STATE_AR5,0x101}},{{false,0x100,0x5678,0,false}});
	append("MAC absolute extension order",{0x65f8,0x200,5},{{r::STATE_A,0x100},{r::STATE_B,0}},
		{{r::STATE_A,0x100},{r::STATE_B,0x10f},{r::STATE_T,3}},{{false,0x200,3,0,false}});
	append("ADD immediate shift16 carry",{0xf160,0xffff},{{r::STATE_A,0x10000},{r::STATE_B,0x1234},{r::STATE_ST1,0x100}},
		{{r::STATE_A,0x10000},{r::STATE_B,0},{r::STATE_ST0,0x800,0x800}});
	append("SUB immediate shift16 carry",{0xf361,1},{{r::STATE_A,0x1234},{r::STATE_B,0x20000},{r::STATE_ST1,0x100}},
		{{r::STATE_A,0x1234},{r::STATE_B,0x10000},{r::STATE_ST0,0x800,0x800}});
	append("ADD unsigned overflow",{0xf060,0xffff},{{r::STATE_A,0}},{{r::STATE_A,0xffff0000},{r::STATE_ST0,0x400,0x400}});
	append("MAC offset extension order",{0x65e3,2,4},{{r::STATE_A,0x10},{r::STATE_B,0}},
		{{r::STATE_A,0x10},{r::STATE_B,0x1c},{r::STATE_T,3},{r::STATE_AR3,0x200}},{{false,0x202,3,0,false}});
	append("MAC fractional saturation",{0xf167,0x8000},{{r::STATE_A,0},{r::STATE_B,0},{r::STATE_T,0x8000},{r::STATE_ST1,0x240}},
		{{r::STATE_A,0},{r::STATE_B,0x7fffffff},{r::STATE_ST0,0x200,0x200}});
	for (unsigned form = 0; form < 4; ++form) {
		u16 const source = form == 3 ? 0x202 : 0x200, word = std::array<u16,4>{0x6cac,0x2222,0x55aa,0xabcd}[form];
		u16 const opcode = std::array<u16,4>{0x4d83,0x4deb,0x4df8,0x4de3}[form];
		append("DELAY retains T and source",form == 0 ? std::initializer_list<u16>{opcode} :
			std::initializer_list<u16>{opcode,u16(form == 1 ? 1 : form == 2 ? 0x200 : 2)},
			{{r::STATE_T,0x1234},{r::STATE_AR3,form == 1 ? 0x1ffU : 0x200U},{r::STATE_ST1,0x240}},
			{{r::STATE_T,0x1234},{r::STATE_AR3,0x200}},{{false,source,word,0,false},{true,u32(source + 1),word,0,false}});
	}
	append("MPYU fractional saturation",{0x2483},{{r::STATE_A,0},{r::STATE_T,0xffff},{r::STATE_ST1,0x240}},
		{{r::STATE_A,0x7fffffff},{r::STATE_T,0xffff},{r::STATE_AR3,0x200},{r::STATE_ST0,0x400,0x400}},{{false,0x200,0xffff,0,false}});
}

inline void append_immediate_product_literals(std::vector<clocked_register_case> &cases)
{
	struct row { u16 opcode,literal,st0,st1,t; u64 a,b,out_a,out_b; u16 out_t,flags,mask; };
	constexpr row rows[]{
		{0x6283,4,0,0x40,0x1234,0,0x5678,24,0x5678,3,0,0},
		{0x6383,0xfffe,0x800,0,3,24,0x123456,24,0xfffffffffaULL,3,0x800,0xffff},
		{0x6383,0xfffe,0x800,0x40,3,24,0x123456,24,0xfffffffff4ULL,3,0x800,0xffff},
		{0x6683,0xfffe,0x800,0,3,100,10,4,10,3,0x800,0xffff},
		{0x6783,0xfffe,0x800,0x40,3,4,10,4,0xfffffffffeULL,3,0x800,0xffff},
		{0x6683,2,0x800,0,3,0x1234,0x7ffffffe,0x80000004,0x7ffffffe,3,0xc00,0xffff},
		{0x6683,2,0x800,0x200,3,0x1234,0x7ffffffe,0x7fffffff,0x7ffffffe,3,0xc00,0xffff},
		{0x6783,2,0x800,0,3,0x1234,0x7ffffffe,0x1234,0x80000004,3,0xa00,0xffff},
		{0x6783,2,0x800,0x200,3,0x1234,0x7ffffffe,0x1234,0x7fffffff,3,0xa00,0xffff},
		{0xf166,0xfffe,0xa00,0,0x2000,0x1234,0,0x1234,0xffffffc000ULL,0x2000,0,0},
		{0x62f8,5,0xa00,0,0,0,0xffffffc000ULL,15,0xffffffc000ULL,3,0,0}
	};
	for (auto const &v : rows) {
		bool const absolute = v.opcode == 0x62f8, memory = v.opcode != 0xf166;
		auto initial = initial_case("immediate-MPY-MAC-destination-and-overflow-literals",{0xf495,v.opcode});
		initial.program[2].value = absolute ? 0x200 : v.literal;
		if (absolute) initial.program[3].value = v.literal;
		initial.program[3 + absolute].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,v.a},{r::STATE_B,v.b},{r::STATE_T,v.t},
			{r::STATE_AR3,0x200},{r::STATE_ST0,v.st0},{r::STATE_ST1,v.st1}}); initial.data = {{0x200,3}};
		auto final = observation("literal-product-flags-T-and-source-preservation",{{STATE_GENPC,0x604 + unsigned(absolute)},
			{r::STATE_A,v.out_a},{r::STATE_B,v.out_b},{r::STATE_T,v.out_t},{r::STATE_AR3,0x200},{r::STATE_ST1,v.st1}},{{0x200,3}});
		if (v.mask) final.expected.push_back({r::STATE_ST0,v.flags,v.mask});
		auto c = literal_sequence(std::move(initial), observation("pre-product-snapshot",{{STATE_GENPC,0x600}}), std::move(final), 32, 1);
		c.data_events = std::vector<clocked_data_event>{};
		if (memory) c.data_events->push_back({false,0x200,3,0,false});
		cases.push_back(std::move(c));
	}
}

inline void append_mac_literals(std::vector<clocked_register_case> &cases)
{
	auto append = [&](u16 opcode,u64 a,u64 b,u16 t,u16 st0,u16 st1,u16 x,u16 y,u64 out_a,u64 out_b,
			u16 out_t,u16 flags,u16 mask,u16 stored,bool alias) {
		bool const parallel = (opcode & 0xf000) == 0xd000;
		auto initial = initial_case("MAC-routing-and-parallel-store", {opcode,0xf4e1});
		initial.program.push_back({0x5ff,0xf495});
		initial.data = {{0xf90,x},{0xf91,y}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,t},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_AR4,0xf91},{r::STATE_AR5,0xf90}});
		auto final = observation("independent-routing-rounding-and-store-outcome", {{STATE_GENPC,0x602},{r::STATE_A,out_a},
			{r::STATE_B,out_b},{r::STATE_T,out_t},{r::STATE_ST1,st1},
			{r::STATE_AR5,alias || opcode == 0xddba ? 0xf91U : 0xf90U}},{{0xf90,alias ? stored : x},{0xf91,parallel && !alias ? stored : y}});
		if (!alias) final.expected.push_back({r::STATE_AR4,0xf92});
		if (mask) final.expected.push_back({r::STATE_ST0,flags,mask});
		auto c = literal_sequence(std::move(initial), observation("pre-MAC-snapshot", {{r::STATE_A,a},{r::STATE_B,b}}), std::move(final), 32, 1);
		c.data_events = std::vector<clocked_data_event>{{false,0xf90,x,0,false},
			{parallel,alias ? 0xf90U : 0xf91U,parallel ? stored : y,0,false}};
		cases.push_back(std::move(c));
	};
	for (unsigned parallel = 0; parallel < 2; ++parallel) for (unsigned operation = 0; operation < 4; ++operation)
	for (unsigned source_b = 0; source_b < 2; ++source_b) for (unsigned destination_b = 0; destination_b < 2; ++destination_b) {
		u64 const a = 0x18000,b = 0x28000;
		s64 result = (parallel ? destination_b : source_b) ? b : a;
		result += operation & 2 ? -6 : 6;
		if (operation & 1) result = (result + 0x8000) & ~s64(0xffff);
		append((parallel ? 0xd03a : 0xb03a) | (operation << 10) | (source_b << 9) | (destination_b << 8),
			a,b,parallel ? 2 : 0,0x800,0x100,parallel ? 3 : 2,parallel ? 0 : 3,
			destination_b ? a : u64(result),destination_b ? u64(result) : b,2,0x800,0x800,source_b ? 2 : 1,false);
	}
	append(0xd93a,0x111111,0x1111,0x400,0,0x105,0x4321,0x1234,0x111111,0xfffef38d11ULL,0x400,0,0,0x222,false);
	append(0xddba,0x111111,0x1111,0x400,0,0x101,0x4321,0x1234,0x111111,0xfffef40000ULL,0x400,0,0,0x22,false);
	append(0xd9b7,0x18000,0x28000,2,0x800,0x100,3,0x1234,0x18000,0x27ffa,2,0,0,1,true);
	append(0xb83a,0x18000,0x28000,0,0,0x140,2,3,0x17ff4,0x28000,2,0,0x600,3,false);
	append(0xd93a,0x18000,0x28000,2,0,0x140,3,0,0x18000,0x27ff4,2,0,0x600,1,false);
	append(0xb83a,0x7fffffff,0,0,0,0x300,0xffff,2,0x7fffffff,0,0xffff,0x400,0x600,2,false);
	append(0xd93a,0x18000,0x7fffffff,2,0,0x300,0xffff,0,0x18000,0x7fffffff,2,0x200,0x600,1,false);
}

inline void append_coefficient_fir_literals(std::vector<clocked_register_case> &cases)
{
	auto const first = cases.size();
	for (unsigned variant = 0; variant != 5; ++variant) {
		bool const repeated = variant == 2 || variant == 4, fir = variant >= 3;
		u16 const opcode = variant == 0 ? 0x788b : variant == 1 ? 0x7a8b : variant == 2 ? 0x7893 : 0xe03a;
		auto initial = initial_case("program-coefficient-MACP-MACD-FIRS-literals",{0xf495});
		unsigned at = 1;
		if (repeated) initial.program[at++].value = 0xec02;
		initial.program[at++].value = opcode; initial.program[at++].value = 0x700; initial.program[at].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,variant == 2 ? 0U : 0x770000U},
			{r::STATE_B,fir ? 0U : 0x11223344U},{r::STATE_T,fir ? 0x4444U : 8U},
			{r::STATE_ST0,0},{r::STATE_ST1,0x100},{r::STATE_AR3,0xf90},{r::STATE_AR4,0xf91},{r::STATE_AR5,0xf90}});
		initial.data = fir ? repeated ? std::vector<memory_word>{{0xf90,1},{0xf91,2},{0xf92,2},{0xf93,2}} :
			std::vector<memory_word>{{0xf90,0x55},{0xf91,0xaa}} : repeated ?
			std::vector<memory_word>{{0xf90,1},{0xf91,2},{0xf92,3}} : std::vector<memory_word>{{0xf90,0x55},{0xf91,0x66}};
		u64 const result_a = variant <= 1 ? 0x7d0b44 : variant == 2 ? 32 : variant == 3 ? 0xff0000 : 0x30000;
		auto final = observation("fixed-coefficient-result-T-and-pointer-state",{{STATE_GENPC,0x601 + at},
			{r::STATE_A,result_a},{r::STATE_B,fir ? variant == 3 ? 0x8762cU : 0x86U : 0x11223344U},
			{r::STATE_T,fir ? 0x4444U : repeated ? 3U : 0x55U},{r::STATE_ST1,0x100},
			{r::STATE_AR3,fir ? 0xf90U : repeated ? 0xf93U : 0xf8fU},
			{r::STATE_AR4,fir ? repeated ? 0xf94U : 0xf92U : 0xf91U},{r::STATE_AR5,0xf90}});
		final.expected_data = initial.data;
		if (variant == 1) final.expected_data[1].value = 0x55;
		if (repeated) final.expected.push_back({r::STATE_RC,0});
		std::vector<clocked_data_event> reads, coefficients;
		for (unsigned i = 0; i != (repeated ? 3U : 1U); ++i) {
			u16 const coefficient = repeated ? fir ? i + 1 : i + 4 : 0x1234;
			initial.program.push_back({0x700 + i,coefficient}); final.expected_program.push_back({0x700 + i,coefficient});
			coefficients.push_back({false,0x700 + i,coefficient,0,false});
			reads.push_back({false,fir ? 0xf90U : 0xf90U + i,u16(repeated ? fir ? 1 : i + 1 : 0x55),0,false});
			if (fir) reads.push_back({false,0xf91 + i,u16(repeated ? 2 : 0xaa),0,false});
			if (variant == 1) reads.push_back({true,0xf91,0x55,0,false});
		}
		auto c = literal_sequence(std::move(initial), observation("pre-coefficient-operation-snapshot",{{STATE_GENPC,0x600}}), std::move(final), 64, 1);
		c.data_events = std::move(reads); c.program_events = std::move(coefficients); cases.push_back(std::move(c));
	}
	auto stream = cases.at(first+2);
	stream.states.front().observation.id = "MACP-retains-held-coefficient-stream";
	stream.states.front().observation.program.push_back({0x800,0xdead});
	stream.checkpoint = 8;
	stream.actions = {{8,{},false,{{0x603,0x800}},{},{{0xf90,0xdead}}}};
	stream.states.back().observation.expected_data.front().value = 0xdead;
	stream.data_events = std::vector<clocked_data_event>{{false,0xf90,1,8},{false,0xf91,2,10},{false,0xf92,3,11}};
	stream.program_events = {{false,0x700,4,9},{false,0x701,5,10},{false,0x702,6,11}};
	cases.push_back(std::move(stream));
	auto successor = cases.at(first+1);
	successor.states.front().observation.id = "MACD-retains-result-before-successor-write";
	successor.checkpoint = 8;
	successor.actions = {{8,{{r::STATE_T,0x7777}},false,{{0x700,0xdead}},{},{{0xf90,0xdead}}}};
	successor.states.back().observation.expected_data.front().value = 0xdead;
	successor.states.back().observation.expected_program.front().value = 0xdead;
	for (auto &value : successor.states.back().observation.expected)
		if (value.index == r::STATE_T) value.value = 0x7777;
	successor.data_events = std::vector<clocked_data_event>{{false,0xf90,0x55,7},{true,0xf91,0x55,9}};
	successor.program_events = {{false,0x700,0x1234,8}};
	cases.push_back(std::move(successor));
}

inline void append_multiply_move_literals(std::vector<clocked_register_case> &cases)
{
	for (unsigned variant = 0; variant != 7; ++variant) {
		std::vector<u16> words{0xf495};
		if (variant == 0) words.push_back(0x3182);
		if (variant == 1) words.push_back(0x0885);
		if (variant == 2) words.insert(words.end(),{0x1df8,0xf21});
		if (variant == 3) words.insert(words.end(),{0x80d3,0x818d,0x81d2,0x8395});
		if (variant == 4 || variant == 5) words.push_back(variant == 4 ? 0x808c : 0x80d2);
		// STLM-to-AR has two-word DAGEN latency (SPRU131G table 7-3).
		if (variant == 6) words.insert(words.end(),{0x8815,0xe552,0xf495,0xe754});
		words.push_back(0xf4e1);
		auto initial = initial_case("MPYA-SUB-XOR-store-and-mapped-move-literals", {0xf495}); initial.program.clear();
		for (unsigned i = 0; i != words.size(); ++i) initial.program.push_back({0x600 + i,words[i]});
		u64 const a = variant == 0 ? 0xfffffd0000ULL : variant == 1 ? 0x10000 : variant == 6 ? 0x12340f25 : 0x1234;
		u64 const b = variant == 0 ? 0 : variant == 2 ? 0xff0000000fULL : 0x89ababcd;
		u16 const ar2 = variant == 0 ? 0xf20 : variant >= 4 ? 0xf23 : 0xf22;
		u16 const ar3 = variant == 3 || variant == 6 ? 0xf23 : 0xf20;
		u16 const ar4 = variant == 6 ? 0xf26 : 0xf23;
		u16 const ar5 = variant == 6 ? 0 : 0xf25;
		u16 const st0 = variant == 0 || variant == 1 ? 0 : 0x800, st1 = variant == 0 ? 0 : 0x100;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,0},
			{r::STATE_AR2,ar2},{r::STATE_AR3,ar3},{r::STATE_AR4,ar4},{r::STATE_AR5,ar5},
			{r::STATE_BK,4},{r::STATE_ST0,st0},{r::STATE_ST1,st1}});
		auto final = observation("fixed-results-and-balanced-address-updates", {{STATE_GENPC,0x600 + words.size()},
			{r::STATE_A,variant == 1 ? 0x17fff : a},{r::STATE_B,variant == 0 ? 0xffffffffebULL : variant == 2 ? 0xff000000ffULL : b},
			{r::STATE_T,variant == 0 ? 7U : 0U},{r::STATE_ST1,st1},{r::STATE_BK,4},
			{r::STATE_AR2,variant == 3 ? 0xf23U : variant == 5 ? 0xf20U : ar2},
			{r::STATE_AR3,variant == 3 ? 0xf20U : variant == 6 ? 0xf22U : ar3},
			{r::STATE_AR4,variant == 4 ? 0xf22U : variant == 6 ? 0xf25U : ar4},
			{r::STATE_AR5,variant == 6 ? 0xf25U : ar5}});
		if (variant >= 2) final.expected.push_back({r::STATE_ST0,st0});
		std::vector<clocked_data_event> events;
		if (variant <= 2) {
			u16 const at = variant == 0 ? 0xf20 : variant == 1 ? 0xf25 : 0xf21, value = variant == 0 ? 7 : variant == 1 ? 0x8001 : 0xf0;
			initial.data = {{at,value}}; final.expected_data = initial.data; events = {{false,at,value,0,false}};
		} else if (variant == 3) {
			initial.data = {{0xf22,0},{0xf23,0},{0xf24,0},{0xf25,0}};
			final.expected_data = {{0xf22,0xabcd},{0xf23,0x1234},{0xf24,0x89ab},{0xf25,0xabcd}};
			events = {{true,0xf23,0x1234,0,false},{true,0xf25,0xabcd,0,false},{true,0xf22,0xabcd,0,false},{true,0xf24,0x89ab,0,false}};
		} else if (variant <= 5) {
			initial.data = {{0xf22,0xbeef},{0xf23,0},{0xf24,0xcafe}};
			final.expected_data = {{0xf22,0xbeef},{0xf23,0x1234},{0xf24,0xcafe}}; events = {{true,0xf23,0x1234,0,false}};
		} else {
			initial.data = {{0xf23,0xcafe},{0xf26,0}}; final.expected_data = {{0xf23,0xcafe},{0xf26,0xcafe}};
			events = {{false,0xf23,0xcafe,7},{true,0x15,0xf25,7},
				{false,0x15,0xf25,7},{true,0xf26,0xcafe,8},{true,0x14,0xf25,8}};
		}
		auto c = literal_sequence(std::move(initial), observation("pre-sequence-snapshot", {{STATE_GENPC,0x600}}), std::move(final), 32, 1);
		c.data_events = std::move(events); cases.push_back(std::move(c));
	}
}

inline void append_dual_arithmetic_literals(std::vector<clocked_register_case> &cases)
{
	struct row { u16 opcode,x,y,t,st1,ar4,ar5,out4,out5,out_t; u64 a,b,out_a,out_b; };
	constexpr row rows[]{
		{0xe13a,0x55,0xaa,0x4444,0x100,0xf91,0xf90,0xf92,0xf90,0x4444,0x77778888,0x100,0x77cd0888,0x3972},
		{0xa6ab,0x8765,0x1234,8,0x100,0xf90,0xf91,0xf91,0xf92,0x8765,0x1000,0x2222,0x9a0aa84,0x2222},
		{0xa7ab,0x8765,0xfffe,0,0x140,0xf90,0xf91,0xf91,0xf92,0x8765,0x1234,0,0x1234,0xfffffde26cULL}
	};
	for (auto const &v : rows) {
		auto initial = initial_case("LMS-and-MACSU-fixed-dual-memory-examples",{0xf495,v.opcode,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,v.a},{r::STATE_B,v.b},{r::STATE_T,v.t},
			{r::STATE_ST0,0},{r::STATE_ST1,v.st1},{r::STATE_AR4,v.ar4},{r::STATE_AR5,v.ar5}});
		initial.data = {{0xf90,v.x},{0xf91,v.y}};
		auto final = observation("fixed-product-sign-rounding-and-dual-address-results",{{STATE_GENPC,0x603},
			{r::STATE_A,v.out_a},{r::STATE_B,v.out_b},{r::STATE_T,v.out_t},{r::STATE_ST1,v.st1},
			{r::STATE_AR4,v.out4},{r::STATE_AR5,v.out5}},{{0xf90,v.x},{0xf91,v.y}});
		auto c = literal_sequence(std::move(initial), observation("pre-dual-operation-snapshot",{{STATE_GENPC,0x600}}), std::move(final), 32, 1);
		c.data_events = std::vector<clocked_data_event>{{false,0xf90,v.x,0,false},{false,0xf91,v.y,0,false}};
		cases.push_back(std::move(c));
	}
}

inline void append_dual_add_sub_literals(std::vector<clocked_register_case> &cases)
{
	for (unsigned variant = 0; variant != 8; ++variant) {
		bool const memory = variant >= 6, special = variant == 4 || variant == 5, b = variant < 4 && variant % 2;
		u16 const opcode = variant < 4 ? 0xa03a + variant * 0x100 : variant == 4 ? 0xa249 : variant == 5 ? 0xa201 : variant == 6 ? 0x6b84 : 0x6b8c;
		auto initial = initial_case("dual-ADD-SUB-and-saturating-ADDM-literals",{0xf495,opcode});
		if (memory) initial.program[2].value = 0xfff8;
		initial.program[2 + memory].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0x1234},{r::STATE_B,0x5678},{r::STATE_T,0x4444},
			{r::STATE_ST0,0},{r::STATE_ST1,memory ? 0x300U : 0x100U},{r::STATE_AR2,0xf94},
			{r::STATE_AR3,0xf95},{r::STATE_AR4,memory ? 0x100U : 0xf91U},{r::STATE_AR5,0xf90}});
		initial.data = memory ? std::vector<memory_word>{{0xff,0xbeef},{0x100,0x8007},{0x101,0xcafe}} :
			special ? std::vector<memory_word>{{0xf94,0xfffb},{0xf95,7}} : std::vector<memory_word>{{0xf90,0x55},{0xf91,0xaa}};
		u64 const result = special ? 0xfffff40000ULL : variant >= 2 ? 0xffffab0000ULL : 0xff0000;
		auto final = observation("fixed-shifted-sign-carry-and-address-effects",{{STATE_GENPC,memory ? 0x604U : 0x603U},
			{r::STATE_A,memory || b ? 0x1234 : result},{r::STATE_B,b ? result : 0x5678},{r::STATE_T,0x4444},
			{r::STATE_ST1,memory ? 0x300U : 0x100U},{r::STATE_AR2,variant == 4 ? 0xf93U : 0xf94U},
			{r::STATE_AR3,variant == 4 ? 0xf96U : 0xf95U},{r::STATE_AR4,memory ? variant == 7 ? 0xffU : 0x100U : special ? 0xf91U : 0xf92U},
			{r::STATE_AR5,0xf90}});
		if (special) final.expected.push_back({r::STATE_ST0,0x800,0xc00});
		if (memory) final.expected.push_back({r::STATE_ST0,0xc00});
		final.expected_data = initial.data; if (memory) final.expected_data[1].value = 0x8000;
		auto c = literal_sequence(std::move(initial), observation("pre-ALU-snapshot",{{STATE_GENPC,0x600}}), std::move(final), 32, 1);
		c.data_events = memory ? std::vector<clocked_data_event>{{false,0x100,0x8007,0,false},{true,0x100,0x8000,0,false}} :
			special ? std::vector<clocked_data_event>{{false,0xf94,0xfffb,0,false},{false,0xf95,7,0,false}} :
			std::vector<clocked_data_event>{{false,0xf90,0x55,0,false},{false,0xf91,0xaa,0,false}};
		cases.push_back(std::move(c));
	}
}
}
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (auto append : {
		literal_tables::append_product_literals,
		literal_tables::append_immediate_product_literals,
		literal_tables::append_mac_literals,
		literal_tables::append_coefficient_fir_literals,
		literal_tables::append_multiply_move_literals,
		literal_tables::append_dual_arithmetic_literals,
		literal_tables::append_dual_add_sub_literals}) append(cases);
	return cases;
}
}
