// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_REGISTER_UNARY_H
#define MAME_NOKIA_TMS320C54X_TEST_REGISTER_UNARY_H

#include "tms320c54x_test_case.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <string_view>

namespace c54x_test {

enum class register_unary_kind { none, logic, shift, negate, absolute };
inline register_unary_kind select_register_unary(std::string_view name)
{
	return name == "reg-logic" ? register_unary_kind::logic : name == "reg-shifts" ? register_unary_kind::shift :
		name == "negate" ? register_unary_kind::negate : name == "absolute" ? register_unary_kind::absolute : register_unary_kind::none;
}
constexpr unsigned register_unary_count(register_unary_kind kind)
{
	return kind == register_unary_kind::logic ? 3104 : kind == register_unary_kind::shift ? 3840 : 400;
}

inline std::string register_unary_result(register_unary_kind kind, unsigned count, unsigned restores, u64 digest)
{
	char output[640];
	char const *format = kind == register_unary_kind::logic ?
		"TMS320C54x staged register logic: PASS cases=%u restore=%u logical_cases=3072 complement_cases=32 checkpoint_policy=balanced data_accesses=0 duplicate_fetches=0 silicon_phase_claim=0 coverage_fnv64=%016llx" : kind == register_unary_kind::shift ?
		"TMS320C54x staged accumulator shifts: PASS cases=%u restore=%u operations=SFTA,SFTL selectors=4 shifts=-16..15 patterns=8 flag_policy=boundaries arp_initial=0 checkpoint_policy=balanced duplicate_fetches=0 data_accesses=0 left_c_policy=manual_formula silicon_phase_claim=0 coverage_fnv64=%016llx" : kind == register_unary_kind::absolute ?
		"TMS320C54x staged ABS: PASS cases=%u restore=%u numeric=128 status_boundaries=240 producer_cases=32 pages=1 checkpoint_policy=balanced data_accesses=0 irq_acceptance=0 silicon_phase_claim=0 coverage_fnv64=%016llx" :
		"TMS320C54x staged NEG: PASS cases=%u restore=%u numeric=128 status_boundaries=240 producer_cases=32 pages=1 checkpoint_policy=balanced data_accesses=0 silicon_phase_claim=0 coverage_fnv64=%016llx";
	std::snprintf(output, sizeof(output), format, count, restores, (unsigned long long)digest);
	return output;
}
// One replay point per numeric tuple, balanced across the original boundaries.
constexpr unsigned register_unary_key(unsigned ordinal, register_unary_kind kind)
{
	if (kind == register_unary_kind::logic)
	{
		if (ordinal < 3072) return ordinal + 3072 * ((ordinal % 3 + (ordinal / 3) % 4 +
			(ordinal / 12) % 32 + (ordinal / 384) % 4 + ordinal / 1536) % 2);
		unsigned const q = ordinal - 3072;
		return 6144 + q + 32 * ((q + q / 4 + q / 16) % 2);
	}
	if (kind == register_unary_kind::shift)
	{
		// Full shift range with baseline flags; flag interactions at four boundaries.
		unsigned const q = ordinal < 2048 ? 0 : ordinal - 2048;
		unsigned const raw = ordinal < 2048 ? ordinal : q % 8 +
			8 * std::array<unsigned, 4>{0,15,16,31}[(q / 8) % 4] +
			256 * ((q / 32) % 8) + 2048 * (1 + q / 256);
		return raw + 16384 * ((raw % 2 + (raw / 2) % 4 + (raw / 8) % 32 +
			(raw / 256) % 8 + (raw / 2048) % 2 + (raw / 4096) % 2 + raw / 8192) % 2);
	}
	// Numeric boundaries x selectors x OVM, then remaining flags at zero/overflow.
	unsigned const raw = ordinal < 128 ? (ordinal % 16) + 16 * ((ordinal / 16) % 4) + 64 * (ordinal / 64) :
		ordinal < 368 ? ((ordinal - 128) % 2 ? 9 : 0) + 16 * (((ordinal - 128) / 2) % 4) + 64 * (2 + (ordinal - 128) / 8) :
		4096 + ((ordinal - 368) % 4) + 4 * std::array<unsigned, 4>{0,1,14,31}[((ordinal - 368) / 4) % 4] + 256 * ((ordinal - 368) / 16);
	bool const producer = raw >= 4096;
	unsigned const p = producer ? raw - 4096 : raw;
	unsigned const checkpoint = producer ? (p % 4 + (p / 4) % 32 + (p / 128) % 2 + p / 256) % 3 :
		(p % 16 + (p / 16) % 4 + (p / 64) % 32 + p / 2048) % 3;
	return raw + 4608 * checkpoint;
}

inline std::array<u64, 2> accumulator_logic_fixture(unsigned variant, bool after)
{
	bool const complement = variant >= 6144;
	unsigned const operation = complement ? 3 : variant % 3, selector = complement ? (variant - 6144) % 4 : (variant / 3) % 4;
	unsigned const pattern = complement ? ((variant - 6144) / 4) % 4 : (variant / 384) % 4;
	int const shift = complement ? 0 : int((variant / 12) % 32) - 16;
	std::array<u64, 2> values = {std::array<u64, 4>{0, 0xffffffffffULL, 0x123456789aULL, 0x8000000001ULL}[pattern],
		std::array<u64, 4>{0xffffffffffULL, 0x55aa55aa55ULL, 0x0f0ff0f00fULL, 0x7fffffffefULL}[pattern]};
	if (after)
	{
		unsigned const source = selector >> 1, destination = selector & 1;
		u64 result = 0;
		for (int bit = 0; bit < 40; ++bit)
		{
			int const source_bit = bit - shift;
			bool const left = BIT(values[destination], bit), right = source_bit >= 0 && source_bit < 40 && BIT(values[source], source_bit);
			if (operation == 0 ? left && right : operation == 1 ? left || right : operation == 2 ? left != right : !right) result |= u64(1) << bit;
		}
		values[destination] = result;
	}
	return values;
}

inline std::array<u64, 3> accumulator_shift_fixture(unsigned variant, bool after)
{
	bool const logical = variant % 2, sxm = (variant / 2048) % 2, ovm = (variant / 4096) % 2, carry = (variant / 8192) % 2;
	unsigned const selector = (variant / 2) % 4, pattern = (variant / 256) % 8;
	int const shift = int((variant / 8) % 32) - 16;
	u64 const value = std::array<u64, 8>{0, 1, 0x007fffffffULL, 0xff80000000ULL, 0x0180000000ULL, 0xff7fffffffULL, 0x80aa001234ULL, 0xffffffff55ULL}[pattern];
	std::array<u64, 3> expected = {selector & 2 ? 0x246813579aULL : value, selector & 2 ? value : 0x246813579aULL,
		u64(0x001b | (pattern & 1 ? 0x600 : 0) | (carry ? 0x800 : 0))};
	if (after)
	{
		u64 result = 0;
		int const width = logical ? 32 : 40;
		for (int bit = 0; bit < width; ++bit)
		{
			int const source_bit = bit - shift;
			bool const set = source_bit >= 0 && source_bit < width ? BIT(value, source_bit) :
				!logical && source_bit >= 40 && sxm && BIT(value, 39);
			if (set) result |= u64(1) << bit;
		}
		if (logical || shift)
		{
			bool const outgoing = shift && BIT(value, shift < 0 ? -shift - 1 : (logical ? 32 : 39) - shift);
			expected[2] = (expected[2] & ~u64(0x800)) | (outgoing ? 0x800 : 0);
		}
		if (!logical && shift > 0)
		{
			s64 const number = BIT(value, 39) ? s64(value) - (s64(1) << 40) : s64(value);
			s64 const scaled = number * (s64(1) << shift);
			if (scaled < -2147483648LL || scaled > 2147483647LL)
			{
				expected[2] |= selector & 1 ? 0x200 : 0x400;
				if (ovm) result = scaled < 0 ? 0xff80000000ULL : 0x007fffffffULL;
			}
		}
		expected[selector & 1] = result;
	}
	return expected;
}

struct negate_fixture
{
	u64 a, b, source, result;
	u16 opcode, st0, result_st0, st1;
	u32 base;
	unsigned prefix;
};

inline negate_fixture negate_case(unsigned variant, bool absolute)
{
	unsigned const v = variant % 4608;
	bool const producer = v >= 4096;
	unsigned const p = producer ? v - 4096 : v;
	unsigned const selectors = producer ? p & 3 : (p >> 4) & 3;
	unsigned const flags = producer ? (p >> 2) & 31 : (p >> 6) & 31;
	bool const source_b = BIT(selectors, 1), destination_b = BIT(selectors, 0);
	static constexpr u64 patterns[] = {0, 1, 0xffffffffffULL, 0xfffffffffeULL,
		0x007fffffffULL, 0xff80000000ULL, 0xff80000001ULL, 0x0080000000ULL,
		0x0080000001ULL, 0x8000000000ULL, 0x7fffffffffULL, 0x0100000000ULL,
		0xff00000000ULL, 0x0123456789ULL, 0xff12345678ULL, 0x000000ffffULL};
	u64 const initial = patterns[producer ? 3 : p & 15];
	u64 const source = producer ? (BIT(p, 8) ? 5 : 0) : initial;
	s64 const signed_source = BIT(source, 39) ? s64(source) - (s64(1) << 40) : s64(source);
	s64 const negated = absolute && signed_source >= 0 ? signed_source : -signed_source;
	bool const overflow = negated < -0x80000000LL || negated > 0x7fffffffLL;
	u64 const result = u64(overflow && BIT(flags, 0) ? std::clamp(negated, s64(-0x80000000LL), s64(0x7fffffffLL)) : negated) & 0xffffffffffULL;
	u16 const st0 = 0x101b | (BIT(flags, 1) ? 0x400 : 0) | (BIT(flags, 2) ? 0x200 : 0) | (BIT(flags, 3) ? 0x800 : 0);
	u16 const result_st0 = (st0 & ~u16(0x800)) | (!source ? 0x800 : 0) | (overflow ? destination_b ? 0x200 : 0x400 : 0);
	return {source_b ? 0x123456789aULL : initial, source_b ? initial : 0x2233445566ULL,
		source, result, u16((absolute ? 0xf485 : 0xf484) | (selectors << 8)), st0, result_st0,
		u16(0x800 | (BIT(flags, 0) ? 0x200 : 0) | (BIT(flags, 4) ? 0x100 : 0)),
		u32(BIT(p, producer ? 7 : 11) ? 0x20000 : 0), unsigned(producer)};
}

template<typename R>
clocked_register_case register_unary_case(unsigned ordinal, register_unary_kind kind)
{
	unsigned const key = register_unary_key(ordinal, kind);
	bool const logic = kind == register_unary_kind::logic, shift = kind == register_unary_kind::shift;
	bool const arithmetic = !logic && !shift;
	bool const complement = logic && key >= 6144;
	auto const f = arithmetic ? negate_case(key, kind == register_unary_kind::absolute) : negate_fixture{};
	unsigned const selector = arithmetic ? (f.opcode >> 8) & 3 : shift ? (key / 2) % 4 :
		complement ? (key - 6144) % 4 : (key / 3) % 4;
	int const amount = int((key / (shift ? 8 : 12)) % 32) - 16;
	u16 const opcode = arithmetic ? f.opcode : shift ?
		(key % 2 ? 0xf0e0 : 0xf460) | (selector << 8) | (amount & 31) : complement ?
		0xf493 | (selector << 8) : 0xf080 + (key % 3) * 0x20 + (selector << 8) + (amount & 31);
	u32 const base = arithmetic ? f.base : selector & 1 ? 0x20000 : 0;
	unsigned const prefix = arithmetic ? f.prefix : 0;
	unsigned const checkpoint = arithmetic ? std::array<unsigned, 3>{3 + prefix, 5 + prefix, 6 + prefix}[key / 4608] :
		(complement ? (key - 6144) / 32 : key / (shift ? 16384 : 3072)) ? 5 : 3;
	u16 const st1 = arithmetic ? f.st1 : shift ? 0x8c0 | (((key / 2048) % 2) << 8) | (((key / 4096) % 2) << 9) :
		0xac0 | ((complement ? ((key - 6144) / 16) % 2 : (key / 1536) % 2) << 8);
	clocked_register_case result{{}, checkpoint, arithmetic ? 12U : 16U};
	for (unsigned tick : {0U, 6U, 7U})
	{
		if (tick == 7 && !prefix) continue;
		u64 a, b, st0;
		if (arithmetic)
		{
			a = f.a; b = f.b;
			if (prefix && tick >= 6) (BIT(opcode, 9) ? b : a) = f.source;
			if (tick >= 6 + prefix) (BIT(opcode, 8) ? b : a) = f.result;
			st0 = tick >= 6 + prefix ? f.result_st0 : f.st0;
		}
		else if (shift)
		{
			auto const values = accumulator_shift_fixture(key, tick >= 6);
			a = values[0]; b = values[1]; st0 = values[2];
		}
		else
		{
			auto const values = accumulator_logic_fixture(key, tick >= 6);
			a = values[0]; b = values[1]; st0 = 0xae1b;
		}
		instruction_case state{"register-unary", opcode, {}, {}, {},
			{{R::STATE_A, a}, {R::STATE_B, b}, {R::STATE_ST0, st0}, {R::STATE_ST1, st1}, {R::STATE_ILLEGAL, 0}}, 0, 0};
		if (logic) state.expected.push_back({R::STATE_PMST, 1});
		state.reset_before = false;
		result.states.push_back({tick, std::move(state)});
	}
	auto &initial = result.states.front().observation;
	for (unsigned i = 0; i < (arithmetic ? 24U : 32U); ++i) initial.program.push_back({base + 0x600 + i, 0xf495});
	if (prefix) initial.program[0].value = (BIT(opcode, 9) ? 0xe900 : 0xe800) | u16(f.source);
	initial.program[prefix].value = opcode;
	// End with a real IDLE instead of executing unobserved padding indefinitely.
	initial.program[prefix + 1].value = 0xf4e1;
	for (auto const &value : initial.expected) initial.initial.push_back({value.index, value.value});
	initial.initial.insert(initial.initial.end(), {{R::STATE_IDLE, 0}, {R::STATE_PMST, arithmetic ? 0 : 1},
		{R::STATE_TCR, 0x10}, {R::STATE_IFR, 0}, {R::STATE_IMR, 0}, {STATE_GENPC, base + 0x600}});
	result.identity = key; result.single_fetch = base + 0x600 + prefix; result.forbid_data_access = true;
	return result;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_REGISTER_UNARY_H
