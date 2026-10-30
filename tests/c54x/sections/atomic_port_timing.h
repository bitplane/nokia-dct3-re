// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 13; ++variant) {
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
		if (variant >= 11) words = {0x75d6,0x124,u16(variant == 11 ? 0x75f8 : 0x74f8),0x120,
			u16(variant == 11 ? 0x124 : 0x123),0x75d6,0x124,0xf4e1};
		auto initial = initial_case("atomic-port-address-value-wrap-and-clock-interval",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1+i].value = words[i];
		initial.initial.insert(initial.initial.end(),{{r::STATE_AR6,0x103},{r::STATE_AR4,0x113},{r::STATE_BK,4},
			{r::STATE_A,variant == 3 ? 1U : 0U},{r::STATE_AR2,variant == 6 ? 1U : 0U},
			{r::STATE_ST0,variant == 8 ? 0x1000U : 0U},{r::STATE_ST1,variant == 5 ? 0x100U : 0U}});
		initial.data = {{0x100,0x5678},{0x103,0x1234},{0x110,0xbeef},{0x113,0},{0x120,u16(variant >= 11 ? 0x9abc : 0xfffe)}};
		auto final = observation("port-results-and-independent-AR-selection",{{STATE_GENPC,0x601+words.size()},
			{r::STATE_AR6,single ? 0x103U : 0x101U},{r::STATE_AR4,single ? 0x110U : 0x113U},
			{r::STATE_A,variant == 5 ? 0xfffffffffeULL : variant == 3 ? 1U : 0U}});
		if (variant == 6 || variant == 7) final.expected.push_back({r::STATE_AR2,variant == 6 ? 0U : 0xffffU});
		final.expected_data = writing ? std::vector<memory_word>{{0x103,0x1234},{0x100,0x5678}} :
			single ? std::vector<memory_word>{{0x113,0xabcd},{0x110,0xbeef},{0x103,0x1234}} :
			std::vector<memory_word>{{0x103,0xabcd},{0x100,0xabcd},{0x113,0},{0x110,0xbeef}};
		if (variant >= 11) final.expected_data.push_back({0x120,u16(variant == 11 ? 0x9abc : 0xabcd)});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-port-program",{{STATE_GENPC,0x601}})},
			{2,observation("port-program-running",{})},{48,std::move(final)}},1,48};
		c.pipeline_frontend = false;
		c.observed_data_space = AS_IO;
		c.initial_io = {{port,0xabcd},{0x123,0xabcd}};
		c.data_events = std::vector<clocked_data_event>{{writing,port,u16(writing ? 0x1234 : 0xabcd),0,false}};
		if (!single) c.data_events->push_back({writing,port,u16(writing ? 0x5678 : 0xabcd),0,false,{},
			variant == 3 || variant == 8 ? 7U : variant == 4 || variant == 9 ? 5U :
			variant == 5 || variant == 7 ? 4U : variant == 6 || variant == 10 ? 6U : 2U});
		if (variant >= 11) c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1234,0,false},
			{variant == 11,u32(variant == 11 ? 0x124 : 0x123),u16(variant == 11 ? 0x9abc : 0xabcd),0,false},
			{true,0x124,0x5678,0,false,{},{},5}};
		if (!single) c.checkpoint = 3; // Port progress and the elapsed-clock origin survive restore.
		cases.push_back(std::move(c));
	}
	return cases;
}
}
