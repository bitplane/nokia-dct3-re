// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_LONG_T_STORE_H
#define MAME_NOKIA_TMS320C54X_TEST_LONG_T_STORE_H

#include "tms320c54x_test_case.h"
#include <array>
#include <cstring>

namespace c54x_test {

enum class long_t_store_kind { none, success, rejected };
inline long_t_store_kind select_long_t_store(char const *profile)
{
	return !std::strcmp(profile, "st-t-long") ? long_t_store_kind::success :
		!std::strcmp(profile, "st-t-long-edge") ? long_t_store_kind::rejected : long_t_store_kind::none;
}
inline unsigned long_t_store_count(long_t_store_kind kind) { return kind == long_t_store_kind::rejected ? 256 : 6144; }
inline unsigned long_t_store_key(unsigned index, long_t_store_kind kind)
{
	if (kind != long_t_store_kind::rejected) return index;
	unsigned const form = index % 4, reg = (index / 4) % 8, compatibility = (index / 32) % 2;
	unsigned const source = (index / 64) % 2, page = index / 128;
	unsigned const point = (form + reg + compatibility + source + page) % 3;
	// Rejection never consumes the value or endpoint fields: retain one of
	// their eight identical stimuli, then balance the original 5/6/7 clocks.
	return index % 128 + page * 1024 + point * 2048;
}
struct long_t_store_input
{
	unsigned form, reg, selected, snapshot, write;
	bool trn, compatibility, excluded;
	u16 mode, literal, destination, value, st0;
	u32 base;
	std::array<u16, 8> initial, final;
};
inline long_t_store_input long_t_store_case(unsigned v, bool excluded)
{
	long_t_store_input f{}; f.excluded = excluded;
	f.form = v % 4; f.reg = (v / 4) % 8; f.compatibility = (v / 32) % 2; f.trn = (v / 64) % 2;
	f.selected = f.compatibility && !f.reg ? 4 : f.reg;
	f.value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(v / 128) % 4];
	bool const peripheral = !excluded && (v / 512) % 2;
	f.destination = excluded ? 0x0e : peripheral ? 0x48 + f.selected : 0x1201 + f.selected * 0x20;
	f.mode = f.form ? (0xe0 + (f.form - 1) * 8) | f.reg : 0xf8;
	f.literal = f.form == 0 ? f.destination : f.form == 1 ? 0xfff0 : 1;
	for (unsigned i = 0; i < 8; ++i) f.initial[i] = 0x2000 + i * 0x20;
	f.initial[f.selected] = f.form == 1 ? f.destination + 16 : f.destination - 1;
	f.final = f.initial;
	if (!excluded && f.form >= 2) f.final[f.selected] = f.destination;
	f.st0 = !excluded && f.compatibility && f.reg && f.form ? 0x608 | (f.reg << 13) : 0x8608;
	f.base = (v / 1024) % 2 ? 0x20000 : 0; f.snapshot = v / 2048; f.write = 7 + unsigned(peripheral);
	return f;
}

template <typename r>
clocked_register_case long_t_store_sequence(unsigned index, long_t_store_kind kind)
{
	unsigned const key = long_t_store_key(index, kind);
	auto const f = long_t_store_case(key, kind == long_t_store_kind::rejected);
	u16 const opcode = (f.trn ? 0x8d00 : 0x8c00) | f.mode;
	instruction_case setup{"long-t-store", opcode, {}, {}, {
		{r::STATE_IDLE, 0}, {r::STATE_ILLEGAL, 0}, {r::STATE_PMST, 0},
		{r::STATE_ST0, 0x8608}, {r::STATE_ST1, 0x900 | (f.compatibility ? 0x20 : 0)},
		{r::STATE_TCR, 0x10}, {r::STATE_IFR, 0}, {r::STATE_IMR, 0}, {r::STATE_BK, 16},
		{r::STATE_T, 0x5aa5}, {r::STATE_TRN, 0x5aa5},
		{r::STATE_A, 0x123456789aULL}, {r::STATE_B, 0x9876543210ULL}
	}, {{r::STATE_ILLEGAL, 0}}, 0, 0};
	for (unsigned i = 0; i < 16; ++i) setup.program.push_back({f.base + 0x600 + i, 0xf495});
	setup.program.insert(setup.program.end(), {{f.base + 0x600, opcode}, {f.base + 0x601, f.literal}, {f.base + 0x602, 0xf4e1}});
	for (unsigned i = 0; i < 8; ++i) setup.initial.push_back({r::STATE_AR0 + i, f.initial[i]});
	setup.initial.push_back({STATE_GENPC, f.base + 0x600});
	clocked_register_case test{{{0, std::move(setup)}}, std::array<unsigned, 3>{5, 6, f.write}[f.snapshot], 16,
		{{4, {}, false, {{f.base + 0x601, 0xdead}}}}, key};
	instruction_case read{"long-t-store", opcode, {}, {}, {}, {{r::STATE_ILLEGAL, f.excluded ? 1ULL : 0}}, 0, 0};
	if (!f.excluded)
	{
		clocked_register_action change{6, {}};
		if (f.write == 7) change.values.push_back({f.trn ? r::STATE_TRN : r::STATE_T, f.value});
		for (unsigned i = 0; i < 8; ++i)
		{
			read.expected.push_back({int(r::STATE_AR0) + int(i), f.final[i]});
			change.values.push_back({r::STATE_AR0 + i, 0x2200 + i});
		}
		test.actions.push_back(std::move(change));
		if (f.write == 8) test.actions.push_back({7, {{f.trn ? r::STATE_TRN : r::STATE_T, f.value}}});
	}
	test.states.push_back({6, std::move(read)});
	if (!f.excluded) test.states.push_back({7, {"long-t-store", opcode, {}, {}, {}, {{r::STATE_ILLEGAL, 0}}, 0, 0}});
	instruction_case final{"long-t-store", opcode, {}, {}, {}, {
		{r::STATE_ILLEGAL, f.excluded ? 1ULL : 0}, {r::STATE_IDLE, f.excluded ? 0ULL : 1},
		{r::STATE_ST0, f.st0}, {r::STATE_ST1, u64(0x900 | (f.compatibility ? 0x20 : 0))},
		{r::STATE_A, 0x123456789aULL}, {r::STATE_B, 0x9876543210ULL}
	}, 0, 0};
	for (unsigned i = 0; i < 8; ++i) final.expected.push_back({int(r::STATE_AR0) + int(i), f.excluded ? u64(f.initial[i]) : 0x2200 + i});
	test.states.push_back({16, std::move(final)});
	test.require_idle = !f.excluded;
	test.data_events = std::vector<clocked_data_event>{};
	if (!f.excluded) test.data_events->push_back({true, f.destination, f.value, f.write});
	return test;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_LONG_T_STORE_H
