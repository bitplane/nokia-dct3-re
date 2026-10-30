// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_CIRCULAR_MOVE_SCHEDULE_H
#define MAME_NOKIA_TMS320C54X_TEST_CIRCULAR_MOVE_SCHEDULE_H

#include <array>

namespace c54x_test::circular_move_schedule {

inline constexpr std::array<unsigned, 4> inputs = {43008, 1344, 48384, 21504};
inline constexpr std::array<unsigned, 4> snapshots = {3, 3, 2, 3};
inline constexpr std::array<std::array<unsigned, 6>, 4> dimensions = {{
	{1024, 21, 2, 1, 1, 1}, {21, 8, 4, 2, 1, 1},
	{18, 32, 2, 21, 2, 1}, {32, 2, 21, 4, 2, 2}
}};
inline constexpr unsigned cases = 43008 + 1344 + 48384 + 21504;

// Retain each original non-checkpoint ordinal once. Summing its decoded
// dimensions distributes snapshots within every destination-kind cohort;
// numeric/address references are still supplied by the original factory.
constexpr unsigned key(unsigned ordinal)
{
	unsigned base = 0;
	for (unsigned cohort = 0; cohort < inputs.size(); ++cohort)
	{
		if (ordinal < inputs[cohort])
		{
			unsigned value = ordinal, tag = 0;
			for (auto radix : dimensions[cohort]) { tag += value % radix; value /= radix; }
			return base + ordinal + inputs[cohort] * (tag % snapshots[cohort]);
		}
		ordinal -= inputs[cohort];
		base += inputs[cohort] * snapshots[cohort];
	}
	return base;
}

} // namespace c54x_test::circular_move_schedule

#endif // MAME_NOKIA_TMS320C54X_TEST_CIRCULAR_MOVE_SCHEDULE_H
