// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	// ST #constant to DAGEN: every target/value/form and older-store class.
	// Replay points are distributed, not multiplied across every operand tuple.
	for (unsigned form = 0; form < 2; ++form) for (unsigned target = 0; target < 10; ++target)
	for (unsigned pattern = 0; pattern < 4; ++pattern) for (unsigned prefix = 0; prefix < 3; ++prefix)
	{
		unsigned const pre = prefix != 0;
		unsigned const write = 5 + pre + form + unsigned(prefix == 1 && !form);
		unsigned const gap = (target + pattern) % 2;
		unsigned const consumer = pre + 2 + form + gap + (target == 9 ? 1 + unsigned(prefix == 1 && !form) : 0);
		unsigned const read = consumer + 5;
		u16 const value = target == 9 ? std::array<u16,4>{0,1,4,8}[pattern] : 0x5000 + 2 * pattern;
		u16 const source = target == 9 ? pattern == 1 ? 0x5002 : 0x5003 : value;
		auto initial = initial_case("constant-dagen", {0xf495});
		if (prefix) initial.program[0].value = prefix == 1 ? 0x8817 : 0x8060;
		initial.program[pre].value = form ? 0x76f8 : 0x7610 + target;
		if (form) initial.program[pre + 1].value = 0x10 + target;
		initial.program[pre + 1 + form].value = value;
		initial.program[consumer].value = target < 8 ? 0x1090 | target : target == 8 ? 0x4a10 : 0x10d1;
		initial.program[consumer + 1].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0x600},{r::STATE_ST1,0x900},
			{r::STATE_A,0x1234566000ULL},{r::STATE_B,0xfeabcd9876ULL},
			{r::STATE_SP,0x4200},{r::STATE_BK,16},{r::STATE_T,0x5aa5}});
		for (unsigned ar = 0; ar < 8; ++ar)
			initial.initial.push_back({r::STATE_AR0 + ar,target == 9 && ar == 1 ? source : 0x4000 + ar});
		if (target != 8) initial.data.push_back({source,0x1350 + pattern});
		auto final = observation("consumer", {{r::STATE_A,target == 8 ? 0x1234566000ULL : 0x1350 + pattern},
			{r::STATE_B,0xfeabcd9876ULL},{r::STATE_SP,target == 8 ? u64(value - 1) : 0x4200},
			{r::STATE_BK,target == 9 ? u64(value) : 16},{r::STATE_T,0x5aa5},{r::STATE_ST0,0x600},{r::STATE_ST1,0x900}});
		for (unsigned ar = 0; ar < 8; ++ar)
		{
			u16 expected = prefix == 1 && ar == 7 ? 0x6000 : 0x4000 + ar;
			if (target == ar) expected = value + 1;
			if (target == 9 && ar == 1) expected = std::array<u16,4>{0x5004,0x5002,0x5000,0x5004}[pattern];
			final.expected.push_back({int(r::STATE_AR0) + int(ar),expected});
		}
		unsigned const point = std::array<unsigned,4>{3 + pre,write - 1,write,read}[(target + pattern + prefix + form) % 4];
		clocked_register_case c{{{0,std::move(initial)},{24,std::move(final)}},point,24};
		// Before the final observation only reject premature implementation fallback.
		c.states.front().observation.expected = {{r::STATE_ILLEGAL,0}};
		c.actions.push_back({3 + pre,{},false,{{0x601 + pre,0xdead}}});
		if (form) c.actions.push_back({4 + pre,{},false,{{0x602 + pre,0xbeef}}});
		c.retained_program_reads = form ? std::vector<memory_word>{{0x601 + pre,0x10 + target},{0x602 + pre,value}} :
			std::vector<memory_word>{{0x601 + pre,value}};
		c.data_events = std::vector<clocked_data_event>{};
		if (prefix && (prefix != 2 || form)) c.data_events->push_back({true,prefix == 1 ? 0x17U : 0x60U,0x6000,6});
		c.data_events->push_back({true,0x10 + target,value,write});
		// Same-clock early DAGEN publication precedes the RAM Execute write in this model.
		if (prefix == 2 && !form) c.data_events->push_back({true,0x60,0x6000,6});
		c.data_events->push_back({false,target == 8 ? 0x10U : source,u16(target == 8 ? 0x4000 : 0x1350 + pattern),read});
		if (target == 8) c.data_events->push_back({true,u32(value - 1),0x4000,read + 1});
		cases.push_back(std::move(c));
	}
	for (unsigned conflict = 0; conflict < 2; ++conflict) for (unsigned target = 0; target < 3; ++target)
	for (unsigned after = 0; after < 2; ++after)
	{
		u16 const address = std::array<u16,3>{0x12,0x18,0x19}[target];
		int const reg = std::array<int,3>{r::STATE_AR2,r::STATE_SP,r::STATE_BK}[target];
		auto initial = initial_case("stm-arbitration", {0xf495});
		if (conflict) initial.program[0].value = 0x8011;
		initial.program[conflict].value = 0x7700 | address;
		initial.program[1 + conflict].value = 0x1234;
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0},{r::STATE_A,0x5678},
			{r::STATE_AR1,0},{r::STATE_AR2,0},{r::STATE_BK,0}});
		initial.expected.push_back({reg,target == 1 ? 0x1000U : 0U});
		unsigned const write = conflict ? 7 : 5;
		clocked_register_case c{{{0,std::move(initial)},
			{write,observation("stm-publish", {{reg,0x1234}})}},write - 1 + after,9};
		c.require_idle = false; c.check_pipeline_clock = true;
		c.data_events = std::vector<clocked_data_event>{};
		if (conflict) c.data_events->push_back({true,0x11,0x5678,6});
		c.data_events->push_back({true,address,0x1234,write});
		cases.push_back(std::move(c));
	}
	// Repeated constant DAGEN writes are a model admission limit, not ISA illegality.
	for (unsigned form = 0; form < 2; ++form) for (unsigned target = 0; target < 10; ++target)
	for (unsigned point : {3U,9U})
	{
		auto initial = form ? initial_case("repeated-dagen-limit", {0xec01,0x76f8,u16(0x10 + target),0x1234}) :
			initial_case("repeated-dagen-limit", {0xec01,u16(0x7610 + target),0x1234});
		initial.initial.insert(initial.initial.end(), {{r::STATE_BK,16},{r::STATE_ST0,0x600},{r::STATE_ST1,0x900}});
		initial.expected.clear();
		auto rejected = observation("rejected", {{r::STATE_ILLEGAL,1},{r::STATE_SP,0x1000},{r::STATE_BK,16}});
		rejected.expected.pop_back(); // Remove observation()'s normal-success illegal check.
		for (unsigned ar = 0; ar < 8; ++ar) {
			initial.initial.push_back({r::STATE_AR0 + ar,0x4000 + ar});
			rejected.expected.push_back({int(r::STATE_AR0) + int(ar),0x4000 + ar});
		}
		clocked_register_case c{{{0,std::move(initial)},{20,std::move(rejected)}},point,20};
		c.require_idle = false; c.forbid_data_access = true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
