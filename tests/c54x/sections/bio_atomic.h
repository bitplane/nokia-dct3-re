// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	constexpr u16 opcodes[]{0xf802,0xf803,0xfa02,0xfa03,0xf902,0xf903,0xfc02,0xfc03,0xfe02,0xfe03,0xfd02,0xfd03,0xff02,0xff03};
	std::vector<clocked_register_case> cases;
	for (u16 opcode : opcodes) for (bool level : {false,true}) {
		bool const take = opcode & 1 ? !level : level;
		bool const execute = (opcode & 0xfd00) == 0xfd00;
		bool const returning = (opcode & 0xfc00) == 0xfc00 && !execute;
		bool const call = (opcode & 0xff00) == 0xf900;
		bool const delayed = opcode & 0x200;
		std::vector<u16> words{opcode};
		if ((opcode & 0xfc00) == 0xf800) words.push_back(0x910);
		if (execute) { words.push_back(0xe802); if (delayed) words.push_back(0xe903); }
		else { if (delayed) words.insert(words.end(),{0xe905,0xe906}); words.push_back(0xe801); }
		words.push_back(0xf4e1);
		auto initial = initial_case("atomic-BIO-NBIO-control-forms-at-both-levels",{0xf495});
		initial.program.insert(initial.program.end(),{{0x10900,0xf495},{0x10910,0xe802},{0x10911,0xf4e1}});
		for (unsigned i = 0; i != words.size(); ++i) initial.program.push_back({0x10901+i,words[i]});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,0x10900},{r::STATE_A,0},{r::STATE_B,0},{r::STATE_SP,0x400},
			{r::STATE_ST0,0},{r::STATE_ST1,0}});
		initial.data = {{0x400,0x910},{0x3ff,0xbeef}};
		auto final = observation("BIO-control-selection-delay-effects-and-stack",{{STATE_GENPC,!execute && take ? 0x10912U : 0x10901+words.size()},
			{r::STATE_A,execute ? take ? 2U : 0U : take ? 2U : 1U},
			{r::STATE_B,execute ? take && delayed ? 3U : 0U : delayed ? 6U : 0U},
			{r::STATE_SP,take && returning ? 0x401U : take && call ? 0x3ffU : 0x400U}});
		final.expected_data = {{0x400,0x910},{0x3ff,u16(take && call ? 0x903 : 0xbeef)}};
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-BIO-control",{{STATE_GENPC,0x10901}})},
			{2,observation("BIO-control-running",{})},{48,std::move(final)}},1,48};
		c.pipeline_frontend = false; c.initial_bio = level; c.bio_samples = std::vector<clocked_line_sample>{{0,level,false}};
		c.data_events = std::vector<clocked_data_event>{};
		if (take && call) c.data_events->push_back({true,0x3ff,0x903,0,false});
		if (take && returning) c.data_events->push_back({false,0x400,0x910,0,false});
		cases.push_back(std::move(c));
	}
	auto initial = initial_case("BIO-low-poll-loop-releases-through-physical-pin",{0xf495,0xf803,0x601,0xe807,0xf4e1});
	initial.initial.insert(initial.initial.end(),{{r::STATE_A,0},{r::STATE_ST1,0}});
	auto final = observation("released-poll-loop-reaches-real-IDLE",{{STATE_GENPC,0x605},{r::STATE_A,7}});
	clocked_register_case c{{{0,std::move(initial)},{1,observation("before-poll",{{STATE_GENPC,0x601}})},
		{2,observation("poll-running",{})},{12,observation("pin-low-remains-live",{{STATE_GENPC,0x601},{r::STATE_A,0},{r::STATE_IDLE,0}})},
		{14,observation("pin-raised",{})},{64,std::move(final)}},12,64};
	c.pipeline_frontend = false; c.initial_bio = false; c.forbid_data_access = true;
	c.actions = {{13,{}}}; c.actions.back().bio_level = true;
	cases.push_back(std::move(c));
	return cases;
}
}
