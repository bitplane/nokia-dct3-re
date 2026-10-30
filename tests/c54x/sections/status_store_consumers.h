// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_XF
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned immediate = 0; immediate != 2; ++immediate)
	for (unsigned family = 0; family != 6; ++family)
	for (unsigned source = 0; source != 2; ++source)
	for (unsigned pattern = 0; pattern != 64; ++pattern)
	for (unsigned gap = 0; gap != (family ? 4U : 1U); ++gap) {
		u16 requested = family ? 0x800 | (pattern & 31) | (pattern & 32 ? 0x100 : 0) : u16(0xffff ^ (pattern * 0x421));
		u16 const before = ((requested ^ 0x611f) & 0x711f) | (source ? 0x800 : 0);
		u64 const live = 0x8012340000ULL | requested, other = 0x8012345678ULL;
		if (!immediate && !family && (pattern & 32)) requested = before & 0x100 ? 0 : 0xffff;
		// This clear policy is retained as an implementation limit, not a silicon assertion.
		u16 const status = (requested & ~u16(0xc00)) | (before & requested & 0x800);
		u16 const mode = immediate || gap || family == 3 ? status : before;
		u64 expected = live;
		if (family == 1 || family == 3) expected = mode & 0x100 ? 0xffffff8001ULL : 0x8001;
		if (family == 2 || family >= 4) {
			expected = other;
			int const shift = int(mode & 15) - (mode & 16 ? 16 : 0);
			for (int i = 0; i < shift; ++i) expected = (expected * 2) & 0xffffffffffULL;
			for (int i = 0; i < -shift; ++i) expected = (expected >> 1) | ((mode & 0x100) && (expected & 0x8000000000ULL) ? 0x8000000000ULL : 0);
			if (family >= 4) expected = (family == 4 ? live + expected : live - expected) & 0xffffffffffULL;
		}
		u32 const base = (family + source + pattern + gap + immediate) % 2 ? 0x20000 : 0;
		unsigned const words = immediate ? 2 : 1;
		auto initial = initial_case("ST1-store-publication-and-mode-consumer", {u16(immediate ? 0x7707 : source ? 0x8907 : 0x8807)});
		for (auto &word : initial.program) word.address += base;
		if (immediate) initial.program[1].value = requested;
		unsigned consumer_words = 0;
		if (family == 1 || family == 3) {
			initial.program[words + gap].value = 0x1000 | (source ? 0x100 : 0) | (family == 3 ? 0xe0 : 0x80);
			consumer_words = family == 3 ? 2 : 1;
			if (family == 3) initial.program[words + gap + 1].value = 0;
		} else if (family) {
			initial.program[words + gap].value = (family == 2 ? 0xf482 : family == 4 ? 0xf480 : 0xf481) | (source ? 0x100 : 0x200);
			consumer_words = 1;
		}
		unsigned const idle = words + gap + consumer_words;
		initial.program[idle].value = 0xf4e1;
		initial.data = {{0x1400,0x8001}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_PMST,!family && (pattern & 32) ? 1U : 0U},
			{r::STATE_ST0,0x260e},{r::STATE_ST1,before},{r::STATE_SP,0x2000},{r::STATE_AR0,0x1400},
			{r::STATE_A,source ? other : 0xdead},{r::STATE_B,source ? 0xdead : other}});
		auto final = observation("consumer-uses-aged-SXM-and-ASM-without-republication", {{STATE_GENPC,base + 0x601 + idle},
			{int(source ? r::STATE_B : r::STATE_A),expected},{int(source ? r::STATE_A : r::STATE_B),other},
			{r::STATE_ST1,status},{r::STATE_PMST,!family && (pattern & 32) ? 1U : 0U},{r::STATE_SP,0x2000},{r::STATE_AR0,0x1400}},{{0x1400,0x8001}});
		clocked_register_case c{{{0,std::move(initial)},{6,observation("Execute-publishes-ST1-and-XF", {{r::STATE_ST1,status}})},
			{18,std::move(final)}},4 + (family + source + pattern + gap + immediate) % 3,18};
		c.actions = {{4,{{int(source ? r::STATE_B : r::STATE_A),live ^ 0xffff}}},
			{5,{{int(source ? r::STATE_B : r::STATE_A),live}}}};
		if (immediate) {
			c.actions.front().program.push_back({base + 0x601,u16(~requested)});
			c.program_events = {{false,base + 0x601,requested,0,false}};
		}
		c.data_events = std::vector<clocked_data_event>{{true,7,requested,6}};
		if (family == 1 || family == 3) {
			unsigned const read = 5 + words + gap + (family == 3);
			clocked_data_event const operand{false,0x1400,0x8001,read};
			if (read <= 6) c.data_events->insert(c.data_events->begin(),operand);
			else c.data_events->push_back(operand);
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
