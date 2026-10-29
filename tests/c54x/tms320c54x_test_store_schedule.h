// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_STORE_SCHEDULE_H
#define MAME_NOKIA_TMS320C54X_TEST_STORE_SCHEDULE_H

#include <array>

namespace c54x_test {

struct long_store_schedule
{
	static constexpr unsigned regular_cases = 12800, shifted_cases = 57344;
	static constexpr unsigned cases = regular_cases + shifted_cases;
	// Preserve fixture ordinals used by other consumers of the address factory.
	static constexpr unsigned shifted_first = 38400, end = shifted_first + shifted_cases;
	inline static constexpr std::array<unsigned, 3> regular_checkpoints{3, 6, 7}, shifted_checkpoints{4, 7, 8};
	static constexpr unsigned next(unsigned variant)
	{
		return variant + 1 == regular_cases ? shifted_first : variant + 1;
	}
	static constexpr unsigned checkpoint_index(unsigned variant)
	{
		bool const shifted = variant >= shifted_first;
		unsigned const v = shifted ? variant - shifted_first : variant, forms = shifted ? 7 : 25;
		unsigned const form = v % forms, offset = (v / forms) % 4;
		unsigned const selector = (v / (forms * 4)) % 4, flags = (v / (forms * 16)) % 4;
		unsigned const pattern = (v / (forms * 64)) % 4, major = v / (forms * 256);
		// Each fixed address/selector/flags/compatibility-or-shift group has
		// four patterns: all three checkpoints occur, one of them twice.
		return (form + offset + selector + flags + pattern + major) % 3;
	}
	static constexpr unsigned checkpoint(unsigned variant)
	{
		unsigned const index = checkpoint_index(variant);
		return (variant >= shifted_first ? shifted_checkpoints : regular_checkpoints)[index];
	}
	static constexpr unsigned case_key(unsigned variant)
	{
		unsigned const ordinal = variant < shifted_first ? variant : regular_cases + variant - shifted_first;
		return ordinal * 3 + checkpoint_index(variant);
	}
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_STORE_SCHEDULE_H
