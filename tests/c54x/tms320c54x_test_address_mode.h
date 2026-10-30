// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_ADDRESS_MODE_H
#define MAME_NOKIA_TMS320C54X_TEST_ADDRESS_MODE_H

#include "tms320c54x_test_case.h"
#include <stdexcept>
#include <utility>

namespace c54x_test {

enum class address_mode_kind { basic, mvmd_cpl, mvmd_cmpt, stlm_cpl, stlm_cmpt, stm_cpl, stm_cmpt };
inline constexpr const char *address_mode_names[]{"cpl-dp", "mvmd-cpl", "mvmd-cmpt", "stlm-cpl", "stlm-cmpt", "stm-cpl", "stm-cmpt"};
constexpr bool address_mode_cmpt(address_mode_kind k)
{
	return k == address_mode_kind::mvmd_cmpt || k == address_mode_kind::stlm_cmpt || k == address_mode_kind::stm_cmpt;
}
constexpr bool address_mode_stlm(address_mode_kind k) { return k == address_mode_kind::stlm_cpl || k == address_mode_kind::stlm_cmpt; }
constexpr bool address_mode_stm(address_mode_kind k) { return k == address_mode_kind::stm_cpl || k == address_mode_kind::stm_cmpt; }
constexpr unsigned address_mode_count(address_mode_kind k) { return k == address_mode_kind::basic ? 8 : address_mode_stlm(k) ? 4 : 8; }

// Preserve raw keys, omitting gaps below TI's Category-I separation.
constexpr unsigned address_mode_key(unsigned index, address_mode_kind kind)
{
	bool const three = kind == address_mode_kind::basic || address_mode_stlm(kind);
	return three ? 6 + (index & 1) + (index / 2) * 8 : 4 + (index % 4) + (index / 4) * 8;
}

// SPRU131G tables 7-16/18/19: separated consumers, not hazard-result predictions.
template <typename R>
clocked_register_case address_mode_sequence(unsigned key, address_mode_kind kind)
{
	bool const full = kind != address_mode_kind::basic, cmpt = address_mode_cmpt(kind);
	bool const stlm = address_mode_stlm(kind), stm = address_mode_stm(kind);
	bool const set = key & 1, store = key & 8, dp = key & 16;
	char const *id = address_mode_names[unsigned(kind)];
	unsigned const gap = (key / 2) % 4, prefix = full && !stlm ? 2 : 1;
	bool const alternate = set;
	u16 const initial_st0 = cmpt ? 0 : dp && !set ? 0x0f : 0x0e;
	u16 const initial_st1 = cmpt ? set ? 0x800 : 0x820 : dp || set ? 0x800 : 0x4800;
	u16 const final_st0 = cmpt ? set ? 0x6000 : 0 : dp && set ? 0x0f : 0x0e;
	u16 const final_st1 = cmpt ? set ? 0x820 : 0x800 : dp || !set ? 0x800 : 0x4800;
	u16 const first = cmpt ? 0x1400 : 0x727, second = cmpt ? 0x1500 : dp ? 0x7a7 : 0x1027;
	u16 const opcode = !full ? dp ? set ? 0xf5b0 : 0xf4b0 : set ? 0xf7be : 0xf6be : stlm ? 0x8907 : stm ? 0x7707 : 0x730b;
	instruction_case initial{id, opcode, {}, {{0x727, 0x1111}, {dp ? 0x7a7 : 0x1027, 0x2222}}, {
		{R::STATE_IDLE, 0}, {R::STATE_TCR, 0x10}, {R::STATE_PMST, 0}, {R::STATE_ILLEGAL, 0},
		{R::STATE_ST0, initial_st0}, {R::STATE_ST1, initial_st1}, {R::STATE_SP, 0x1000},
		{R::STATE_A, 0x7777}, {R::STATE_IFR, 0}, {R::STATE_IMR, 0}, {STATE_GENPC, 0x600}
	}, {{R::STATE_ILLEGAL, 0}}, 0, 0};
	for (unsigned i = 0; i < 20; ++i) initial.program.push_back({0x600 + i, 0xf495});
	initial.program.push_back({0x600, opcode});
	if (prefix == 2) initial.program.push_back({0x601, stm ? set ? cmpt ? 0x820 : 0x4800 : 0x800 : 7});
	initial.program.push_back({0x600 + prefix + gap, cmpt ? store ? 0x8093 : 0x1093 : store ? 0x8027 : 0x1027});
	if (full) initial.initial.push_back({R::STATE_B, cmpt ? set ? 0x820 : 0x800 : set ? 0x4800 : 0x800});
	if (cmpt)
	{
		initial.initial.insert(initial.initial.end(), {{R::STATE_AR0, 0x1400}, {R::STATE_AR3, 0x1500}});
		initial.data.insert(initial.data.end(), {{0x1400, 0x1111}, {0x1500, 0x2222}});
	}
	instruction_case final{id, opcode, {}, {}, {}, {
		{R::STATE_ILLEGAL, 0}, {R::STATE_ST0, final_st0}, {R::STATE_ST1, final_st1},
		{R::STATE_A, u64(store ? 0x7777 : cmpt || alternate ? 0x2222 : 0x1111)}
	}, 0, 0, {{first, store && !cmpt && !alternate ? 0x7777 : 0x1111}, {second, store && (cmpt || alternate) ? 0x7777 : 0x2222}}};
	// ARF=3 selects AR3 in both modes; only compatibility mode updates ARP.
	if (cmpt) final.expected.insert(final.expected.end(), {{R::STATE_AR0, 0x1400}, {R::STATE_AR3, 0x1501}});
	clocked_register_case result{{{0, std::move(initial)}, {11, std::move(final)}}, 4 + gap, 11};
	result.identity = key;
	result.require_idle = false; // Preserve the original bounded NOP stream.
	result.check_pipeline_clock = true;
	return result;
}

template <typename R>
clocked_register_case immediate_dp_sequence(unsigned key)
{
	unsigned const page = key & 511, chain = (key / 512) & 1;
	u16 const first = 0xea00 | (chain ? page ^ 1 : page);
	instruction_case initial{"immediate-dp", first, {}, {{((page ^ 1) << 7) | 0x7f, 0x1111}, {(page << 7) | 0x7f, 0x2222}}, {
		{R::STATE_IDLE, 0}, {R::STATE_TCR, 0x10}, {R::STATE_PMST, 0}, {R::STATE_ILLEGAL, 0},
		{R::STATE_ST0, 0x0600 | (page ^ 1)}, {R::STATE_ST1, 0x800}, {R::STATE_A, 0},
		{R::STATE_IFR, 0}, {R::STATE_IMR, 0}, {STATE_GENPC, 0x600}
	}, {{R::STATE_ILLEGAL, 0}, {R::STATE_ST0, u64(0x0600 | (page ^ 1))}, {R::STATE_ST1, 0x800}, {R::STATE_A, 0}}, 0, 0};
	for (unsigned i = 0; i < 16; ++i) initial.program.push_back({0x600 + i, 0xf495});
	initial.program.push_back({0x600, first});
	if (chain) initial.program.push_back({0x601, 0xea00 | page});
	initial.program.push_back({0x601 + chain, 0x107f});
	auto status = initial;
	status.expected[1].value = 0x0600 | page;
	status.initial.clear(); status.data.clear(); status.program.clear();
	auto loaded = status;
	loaded.expected[3].value = 0x2222;
	clocked_register_case result{{{0, std::move(initial)}, {6 + chain, std::move(status)}, {7 + chain, std::move(loaded)}}, 4 + chain, 10};
	result.identity = key; result.require_idle = false; result.check_pipeline_clock = true;
	return result;
}

template <typename R>
clocked_register_case address_mode_bank_case(unsigned ordinal)
{
	if (ordinal >= 48)
	{
		auto result = immediate_dp_sequence<R>(ordinal - 48);
		result.identity = ordinal - 48 + 128;
		return result;
	}
	for (unsigned bank = 0; bank < 7; ++bank)
	{
		auto const kind = address_mode_kind(bank);
		unsigned const count = address_mode_count(kind);
		if (ordinal < count)
		{
			unsigned const key = address_mode_key(ordinal, kind);
			auto result = address_mode_sequence<R>(key, kind);
			result.identity = bank ? 32 + (bank - 1) * 16 + key : key;
			return result;
		}
		ordinal -= count;
	}
	throw std::out_of_range("address-mode bank ordinal");
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_ADDRESS_MODE_H
