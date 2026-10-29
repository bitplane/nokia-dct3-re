// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_DUAL_INPUT_H
#define MAME_NOKIA_TMS320C54X_TEST_DUAL_INPUT_H

#include <algorithm>
#include <array>

namespace c54x_test {

// Fixture-owned inputs and signed-integer expectations, shared with mapped
// and parallel consumers. No CPU implementation helper supplies a result.
struct dual_multiply_fixture
{
	unsigned x, y, destination, snapshot, flags, source, operation;
	bool linear;
	u32 base;
	std::array<u16, 4> initial, final;
	u16 xaddress, yaddress, xword, yword, st0, st1, pmst, result_st0, stored;
	u64 result;
};

inline u64 dual_macsu_seed(unsigned variant, bool edge)
{
	unsigned const v = !edge && variant >= 18432 ? (variant - 18432) / 4096 : variant / 512;
	return std::array<u64, 8>{0, 1, 0xffffffffffULL, 0x7fffffff, 0xff80000000ULL, 0x0180000000ULL, 0xfe7fffffffULL, 0x123456789aULL}[v % 8];
}

inline dual_multiply_fixture dual_multiply_case(unsigned variant, bool edge, bool alu = false, bool macsu = false, bool signed_mac = false, bool family = false, bool parallel = false)
{
	dual_multiply_fixture f{};
	unsigned const family_index = family ? variant / (edge ? 16U : 215040U) : 0;
	if (family) variant %= edge ? 16U : 215040U;
	bool const subtract = alu && variant >= (edge ? 16U : 43008U);
	if (alu) variant %= edge ? 16U : 43008U;
	bool const arithmetic = variant >= 18432 && !edge;
	unsigned const v = edge ? variant : arithmetic ? variant - 18432 : variant;
	unsigned const layout = edge || arithmetic ? 0 : (v / 512) % 3;
	f.x = edge || arithmetic ? 8 : v & 15; f.y = edge || arithmetic ? 9 : (v >> 4) & 15;
	f.destination = arithmetic ? (v / 64) % 2 : edge ? 0 : (v / 256) % 2;
	f.source = f.destination ^ (family_index / 4);
	f.operation = family_index % 4;
	f.flags = arithmetic ? (v / 128) % 32 : edge ? 0 : ((v / 64) ^ (v / 512)) % 32;
	f.linear = edge || arithmetic || (v / 1536) % 2;
	f.base = (edge ? (v / 4) % 2 : arithmetic ? (v / (macsu ? 32768 : 4096)) % 2 : (v / 3072) % 2) ? 0x20000 : 0;
	f.snapshot = edge ? v / 8 : arithmetic ? v / (macsu ? 65536 : 8192) : v / 6144;
	for (unsigned i = 0; i < 4; ++i) f.initial[i] = layout == 2 ? 0x1048 + i : 0x100f + (layout ? 0 : 0x100 * i);
	if (edge)
	{
		unsigned const kind = v % 4;
		if (kind < 2) f.initial[0] = kind ? 0x20 : alu ? 0x06 : 0x0e;
		else f.initial[1] = kind == 2 ? (alu ? 0x06 : 0x0e) : 0x33;
	}
	f.xaddress = f.initial[f.x & 3]; f.yaddress = f.initial[f.y & 3];
	static constexpr std::array<u16, 8> words{0, 1, 0xffff, 0x8000, 0x7fff, 0x1234, 0x5555, 0xaaaa};
	f.xword = words[arithmetic ? v % 8 : (v / 16) % 8];
	f.yword = f.xaddress == f.yaddress ? f.xword : words[arithmetic ? (v / 8) % 8 : (v / 128) % 8];
	f.final = f.initial;
	auto modify = [&f](u16 a, unsigned mode) -> u16 {
		if (mode == 1) return a - 1;
		if (mode == 2) return a + 1;
		if (mode == 3) return f.linear ? a + 3 : (a & 0xfff0) | ((a + 3) & 15);
		return a;
	};
	if (!edge)
	{
		f.final[f.x & 3] = modify(f.initial[f.x & 3], f.x >> 2);
		if ((f.x & 3) != (f.y & 3)) f.final[f.y & 3] = modify(f.initial[f.y & 3], f.y >> 2);
	}
	f.st0 = 0xa002 | (f.flags & 16 ? 0x1e00 : 0);
	f.st1 = 0x800 | (f.flags & 1 ? 0x40 : 0) | (f.flags & 2 ? 0x200 : 0) | (f.flags & 8 ? 0x100 : 0) | (v & 1 ? 0x4020 : 0);
	f.pmst = f.flags & 4 ? 2 : 0;
	if (parallel)
	{
		f.st1 |= v & 31;
		f.pmst |= (v / 32) & 1;
		u64 const source = f.source == f.destination ? dual_macsu_seed(variant, edge) : f.source ? 0xfeabcd9876ULL : 0x123456789aULL;
		int const shift = int(v & 31) - (v & 16 ? 32 : 0);
		s64 const signed_source = source & (u64(1) << 39) ? s64(source) - (s64(1) << 40) : s64(source);
		u64 shifted = shift < 0 ? u64(f.flags & 8 ? signed_source >> -shift : s64(source >> -shift)) & 0xffffffffffULL : (source << shift) & 0xffffffffffULL;
		if (f.pmst & 1)
		{
			if (f.flags & 8) { s64 const number = shifted & (u64(1) << 39) ? s64(shifted) - (s64(1) << 40) : s64(shifted); shifted = u64(std::clamp(number, s64(-2147483648LL), s64(2147483647LL))); }
			else shifted = std::min(shifted, u64(0xffffffff));
		}
		f.stored = u16(shifted >> 16);
	}
	s64 const product = s64(s16(f.xword)) * s64(s16(f.yword)) * (f.flags & 1 ? 2 : 1);
	bool const overflow = product < -2147483648LL || product > 2147483647LL;
	f.result = u64(overflow && (f.flags & 2) ? (product < 0 ? -2147483648LL : 2147483647LL) : product) & 0xffffffffffULL;
	f.result_st0 = f.st0 | (overflow ? f.destination ? 0x200 : 0x400 : 0);
	if (macsu)
	{
		s64 product = (signed_mac ? s64(s16(f.xword)) : s64(f.xword)) * s16(f.yword) * (f.flags & 1 ? 2 : 1);
		if ((f.flags & 7) == 7) product = std::clamp(product, s64(-2147483648LL), s64(2147483647LL));
		u64 const seed = dual_macsu_seed(variant, edge);
		s64 result = (BIT(seed, 39) ? s64(seed) - (s64(1) << 40) : s64(seed)) + (f.operation & 2 ? -product : product);
		if (f.operation & 1)
		{
			s64 const biased = result + 32768;
			result = (biased >= 0 ? biased / 65536 : -((-biased + 65535) / 65536)) * 65536;
		}
		bool const ov = result < -2147483648LL || result > 2147483647LL;
		f.result = u64(ov && (f.flags & 2) ? std::clamp(result, s64(-2147483648LL), s64(2147483647LL)) : result) & 0xffffffffffULL;
		f.result_st0 = f.st0 | (ov ? f.destination ? 0x200 : 0x400 : 0);
	}
	if (alu)
	{
		s64 const left = s64(f.flags & 8 ? s16(f.xword) : s32(f.xword)) * 65536;
		s64 const right = s64(f.flags & 8 ? s16(f.yword) : s32(f.yword)) * 65536;
		s64 const result = subtract ? left - right : left + right;
		bool const ov = result < -2147483648LL || result > 2147483647LL;
		bool const carry = subtract ? f.xword >= f.yword : u32(f.xword) + f.yword > 65535;
		f.result = u64(ov && (f.flags & 2) ? (result < 0 ? -2147483648LL : 2147483647LL) : result) & 0xffffffffffULL;
		f.result_st0 = (f.st0 & ~0x800) | (carry ? 0x800 : 0) | (ov ? f.destination ? 0x200 : 0x400 : 0);
	}
	return f;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_DUAL_INPUT_H
