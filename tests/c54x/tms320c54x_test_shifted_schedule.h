// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_SHIFTED_SCHEDULE_H
#define MAME_NOKIA_TMS320C54X_TEST_SHIFTED_SCHEDULE_H

namespace c54x_test {

// Legacy variant numbers retain operand identities, not repeated checkpoint
// copies. Every non-pattern group still visits Decode, Access and Read.
struct shifted_memory_schedule
{
	static constexpr unsigned ram_cases = 49152, mapped_cases = 32768;
	static constexpr unsigned arithmetic_cases = ram_cases + mapped_cases;
	static constexpr unsigned subc_ram_cases = 6144, subc_mapped_cases = 4096;
	static constexpr unsigned cases = arithmetic_cases + subc_ram_cases + subc_mapped_cases;
	static constexpr unsigned end = 264192 + subc_mapped_cases;
	static constexpr unsigned bk_cases = 8192, bk_end = 12288 + 4096;
	static constexpr unsigned next(unsigned variant, bool bk)
	{
		unsigned const n = variant + 1;
		if (bk) return n == 4096 ? 12288 : n;
		if (n == ram_cases) return 147456;
		if (n == 147456 + mapped_cases) return 245760;
		if (n == 245760 + subc_ram_cases) return 264192;
		return n;
	}
	static constexpr unsigned local(unsigned variant, bool bk)
	{
		return bk ? variant % 4096 : variant >= 264192 ? variant - 264192 :
			variant >= 245760 ? variant - 245760 : variant >= 147456 ? variant - 147456 : variant;
	}
	static constexpr unsigned checkpoint_index(unsigned variant, bool bk)
	{
		unsigned const v = local(variant, bk);
		unsigned const stride = bk ? 8 : variant >= 264192 ? 16 : variant >= 245760 ? 24 : variant >= 147456 ? 64 : 96;
		unsigned const patterns = !bk && variant >= 245760 ? 8 : 4;
		unsigned const pattern = (v / stride) % patterns;
		unsigned const group = v % stride + stride * (v / (stride * patterns));
		return (pattern + group) % 3;
	}
	static constexpr unsigned case_key(unsigned variant, bool bk)
	{
		unsigned const v = local(variant, bk);
		unsigned const ordinal = bk ? v + (variant >= 12288 ? 4096 : 0) :
			variant >= 264192 ? arithmetic_cases + subc_ram_cases + v :
			variant >= 245760 ? arithmetic_cases + v : variant >= 147456 ? ram_cases + v : v;
		return ordinal * 3 + checkpoint_index(variant, bk);
	}
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_SHIFTED_SCHEDULE_H
