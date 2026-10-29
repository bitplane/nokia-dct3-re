// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_RMW_SCHEDULE_H
#define MAME_NOKIA_TMS320C54X_TEST_RMW_SCHEDULE_H

#include <array>

namespace c54x_test {

struct long_rmw_schedule
{
	static constexpr unsigned operations = 3, address_shapes = 100, flags = 8, values = 8;
	static constexpr unsigned unique_cases = operations * address_shapes * flags * values * 2;
	inline static constexpr std::array<unsigned, 4> checkpoints{4, 6, 7, 8};

	// Each operation/address/compatibility/flag group has all eight value
	// pairs. Rotate their checkpoints so each saved shape is exercised twice,
	// without repeating the entire value matrix four times.
	static constexpr unsigned checkpoint_index(unsigned variant)
	{
		return (variant / (operations * address_shapes * flags) + variant % operations +
			 (variant / operations) % address_shapes) % checkpoints.size();
	}
	static constexpr unsigned case_key(unsigned variant)
	{
		return variant * checkpoints.size() + checkpoint_index(variant);
	}
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_RMW_SCHEDULE_H
