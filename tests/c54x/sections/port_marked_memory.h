// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
namespace marker_tables {
inline void append_port_marked_memory(std::vector<clocked_register_case> &cases)
{
	struct row { u16 opcode,extension,operand,st0,st1; u64 a,b,out_a,out_b; u16 flags,mask; unsigned interval; };
	constexpr row rows[]{
		{0xf020,0xff80,0,0,0,0,5,0xff80,5,0,0,4},
		{0xf020,0xff80,0,0,0x100,0,5,0xffffffff80ULL,5,0,0,4},
		{0x8094,0,0,0,0x100,0x12345678,5,0x12345678,5,0,0,3},
		{0x00f8,0x120,1,0,0,0xffffffffffULL,5,0,5,0x800,0x800,4},
		{0x00f8,0x120,0xfffe,0,0x100,1,5,0xffffffffffULL,5,0,0,4},
		{0x01f8,0x120,0x8001,0x1800,0,0x12345678,1,0x12345678,0x8002,0x1000,0xffff,4},
		{0x01f8,0x120,0x8001,0x1800,0x100,0x12345678,1,0x12345678,0xffffff8002ULL,0x1000,0xffff,4},
		{0x09f8,0x120,1,0,0,0x12345678,0,0x12345678,0xffffffffffULL,0,0x800,4},
		{0x09f8,0x120,0xfffe,0,0x100,0x12345678,0xffffffffffULL,0x12345678,1,0x800,0x800,4},
		{0xec05,0x0082,1,0,0,0,5,6,5,0,0,13},
		{0x7593,0x124,0xabcd,0,0,0,5,0,5,0,0,4}
	};
	for (auto const &v : rows) {
		bool const store = v.opcode == 0x8094;
		bool const port = v.opcode == 0x7593;
		std::vector<u16> words{0x75d6,0x124,v.opcode};
		if (!store) words.push_back(v.extension);
		words.insert(words.end(),{0x75d6,0x124,0xf4e1});
		auto initial = marked_program("port-marked-load-store-ALU-and-repeat",words);
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,v.a},{r::STATE_B,v.b},{r::STATE_ST0,v.st0},{r::STATE_ST1,v.st1},
			{r::STATE_AR2,0x120},{r::STATE_AR3,0x120},{r::STATE_AR4,0x120},{r::STATE_AR6,0x103},{r::STATE_BK,4}});
		initial.data = {{0x103,0x1234},{0x100,0x5678},{0x120,v.operand},{0x121,0xbeef}};
		auto final = observation("fixed-results-source-preservation-and-port-interval",{{STATE_GENPC,0x601+words.size()},
			{r::STATE_A,v.out_a},{r::STATE_B,v.out_b},{r::STATE_ST0,v.flags,v.mask},{r::STATE_ST1,v.st1},
			{r::STATE_AR2,0x120},{r::STATE_AR3,port ? 0x121U : 0x120U},{r::STATE_AR4,store ? 0x121U : 0x120U},
			{r::STATE_AR6,0x101}},{{0x103,0x1234},{0x100,0x5678},{0x120,u16(store ? 0x5678 : v.operand)},{0x121,0xbeef}});
		if (v.opcode == 0xec05) final.expected.push_back({r::STATE_RC,0});
		auto c = marked_sequence(std::move(initial), std::move(final), 64, 3);
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1234,0,false}};
		if (port) c.data_events->push_back({true,0x124,0xabcd,0,false,{},2});
		c.data_events->push_back({true,0x124,0x5678,0,false,{},port ? 2U : v.interval});
		if (v.opcode == 0xec05) {
			// Marker Execute=8; ADDs Execute=10..15; resumed two-word marker Execute=21.
			c.data_events->front().cycle = 8; c.data_events->front().check_cycle = true;
			c.data_events->back().cycle = 21; c.data_events->back().check_cycle = true;
		}
		cases.push_back(std::move(c));
	}
}

inline void append_port_marked_transfers(std::vector<clocked_register_case> &cases)
{
	constexpr u16 opcodes[]{0x7213,0x720b,0x708a,0x7083,0x7092,0x7090,0x7088,0x7194,0x7184};
	for (unsigned variant = 0; variant != std::size(opcodes); ++variant) {
		bool const mvdk = variant >= 7;
		std::vector<u16> words = mvdk ? std::vector<u16>{0x75d6,0x124} : std::vector<u16>{0x75f8,0x100,0x124};
		words.insert(words.end(),{opcodes[variant],u16(mvdk ? 0x122 : 0x120)});
		if (mvdk) words.insert(words.end(),{0x75d6,0x124});
		else words.insert(words.end(),{0x75f8,0x100,0x124});
		words.push_back(0xf4e1);
		auto initial = marked_program("port-marked-MVDM-MVKD-MVDK",words);
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0x11223344},{r::STATE_B,0xff1234abcdULL},
			{r::STATE_ST0,mvdk ? 0xaa5U : 0x1800U},{r::STATE_ST1,0x100},{r::STATE_AR0,0x123},
			{r::STATE_AR2,0x123},{r::STATE_AR3,0x123},{r::STATE_AR4,0x121},{r::STATE_AR6,0x103},{r::STATE_BK,4}});
		initial.data = {{0x100,0x1111},{0x103,0x2222},{0x120,u16(variant == 0 ? 0x1234 : variant == 1 ? 0xcafe : mvdk ? 0xbeef : 0x5678)},
			{0x121,0xabcd},{0x122,0},{0x123,u16(mvdk ? 0xcafe : 0)}};
		auto final = observation("transfer-result-neighbors-status-and-addressing",{{STATE_GENPC,0x601+words.size()},
			{r::STATE_A,0x11223344},{r::STATE_B,variant == 1 ? 0xff1234cafeULL : 0xff1234abcdULL},
			{r::STATE_ST0,mvdk ? 0xaa5U : 0x1800U},{r::STATE_ST1,0x100},
			{r::STATE_AR0,variant == 5 ? 0x124U : variant == 6 ? 0x122U : 0x123U},
			{r::STATE_AR2,variant == 2 ? 0x122U : variant == 4 ? 0x124U : 0x123U},
			{r::STATE_AR3,variant == 0 ? 0x1234U : 0x123U},{r::STATE_AR4,variant == 7 ? 0x122U : 0x121U},
			{r::STATE_AR6,mvdk ? 0x101U : 0x103U}});
		final.expected_data = initial.data;
		if (mvdk) final.expected_data[4].value = 0xabcd;
		else if (variant >= 2) final.expected_data[5].value = 0x5678;
		auto c = marked_sequence(std::move(initial), std::move(final), 48, 3);
		c.data_events = mvdk ? std::vector<clocked_data_event>{{true,0x124,0x2222,0,false},{true,0x124,0x1111,0,false,{},4}} :
			std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},{true,0x124,0x1111,0,false,{},5}};
		cases.push_back(std::move(c));
	}
}

inline void append_port_loads(std::vector<clocked_register_case> &cases)
{
	for (unsigned variant = 0; variant != 6; ++variant) {
		bool const port_read = variant < 2;
		bool const circular = variant >= 4;
		bool const sign = variant >= 2 && !(variant & 1);
		std::vector<u16> words = port_read ? std::vector<u16>{0x75d6,0x124,0xf495,
			u16(variant ? 0x7483 : 0x7482),0x123,u16(variant ? 0x7483 : 0x7482),0x123,0xf4e1} :
			std::vector<u16>{0x75f8,0x100,0x124,u16(circular ? 0x56d3 : 0x5685),0x75f8,0x100,0x124,0xf4e1};
		auto initial = marked_program("stationary-port-or-doubleword-load",words);
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,port_read ? 5U : 0U},{r::STATE_B,port_read ? 133U : 0x76543210U},
			{r::STATE_AR2,0x120},{r::STATE_AR3,port_read ? 0x122U : 0x124U},{r::STATE_AR5,0x120},{r::STATE_AR6,0x103},
			{r::STATE_BK,port_read ? 4U : 5U},{r::STATE_ST0,0xaa5},{r::STATE_ST1,port_read || sign ? 0x100U : 0U}});
		initial.data = port_read ? std::vector<memory_word>{{0x103,0x1234},{0x120,u16(variant ? 0xabcd : 0)},
			{0x121,0xbeef},{0x122,0},{0x123,0xcafe}} : std::vector<memory_word>{{0x100,0x1111},
			{0x120,0xff80},{0x121,0x1234},{0x124,0xff80},{0x125,0x1234}};
		auto final = observation("load-result-neighbors-status-and-independent-pointer",{{STATE_GENPC,0x609},
			{r::STATE_A,port_read ? 5ULL : sign ? 0xffff801234ULL : 0xff801234ULL},
			{r::STATE_B,port_read ? 133U : 0x76543210U},{r::STATE_AR2,0x120},
			{r::STATE_AR3,port_read ? 0x122U : circular ? 0x121U : 0x124U},{r::STATE_AR5,0x120},
			{r::STATE_ST0,0xaa5},{r::STATE_ST1,port_read || sign ? 0x100U : 0U}});
		final.expected_data = initial.data;
		if (port_read) final.expected_data[variant ? 3 : 1].value = 0xabcd;
		auto c = marked_sequence(std::move(initial), std::move(final), 48, 3); c.initial_io = {{0x123,0xabcd}};
		c.data_events = port_read ? std::vector<clocked_data_event>{{true,0x124,0x1234,0,false},
			{false,0x123,0xabcd,0,false},{false,0x123,0xabcd,0,false,{},2}} :
			std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},{true,0x124,0x1111,0,false,{},4}};
		cases.push_back(std::move(c));
	}
}

inline void append_port_low_store(std::vector<clocked_register_case> &cases)
{
	constexpr u16 opcodes[]{0x81f8,0x8192,0x8190,0x8191,0x8090,0x8194};
	constexpr u16 destinations[]{0x120,0x122,0x124,0x126,0x124,0x128};
	for (unsigned variant = 0; variant != std::size(opcodes); ++variant) {
		std::vector<u16> words{0x75f8,0x100,0x124,opcodes[variant]};
		if (!variant) words.push_back(0x120);
		words.insert(words.end(),{0x75f8,0x100,0x124,0xf4e1});
		auto initial = marked_program("low-store-source-pointer-and-status-selection",words);
		u16 const st0 = variant == 5 ? 0xa608 : 0x1800, st1 = variant == 5 ? 0x800 : 0x100;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0x12345678},{r::STATE_B,0x1234beef},
			{r::STATE_AR0,0x124},{r::STATE_AR1,0x126},{r::STATE_AR2,0x122},{r::STATE_AR4,0x128},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1}});
		initial.data = {{0x100,0x1111}};
		for (unsigned address = 0x120; address != 0x12a; ++address) initial.data.push_back({address,u16(address & 1 ? 0xcafe : 0)});
		auto final = observation("exact-destination-preserves-neighbors-accumulators-and-status",{{STATE_GENPC,0x601+words.size()},
			{r::STATE_A,0x12345678},{r::STATE_B,0x1234beef},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_AR0,variant == 2 || variant == 4 ? 0x125U : 0x124U},
			{r::STATE_AR1,variant == 3 ? 0x127U : 0x126U},{r::STATE_AR2,variant == 1 ? 0x123U : 0x122U},
			{r::STATE_AR4,variant == 5 ? 0x129U : 0x128U}});
		final.expected_data = initial.data;
		for (auto &word : final.expected_data) if (word.address == destinations[variant]) word.value = variant == 4 ? 0x5678 : 0xbeef;
		auto c = marked_sequence(std::move(initial), std::move(final), 48, 3);
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},{true,0x124,0x1111,0,false,{},variant ? 4U : 5U}};
		cases.push_back(std::move(c));
	}
}

inline void append_port_register_examples(std::vector<clocked_register_case> &cases)
{
	constexpr u16 opcodes[]{0x8814,0x8813,0x8811,0x8912};
	constexpr unsigned destinations[]{4,3,1,2};
	for (unsigned variant = 0; variant != 6; ++variant) {
		bool const repeat = variant == 4, load = variant == 5;
		std::vector<u16> words{0x75f8,0x100,0x124};
		if (repeat) words.insert(words.end(),{0xec0c,0x6d90,0xe80f});
		else if (load) words.insert(words.end(),{0x13f8,0x120});
		else words.push_back(opcodes[variant]);
		words.insert(words.end(),{0x75f8,0x100,0x124,0xf4e1});
		auto initial = marked_program("port-marked-register-store-repeat-and-LDU",words);
		u64 const a = repeat ? 0 : load ? 0x12345678 : 0x1234beef;
		u64 const b = repeat || load ? 0 : 0x5678cafe;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_RC,0},{r::STATE_ST0,0},{r::STATE_ST1,repeat ? 0U : 0x100U}});
		for (unsigned ar = 0; ar != 8; ++ar) initial.initial.push_back({int(r::STATE_AR0+ar),0});
		initial.data = {{0x100,0x1111},{0x120,0x8001}};
		auto final = observation("selected-register-or-repeat-result-and-port-interval",{{STATE_GENPC,0x601+words.size()},
			{r::STATE_A,repeat ? 15 : a},{r::STATE_B,load ? 0x8001 : b},{r::STATE_RC,0}});
		for (unsigned ar = 0; ar != 8; ++ar) final.expected.push_back({int(r::STATE_AR0+ar),
			repeat && !ar ? 13U : !repeat && !load && ar == destinations[variant] ? variant == 3 ? 0xcafeU : 0xbeefU : 0U});
		final.expected_data = initial.data;
		auto c = marked_sequence(std::move(initial), std::move(final), 64, 3);
		// The modeled repeat tail refills Prefetch; steady MAR retirement remains one clock.
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},{true,0x124,0x1111,0,false,{},repeat ? 22U : load ? 5U : 4U}};
		cases.push_back(std::move(c));
	}
}
}
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (auto append : {
		marker_tables::append_port_marked_memory,
		marker_tables::append_port_marked_transfers,
		marker_tables::append_port_loads,
		marker_tables::append_port_low_store,
		marker_tables::append_port_register_examples}) append(cases);
	return cases;
}
}
