// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 7; ++variant) {
		std::vector<u16> words{0xf495};
		if (variant == 0) words.push_back(0x3182);
		if (variant == 1) words.push_back(0x0885);
		if (variant == 2) words.insert(words.end(),{0x1df8,0xf21});
		if (variant == 3) words.insert(words.end(),{0x80d3,0x818d,0x81d2,0x8395});
		if (variant == 4 || variant == 5) words.push_back(variant == 4 ? 0x808c : 0x80d2);
		if (variant == 6) words.insert(words.end(),{0x8815,0xe552,0xe754});
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
			events = {{true,0x15,0xf25,0,false},{false,0xf23,0xcafe,0,false},{true,0xf26,0xcafe,0,false},
				{false,0x15,0xf25,0,false},{true,0x14,0xf25,0,false}};
		}
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-sequence-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("sequence-running", {})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false; c.data_events = std::move(events); cases.push_back(std::move(c));
	}
	return cases;
}
}
