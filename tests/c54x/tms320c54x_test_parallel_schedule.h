// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_PARALLEL_SCHEDULE_H
#define MAME_NOKIA_TMS320C54X_TEST_PARALLEL_SCHEDULE_H

#include <array>

namespace c54x_test::parallel_schedule {

inline constexpr unsigned inputs_per_form = 18432 + 16384;

// Address cohorts change status as well as snapshot: retain them in full.
// Numeric copies have identical consumed fields; vary the one selected
// snapshot across word, accumulator/T, shift, flags, page and opcode form.
constexpr unsigned key(unsigned ordinal)
{
	unsigned const form = ordinal / inputs_per_form, value = ordinal % inputs_per_form;
	if (value < 18432) return form * 67584 + value;
	unsigned n = value - 18432, tag = form;
	for (unsigned radix : std::array<unsigned, 5>{8, 8, 8, 16, 2}) { tag += n % radix; n /= radix; }
	return form * 67584 + value + 16384 * (tag % 3);
}

} // namespace c54x_test::parallel_schedule

#endif // MAME_NOKIA_TMS320C54X_TEST_PARALLEL_SCHEDULE_H
