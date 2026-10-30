// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_ATOMIC_ROUTES_H
#define MAME_NOKIA_TMS320C54X_TEST_ATOMIC_ROUTES_H

namespace c54x_test {

enum class atomic_fixture { none, basic, corpus, reference };

struct atomic_phase_range
{
	unsigned first, last;
	atomic_fixture owner;
};

// Routing only: values and assertions remain in the owning fixture.
// test_c54x_atomic_routing checks the complete guard partition independently.
inline constexpr atomic_phase_range atomic_phase_ranges[] = {
	{5, 77, atomic_fixture::basic},
	{78, 86, atomic_fixture::corpus},
	{95, 99, atomic_fixture::corpus},
	{101, 116, atomic_fixture::corpus},
	{128, 128, atomic_fixture::corpus},
	{185, 422, atomic_fixture::corpus},
	{424, 525, atomic_fixture::corpus},
	{533, 547, atomic_fixture::corpus},
	{548, 726, atomic_fixture::reference},
	{728, 738, atomic_fixture::reference},
	{740, 749, atomic_fixture::reference},
	{6501, 6502, atomic_fixture::corpus},
	{6503, 6503, atomic_fixture::reference},
	{6504, 6504, atomic_fixture::corpus},
	{6505, 6505, atomic_fixture::reference},
	{6508, 6510, atomic_fixture::reference},
	{6515, 6516, atomic_fixture::corpus},
	{6520, 6521, atomic_fixture::corpus},
	{6522, 6523, atomic_fixture::reference},
	{6524, 6528, atomic_fixture::corpus},
	{6529, 6529, atomic_fixture::reference},
	{6530, 6532, atomic_fixture::corpus},
	{6533, 6533, atomic_fixture::reference},
	{6534, 6557, atomic_fixture::corpus},
	{6558, 6559, atomic_fixture::reference},
	{6560, 6560, atomic_fixture::corpus},
	{6561, 6561, atomic_fixture::reference},
	{6563, 6567, atomic_fixture::corpus},
	{6570, 6571, atomic_fixture::corpus},
	{6572, 6572, atomic_fixture::reference},
	{6573, 6592, atomic_fixture::corpus},
	{6600, 6855, atomic_fixture::corpus},
	{7031, 7031, atomic_fixture::reference},
	{7041, 7041, atomic_fixture::reference},
	{8000, 9023, atomic_fixture::corpus},
	{9100, 9107, atomic_fixture::basic},
	{9200, 9205, atomic_fixture::reference},
	{9207, 9207, atomic_fixture::corpus},
	{9211, 9214, atomic_fixture::reference},
	{9215, 9222, atomic_fixture::corpus},
	{9223, 9225, atomic_fixture::reference},
	{9226, 9246, atomic_fixture::corpus},
	{9248, 9249, atomic_fixture::reference},
	{9250, 9251, atomic_fixture::corpus},
	{9252, 9252, atomic_fixture::reference},
	{9253, 9258, atomic_fixture::corpus},
	{9400, 9403, atomic_fixture::reference},
	{9410, 9413, atomic_fixture::reference},
	{40000, 40008, atomic_fixture::reference},
	{41000, 41511, atomic_fixture::reference},
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
