// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_ATOMIC_ROUTES_H
#define MAME_NOKIA_TMS320C54X_TEST_ATOMIC_ROUTES_H

namespace c54x_test {

enum class atomic_fixture { none, corpus };

struct atomic_phase_range
{
	unsigned first, last;
	atomic_fixture owner;
};

// Routing only: values and assertions remain in the owning fixture.
// test_c54x_atomic_routing checks the complete guard partition independently.
inline constexpr atomic_phase_range atomic_phase_ranges[] = {
	{128, 128, atomic_fixture::corpus},
	{335, 336, atomic_fixture::corpus},
	{342, 345, atomic_fixture::corpus},
	{354, 357, atomic_fixture::corpus},
	{361, 365, atomic_fixture::corpus},
	{367, 368, atomic_fixture::corpus},
	{373, 373, atomic_fixture::corpus},
	{376, 386, atomic_fixture::corpus},
	{422, 422, atomic_fixture::corpus},
	{424, 525, atomic_fixture::corpus},
	{533, 547, atomic_fixture::corpus},
	{6501, 6502, atomic_fixture::corpus},
	{6515, 6516, atomic_fixture::corpus},
	{6525, 6528, atomic_fixture::corpus},
	{6530, 6530, atomic_fixture::corpus},
	{6536, 6544, atomic_fixture::corpus},
	{6547, 6557, atomic_fixture::corpus},
	{6560, 6560, atomic_fixture::corpus},
	{6565, 6567, atomic_fixture::corpus},
	{6571, 6571, atomic_fixture::corpus},
	{6573, 6588, atomic_fixture::corpus},
	{6591, 6592, atomic_fixture::corpus},
	{6600, 6855, atomic_fixture::corpus},
	{9023, 9023, atomic_fixture::corpus},
	{9220, 9222, atomic_fixture::corpus},
	{9227, 9246, atomic_fixture::corpus},
	{9250, 9251, atomic_fixture::corpus},
	{9253, 9258, atomic_fixture::corpus},
	{1000000, 1000000, atomic_fixture::corpus},
};

constexpr atomic_fixture atomic_fixture_for(unsigned phase)
{
	for (auto const &range : atomic_phase_ranges)
		if (phase >= range.first && phase <= range.last)
			return range.owner;
	return atomic_fixture::none;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_ATOMIC_ROUTES_H
