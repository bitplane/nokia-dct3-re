// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_ATOMIC_ROUTES_H
#define MAME_NOKIA_TMS320C54X_TEST_ATOMIC_ROUTES_H

namespace c54x_test {

enum class atomic_fixture { none, basic, corpus };

struct atomic_phase_range
{
	unsigned first, last;
	atomic_fixture owner;
};

// Routing only: values and assertions remain in the owning fixture.
// test_c54x_atomic_routing checks the complete guard partition independently.
inline constexpr atomic_phase_range atomic_phase_ranges[] = {
	{22, 77, atomic_fixture::basic},
	{78, 86, atomic_fixture::corpus},
	{95, 99, atomic_fixture::corpus},
	{101, 116, atomic_fixture::corpus},
	{128, 128, atomic_fixture::corpus},
	{191, 386, atomic_fixture::corpus},
	{422, 422, atomic_fixture::corpus},
	{424, 525, atomic_fixture::corpus},
	{533, 547, atomic_fixture::corpus},
	{6501, 6502, atomic_fixture::corpus},
	{6504, 6504, atomic_fixture::corpus},
	{6515, 6516, atomic_fixture::corpus},
	{6520, 6521, atomic_fixture::corpus},
	{6524, 6528, atomic_fixture::corpus},
	{6530, 6532, atomic_fixture::corpus},
	{6534, 6557, atomic_fixture::corpus},
	{6560, 6560, atomic_fixture::corpus},
	{6563, 6567, atomic_fixture::corpus},
	{6570, 6571, atomic_fixture::corpus},
	{6573, 6592, atomic_fixture::corpus},
	{6600, 6855, atomic_fixture::corpus},
	{8000, 9023, atomic_fixture::corpus},
	{9100, 9107, atomic_fixture::basic},
	{9207, 9207, atomic_fixture::corpus},
	{9215, 9222, atomic_fixture::corpus},
	{9226, 9246, atomic_fixture::corpus},
	{9250, 9251, atomic_fixture::corpus},
	{9253, 9258, atomic_fixture::corpus},
	{90010, 90011, atomic_fixture::corpus},
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
