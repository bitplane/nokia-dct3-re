// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_PERIPHERAL_SCHEDULE_H
#define MAME_NOKIA_TMS320C54X_TEST_PERIPHERAL_SCHEDULE_H

namespace c54x_test::shifted_bus_schedule {

inline constexpr unsigned boundary_cases = 2 * 4 * 5 * 4 * 2 * 4 * 2 * 3;
inline constexpr unsigned cases = boundary_cases + 62 * 4 * 5 * 3;

// Original matrix keys remain the identities. Fully cross arithmetic, flags,
// pages and snapshots at 0020/005f; interior addresses retain every form,
// shift and snapshot with A, unsigned 8000, OVM clear and program page zero.
constexpr unsigned key(unsigned index)
{
	if (index < boundary_cases)
		return 64 * (index / 2) + (index % 2 ? 63 : 0);
	index -= boundary_cases;
	unsigned const address = 1 + index % 62;
	index /= 62;
	unsigned const shift = index % 4;
	index /= 4;
	unsigned const form = index % 5;
	unsigned const snapshot = index / 5;
	constexpr unsigned pattern_stride = 64 * 4 * 5 * 4 * 2;
	constexpr unsigned snapshot_stride = pattern_stride * 4 * 2;
	return address + 64 * shift + 64 * 4 * form + 2 * pattern_stride + snapshot_stride * snapshot;
}

} // namespace c54x_test::shifted_bus_schedule

#endif // MAME_NOKIA_TMS320C54X_TEST_PERIPHERAL_SCHEDULE_H
