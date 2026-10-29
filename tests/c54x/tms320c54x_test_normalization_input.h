// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_NORMALIZATION_INPUT_H
#define MAME_NOKIA_TMS320C54X_TEST_NORMALIZATION_INPUT_H

#include <array>

namespace c54x_test {

// Fixture-owned bit-count and bit-position expectations, not CPU helpers.
struct exponent_fixture
{
	u64 value;
	bool b, producer;
	unsigned gap;
	u16 st0, st1;
	u32 base;
	u64 source() const { return producer ? u16(value) : value; }
	u16 exponent() const
	{
		if (!source()) return 0;
		u64 magnitude = source();
		if (magnitude & 0x8000000000ULL) magnitude = (~magnitude) & 0xffffffffffULL;
		unsigned significant = 0;
		while (magnitude) { ++significant; magnitude >>= 1; }
		return u16(s16(31 - int(significant)));
	}
};
struct normalization_fixture
{
	unsigned selector, prefix, gap, read_cycle, execute_cycle;
	u16 t, st0, st1, result_st0;
	u32 base;
	std::array<u64, 2> before, after;
};

inline exponent_fixture exponent_case(unsigned v)
{
	unsigned const flags = (v / 96) % 8;
	return {std::array<u64, 16>{0, 1, 2, 0x7fff, 0x8000, 0x7fffffff, 0x80000000ULL, 0xffffffffULL, 0x0100000000ULL, 0x7fffffffffULL, 0x8000000000ULL, 0xffffffffffULL, 0xffffffffcbULL, 0x0785432105ULL, 0xff80000000ULL, 0xff00000000ULL}[v % 16],
		bool((v / 16) % 2), bool((v / 768) % 2), (v / 32) % 3, u16(0xa602 | (flags & 4 ? 0x1800 : 0)), u16(0x800 | ((flags & 3) << 8)), (v / 1536) % 2 ? 0x20000U : 0};
}

inline normalization_fixture normalization_case(unsigned variant, bool producer, bool edge)
{
	unsigned const selector = variant % 4;
	unsigned const pattern = producer ? (variant / 4) % 16 : edge ? (variant / 64) % 8 : (variant / 192) % 8;
	unsigned const flags = producer ? (variant / 192) % 4 : edge ? (variant / 512) % 8 : (variant / 1536) % 8;
	unsigned const gap = producer ? (variant / 64) % 3 : 0, prefix = unsigned(producer);
	u32 const base = (producer ? (variant / 768) % 2 : edge ? (variant / 4096) % 2 : (variant / 12288) % 2) ? 0x20000 : 0;
	std::array<u64, 2> before = producer ? std::array<u64, 2>{exponent_case(pattern).value, exponent_case((pattern + 7) % 16).value} : std::array<u64, 2>{
		std::array<u64, 8>{0, 1, 0x7fffffff, 0xff80000000ULL, 0x7fffffffffULL, 0x8000000000ULL, 0xffffffffffULL, 0x123456789aULL}[pattern],
		std::array<u64, 8>{1, 0xffffffffULL, 0xff80000000ULL, 0x7fffffff, 0xffffffffffULL, 0x123456789aULL, 0x8000000000ULL, 0x7fffffffffULL}[pattern]};
	u64 const source = before[selector >> 1];
	u16 const st0 = 0x1004 | ((producer ? pattern & 1 : flags & 4) ? 0x0e00 : 0);
	u16 const st1 = 0x813 | ((flags & 3) << 8);
	int const basic_shift = int((variant / 4) % 48) - 16;
	u16 const t = producer ? 0xa5c1 : edge ? u16(0xa5e0 | ((variant / 4) % 16)) : u16(0xa5c0 | (basic_shift & 63));
	exponent_fixture const exponent{source, false, false, 0, 0, 0, 0};
	int const shift = producer ? gap ? s16(exponent.exponent()) : 1 : edge ? int((variant / 4) % 16) - 16 : basic_shift;
	u64 result = 0;
	bool overflow = false;
	{
		bool const sign = source & 0x8000000000ULL;
		for (int bit = 0; bit < 40; ++bit)
		{
			int const from = bit - shift;
			if ((from >= 0 && from < 40 && BIT(source, from)) || (shift < 0 && from >= 40 && (flags & 1) && sign)) result |= u64(1) << bit;
		}
		if (shift >= 0)
			// Inspect the wide mathematical result without overflowing s64
			// when a 40-bit source is shifted left by as many as 31 bits.
			for (int bit = 31; bit <= 70; ++bit)
			{
				int const from = bit - shift;
				bool const value = from < 0 ? false : from < 40 ? bool(BIT(source, from)) : sign;
				overflow |= value != sign;
			}
		if (overflow && (flags & 2)) result = sign ? 0xff80000000ULL : 0x7fffffff;
	}
	auto after = before;
	after[selector & 1] = result;
	return {selector, prefix, gap, 5 + prefix + gap, 6 + prefix + gap, t, st0, st1,
		u16(st0 | (overflow ? selector & 1 ? 0x200 : 0x400 : 0)), base, before, after};
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_NORMALIZATION_INPUT_H
