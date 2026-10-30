// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_COMPOSITION_LIMITS_H
#define MAME_NOKIA_TMS320C54X_TEST_COMPOSITION_LIMITS_H

#include "tms320c54x_test_case.h"
#include <string_view>

namespace c54x_test {

enum class composition_limit { none, interrupt_pop, clock_move, delay_store, block_xc, delayed_block };
inline composition_limit select_composition_limit(std::string_view name)
{
	return name == "pop-irq-edge" ? composition_limit::interrupt_pop :
		name == "clock-mvdk-edge" ? composition_limit::clock_move :
		name == "st-delay-edge" ? composition_limit::delay_store :
		name == "block-xc-edge" ? composition_limit::block_xc :
		name == "rptbd-edge" ? composition_limit::delayed_block : composition_limit::none;
}

constexpr unsigned composition_limit_count(composition_limit kind)
{
	return kind == composition_limit::delay_store ? 16 : kind == composition_limit::block_xc ? 18 :
		kind == composition_limit::delayed_block ? 28 : 24;
}

struct composition_limit_contract { char const *summary, *completion; };
inline composition_limit_contract composition_limit_result(composition_limit kind)
{
	switch (kind)
	{
	case composition_limit::interrupt_pop:
		return {"TMS320C54x interrupt POPM exclusions: PASS cases=24 restore=24 destinations=IMR,IFR contexts=RPT,BD_first,BD_last pages=2 snapshots=2 reads=0 writes=0 stack_changes=0 mask_changes=0 acknowledgements=0 hardware_trap_claim=0", "TMS320C54x isolated interrupt POPM exclusions complete"};
	case composition_limit::clock_move:
		return {"TMS320C54x CLKMD MVDK exclusions: PASS cases=24 restore=24 contexts=RPT,BD_delay source_forms=3 pages=2 snapshots=2 reads=0 writes=0 pointer_changes=0 clock_changes=0 hardware_trap_claim=0", "TMS320C54x isolated CLKMD MVDK exclusions complete"};
	case composition_limit::delay_store:
		return {"TMS320C54x delay store boundary: PASS cases=16 restore=16 forms=4 pages=2 snapshots=2 partial_instruction_rejected=1 data_reads=0 memory_writes=0 atomic_fallback=0", "TMS320C54x isolated delay store boundary complete"};
	case composition_limit::block_xc:
		return {"TMS320C54x block XC exclusions: PASS cases=18 restore=18 forms=XC1,XC2 selected_span_crossings=3 pages=2 snapshots=3 data_reads=0 data_writes=0 pointer_effects=0 silicon_trap_claim=0", "TMS320C54x isolated block XC exclusions complete"};
	case composition_limit::delayed_block:
		return {"TMS320C54x delayed block repeat exclusions: PASS cases=28 restore=28 forms=STM_BRC,STLM_BRC,POPM_BRC,branch,spill,nested_body,nested_delay pages=2 snapshots=2 data_reads=0 data_writes=0 pointer_updates=0 counters_preserved=1 silicon_trap_claim=0", "TMS320C54x isolated delayed block repeat exclusions complete"};
	default:
		return {"", ""};
	}
}

template<typename R>
clocked_register_case rejection_sequence(instruction_case input, unsigned raw, unsigned checkpoint, unsigned finish)
{
	clocked_register_case result{{{0, std::move(input)}}, checkpoint, finish};
	result.states.push_back({finish, result.states.front().observation});
	result.states.back().observation.expected.push_back({R::STATE_ILLEGAL, 1});
	result.require_idle = false; result.forbid_data_access = true; result.identity = raw;
	return result;
}

// These are explicit staged-model admission limits, not silicon traps.
template<typename R>
clocked_register_case control_span_limit_case(unsigned raw, composition_limit kind)
{
	bool const delay = kind == composition_limit::delay_store, xc = kind == composition_limit::block_xc;
	u32 const base = (delay ? (raw / 4) % 2 : xc ? (raw / 3) % 2 : (raw / 7) % 2) ? 0x20000 : 0;
	instruction_case input{"control-span-limit", 0, {}, {}, {
		{R::STATE_IDLE, 0}, {R::STATE_ILLEGAL, 0}, {R::STATE_PMST, 0}, {R::STATE_TCR, 0x10},
		{R::STATE_ST0, 0x0608}, {R::STATE_ST1, 0x900}, {R::STATE_IMR, 0}, {R::STATE_IFR, 0},
		{STATE_GENPC, base + 0x600}
	}, {}, 0, 0};
	for (unsigned i = 0; i < (delay ? 16 : xc ? 12 : 24); ++i) input.program.push_back({base + 0x600 + i, 0xf495});
	auto word = [&](unsigned offset, u16 value) { input.program[offset].value = value; };
	unsigned checkpoint, finish = xc ? 20 : 16;
	if (delay)
	{
		unsigned const form = raw % 4;
		word(0, 0xf273); word(1, 0x700); word(3, 0x8000 | std::array<u16,4>{0xe2,0xea,0xf2,0xf8}[form]);
		word(4, form == 3 ? 0x5001 : 1);
		input.program.push_back({base + 0x700, 0xf4e1});
		input.initial.insert(input.initial.end(), {{R::STATE_A, 0x123456789aULL}, {R::STATE_B, 0xfedcba9876ULL},
			{R::STATE_SP, 0x3000}, {R::STATE_BK, 13}, {R::STATE_AR2, 0x5005}});
		input.data = {{0x3000, 0x700}, {form == 3 ? 0x5001U : 0x5006U, 0xdead}};
		checkpoint = raw < 8 ? 3 : 5;
	}
	else if (xc)
	{
		word(0, 0xf072); word(1, 0x602 + unsigned(raw % 3 == 2));
		word(2, raw % 3 ? 0xff00 : 0xfd00); word(3, 0x8c93); word(4, 0x8d94);
		input.initial.insert(input.initial.end(), {{R::STATE_BRC, 3}, {R::STATE_AR3, 0x5100}, {R::STATE_AR4, 0x5200}});
		input.expected = {{R::STATE_BRC, 3}, {R::STATE_AR3, 0x5100}, {R::STATE_AR4, 0x5200}};
		checkpoint = std::array<unsigned,3>{3,7,9}[raw / 6];
	}
	else
	{
		unsigned const form = raw % 7, guard = form == 5 ? 8 : form == 4 ? 6 : 5;
		word(0, 0xf272); word(1, form == 5 ? 0x605 : 0x606);
		word(2, std::array<u16,7>{0x771a,0x881a,0x8a1a,0xf073,0xf495,0xf495,0xf272}[form]);
		word(3, form == 4 ? 0x10e1 : form == 0 ? 1 : form == 3 || form == 6 ? 0x604 : 0xf495);
		word(4, form == 5 ? 0xf272 : form == 4 ? 3 : 0xf495); word(5, form == 5 ? 0x606 : 0xf495);
		word(7, 0xf4e1);
		input.initial.insert(input.initial.end(), {{R::STATE_A, 0x123456789aULL}, {R::STATE_B, 0xfedcba9876ULL},
			{R::STATE_T, 0x5aa5}, {R::STATE_SP, 0x3000}, {R::STATE_AR1, 0x4000}, {R::STATE_AR2, 0x5000},
			{R::STATE_AR3, 0x6000}, {R::STATE_AR4, 0x7000}, {R::STATE_AR5, 0x7100}, {R::STATE_AR6, 0x7200}, {R::STATE_BRC, 2}});
		input.expected = {{R::STATE_BRC, 2}, {R::STATE_SP, 0x3000}, {R::STATE_AR2, 0x5000},
			{R::STATE_AR3, 0x6000}, {R::STATE_A, 0x123456789aULL}, {R::STATE_T, 0x5aa5}};
		checkpoint = guard - 1 + raw / 14;
		auto result = rejection_sequence<R>(std::move(input), raw, checkpoint, finish);
		result.final_pipeline_clock = guard;
		return result;
	}
	return rejection_sequence<R>(std::move(input), raw, checkpoint, finish);
}

template<typename R>
clocked_register_case composition_limit_case(unsigned raw, composition_limit kind)
{
	if (kind != composition_limit::interrupt_pop && kind != composition_limit::clock_move)
		return control_span_limit_case<R>(raw, kind);
	bool const pop = kind == composition_limit::interrupt_pop;
	unsigned const form = raw % (pop ? 2 : 3), context = (raw / (pop ? 2 : 3)) % (pop ? 3 : 2);
	u32 const base = (raw / 6) % 2 ? (pop ? 0x10000 : 0x20000) : 0;
	instruction_case input{pop ? "interrupt-pop-limit" : "clock-move-limit", 0, {}, {}, {
		{R::STATE_IDLE, 0}, {R::STATE_ILLEGAL, 0}, {R::STATE_TCR, 0x10}, {R::STATE_PMST, 0},
		{R::STATE_ST0, pop ? 0x000eULL : 0x0628ULL}, {R::STATE_ST1, pop ? 0x800ULL : 0x900ULL},
		{R::STATE_SP, pop ? 0x1000ULL : 0x3000ULL}, {R::STATE_IFR, 0}, {R::STATE_IMR, pop ? 0x1357ULL : 0},
		{R::STATE_A, 0x123456789aULL}, {R::STATE_B, pop ? 0x2233445566ULL : 0xfedcba9876ULL},
		{STATE_GENPC, base + 0x600}
	}, {}, 0, 0};
	for (unsigned i = 0; i < 16; ++i) input.program.push_back({base + 0x600 + i, 0xf495});
	unsigned at = 0;
	auto word = [&](u16 value) { input.program[at++].value = value; };
	word(context ? 0xf273 : 0xec01);
	if (context) word(0x680);
	if (pop)
	{
		if (context == 2) word(0xf495);
		word(0x8a00 | form);
		input.data = {{0x1000, 0x5a5a}};
	}
	else
	{
		word(form == 0 ? 0x7120 : form == 1 ? 0x7196 : 0x71f8);
		word(form == 2 ? 0x1420 : 0x58);
		if (form == 2) word(0x58);
		input.initial.push_back({R::STATE_BK, 13});
		for (unsigned i = 0; i < 8; ++i) input.initial.push_back({int(R::STATE_AR0) + int(i), i == 6 && form == 1 ? 0x1420ULL : 0x1400ULL + i});
		input.data = {{0x1420, 0x3146}, {0x58, 0}, {0x58, 0}};
	}
	input.expected = pop ? std::vector<register_check>{{R::STATE_SP, 0x1000}, {R::STATE_IMR, 0x1357}, {R::STATE_IFR, 0}} :
		std::vector<register_check>{{R::STATE_AR6, form == 1 ? 0x1420ULL : 0x1406ULL}};
	if (!pop) input.expected_data = {{0x58, 0}};
	// Only require the final rejection latch: the old tests did not specify
	// its intermediate clock. Preservation and absent bus effects hold throughout.
	return rejection_sequence<R>(std::move(input), raw, 2 + raw / 12, 12);
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_COMPOSITION_LIMITS_H
