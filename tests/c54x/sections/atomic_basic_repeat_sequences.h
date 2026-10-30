// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 4; ++variant) {
		std::vector<u16> const words = variant == 0 ? std::vector<u16>{0xf062,0x1234,0x4ef8,0x90c,0xe800,0xf070,2,0x6d10} :
			variant == 1 ? std::vector<u16>{0xf171,1,0x6d10} : variant == 2 ?
			std::vector<u16>{0x47f8,0x918,0x6bf8,0x91a,1} : std::vector<u16>{0xf070,1,0x7d92,0x924};
		auto initial = initial_case("basic-repeat-and-transfer-sequences",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1 + i].value = words[i];
		initial.program[1 + words.size()].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0},{r::STATE_B,variant == 1 ? 0x1234U : 0U},
			{r::STATE_AR0,0},{r::STATE_AR2,0x920},{r::STATE_ST0,0},{r::STATE_ST1,0}});
		auto final = observation("fixed-repeat-count-and-memory-space-effects",{{STATE_GENPC,0x602 + words.size()},
			{r::STATE_A,0},{r::STATE_B,0},{r::STATE_AR0,variant == 0 ? 3U : variant == 1 ? 2U : 0U},
			{r::STATE_AR2,variant == 3 ? 0x922U : 0x920U},{r::STATE_RC,0}});
		std::vector<clocked_data_event> events, program;
		if (variant == 0) {
			initial.data = {{0x90c,0xbeef},{0x90d,0xcafe}}; final.expected_data = {{0x90c,0x1234},{0x90d,0}};
			events = {{true,0x90c,0x1234,0,false},{true,0x90d,0,0,false}};
		} else if (variant == 2) {
			initial.data = {{0x918,1},{0x91a,0}}; final.expected_data = {{0x918,1},{0x91a,2}};
			events = {{false,0x918,1,0,false},{false,0x91a,0,0,false},{true,0x91a,1,0,false},
				{false,0x91a,1,0,false},{true,0x91a,2,0,false}};
		} else if (variant == 3) {
			initial.data = {{0x920,0xaaaa},{0x921,0xbbbb}}; final.expected_data = initial.data;
			initial.program.insert(initial.program.end(),{{0x924,0},{0x925,0}}); final.expected_program = {{0x924,0xaaaa},{0x925,0xbbbb}};
			events = {{false,0x920,0xaaaa,0,false},{false,0x921,0xbbbb,0,false}};
			program = {{true,0x924,0xaaaa,0,false},{true,0x925,0xbbbb,0,false}};
		}
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-sequence-snapshot",{{STATE_GENPC,0x601}})},
			{2,observation("sequence-running",{})},{64,std::move(final)}},1,64};
		c.pipeline_frontend = false; c.data_events = std::move(events); c.program_events = std::move(program); cases.push_back(std::move(c));
	}
	{
		auto initial = initial_case("basic-absolute-store-load-complement-and-XOR",{0xf495,0x76f8,0x910,0x5678,0x7214,0x912,
			0x57f8,0x914,0xf793,0xff0c,0xf495,0xf793,0xf065,0xff,0xf054,0xf0,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0},{r::STATE_B,0},{r::STATE_AR4,0},{r::STATE_ST0,0},{r::STATE_ST1,0}});
		initial.data = {{0x910,0},{0x912,0xabcd},{0x914,0x1234},{0x915,0x5678}};
		auto final = observation("independent-mixed-sequence-results",{{STATE_GENPC,0x611},{r::STATE_A,0xff0f00},
			{r::STATE_B,0xffedcba987ULL},{r::STATE_AR4,0xabcd}},{{0x910,0x5678},{0x912,0xabcd},{0x914,0x1234},{0x915,0x5678}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-mixed-sequence-snapshot",{{STATE_GENPC,0x601}})},
			{2,observation("mixed-sequence-running",{})},{64,std::move(final)}},1,64};
		c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{{true,0x910,0x5678,0,false},{false,0x912,0xabcd,0,false},
			{true,0x14,0xabcd,0,false},{false,0x914,0x1234,0,false},{false,0x915,0x5678,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
