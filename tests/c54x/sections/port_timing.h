// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
namespace marker_tables {
inline void append_port_timing(std::vector<clocked_register_case> &cases)
{
	for (unsigned variant = 0; variant != 15; ++variant) {
		bool const writing = variant == 2 || variant >= 6;
		bool const single = variant == 1;
		u16 const port = writing ? 0x124 : 0x123;
		u16 const opcode = writing ? 0x75d6 : single ? 0x74d4 : 0x74d6;
		std::vector<u16> words{opcode,port};
		if (variant == 3 || variant == 4) words.insert(words.end(),{0xf844,0x605}); // BC next port, ANEQ.
		if (variant == 5) words.insert(words.end(),{0x10f8,0x120}); // Absolute signed LD between port markers.
		if (variant == 6 || variant == 7) words.insert(words.end(),{0x6c8a,0x605}); // BANZ next port,*AR2-.
		if (variant == 8 || variant == 9) words.insert(words.end(),{0xf830,0x605}); // BC next port,TC.
		if (variant == 10) words.insert(words.end(),{0xf073,0x605}); // B next port.
		if (!single) words.insert(words.end(),{opcode,port});
		words.push_back(0xf4e1);
		if (variant == 11 || variant == 12) words = {0x75d6,0x124,u16(variant == 11 ? 0x75f8 : 0x74f8),0x120,
			u16(variant == 11 ? 0x124 : 0x123),0x75d6,0x124,0xf4e1};
		if (variant >= 13) words = {0x75d6,0x124,0x6082,0x1234,0x75d6,0x124,0xf4e1};
		auto initial = marked_program("port-address-value-wrap-and-clock-interval",words);
		initial.initial.insert(initial.initial.end(),{{r::STATE_AR6,0x103},{r::STATE_AR4,0x113},{r::STATE_BK,4},
			{r::STATE_A,variant == 3 ? 1U : 0U},{r::STATE_AR2,variant == 6 ? 1U : 0U},
			{r::STATE_ST0,variant == 8 ? 0x1000U : 0U},{r::STATE_ST1,variant == 5 ? 0x100U : 0U}});
		initial.data = {{0x100,0x5678},{0x103,0x1234},{0x110,0xbeef},{0x113,0},{0x120,u16(variant >= 11 ? 0x9abc : 0xfffe)}};
		if (variant >= 13) {
			initial.initial.push_back({r::STATE_AR2,0x120});
			initial.data.back().value = variant == 13 ? 0x1234 : 0x5678;
		}
		auto final = observation("port-results-and-independent-AR-selection",{{STATE_GENPC,0x601+words.size()},
			{r::STATE_AR6,single ? 0x103U : 0x101U},{r::STATE_AR4,single ? 0x110U : 0x113U},
			{r::STATE_A,variant == 5 ? 0xfffffffffeULL : variant == 3 ? 1U : 0U}});
		if (variant == 6 || variant == 7) final.expected.push_back({r::STATE_AR2,variant == 6 ? 0U : 0xffffU});
		final.expected_data = writing ? std::vector<memory_word>{{0x103,0x1234},{0x100,0x5678}} :
			single ? std::vector<memory_word>{{0x113,0xabcd},{0x110,0xbeef},{0x103,0x1234}} :
			std::vector<memory_word>{{0x103,0xabcd},{0x100,0xabcd},{0x113,0},{0x110,0xbeef}};
		if (variant == 11 || variant == 12) final.expected_data.push_back({0x120,u16(variant == 11 ? 0x9abc : 0xabcd)});
		if (variant >= 13) {
			final.expected.push_back({r::STATE_AR2,0x120});
			final.expected.push_back({r::STATE_ST0,variant == 13 ? 0x1000U : 0U,0x1000});
			final.expected_data.push_back({0x120,u16(variant == 13 ? 0x1234 : 0x5678)});
		}
		auto c = marked_sequence(std::move(initial), std::move(final), 48, 1);
		c.initial_io = {{port,0xabcd},{0x123,0xabcd}};
		c.data_events = std::vector<clocked_data_event>{{writing,port,u16(writing ? 0x1234 : 0xabcd),0,false}};
		if (!single) c.data_events->push_back({writing,port,u16(writing ? 0x5678 : 0xabcd),0,false,{},
			variant == 3 || variant == 8 ? 7U : variant == 4 || variant == 9 ? 5U :
			variant == 5 || variant == 7 ? 4U : variant == 6 || variant == 10 ? 6U : 2U});
		if (variant == 11 || variant == 12) c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1234,0,false},
			{variant == 11,u32(variant == 11 ? 0x124 : 0x123),u16(variant == 11 ? 0x9abc : 0xabcd),0,false},
			{true,0x124,0x5678,0,false,{},{},5}};
		if (variant >= 13) c.data_events->back().cycles_from_previous = 4;
		if (!single) c.checkpoint = 3; // Port progress and the elapsed-clock origin survive restore.
		cases.push_back(std::move(c));
	}
}

inline void append_port_indirect_output(std::vector<clocked_register_case> &cases)
{
	constexpr u16 opcodes[]{0x7582,0x7583,0x7595,0x7592,0x758a};
	constexpr u16 values[]{0xface,0xbeef,0xcafe,0xface,0xface};
	for (unsigned variant = 0; variant != std::size(opcodes); ++variant) {
		auto initial = initial_case("indirect-port-source-and-modification",{0xf495,0x75f8,0x100,0x124,
			opcodes[variant],0x124,0x75f8,0x100,0x124,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_AR2,0x120},{r::STATE_AR3,0x122},{r::STATE_AR5,0x124},
			{r::STATE_ST0,0x1800},{r::STATE_ST1,0x100},{r::STATE_A,0x12345678},{r::STATE_B,0x76543210}});
		initial.data = {{0x100,0x1111},{0x120,0xface},{0x122,0xbeef},{0x124,0xcafe}};
		auto final = observation("old-source-output-and-one-selected-pointer-update",{{STATE_GENPC,0x60a},
			{r::STATE_AR2,variant == 3 ? 0x121U : variant == 4 ? 0x11fU : 0x120U},{r::STATE_AR3,0x122},
			{r::STATE_AR5,variant == 2 ? 0x125U : 0x124U},{r::STATE_A,0x12345678},{r::STATE_B,0x76543210},
			{r::STATE_ST0,0x1800},{r::STATE_ST1,0x100}});
		final.expected_data = initial.data;
		auto c = marked_sequence(std::move(initial), std::move(final), 48, 3);
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},{true,0x124,values[variant],0,false},
			{true,0x124,0x1111,0,false,{},{},5}};
		cases.push_back(std::move(c));
	}
}
}
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (auto append : {
		marker_tables::append_port_timing,
		marker_tables::append_port_indirect_output}) append(cases);
	return cases;
}
}
