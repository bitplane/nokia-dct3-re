// license:BSD-3-Clause
// copyright-holders:Gaz
#include "nokia_dsp_compare_clock.h"
#include <cassert>

using namespace nokia_dsp_compare_clock;

int main()
{
	// A half-quarter write still targets the same reference-clock edge.
	assert(next(0, 0, 5000, 1).reference_ticks == 12);
	assert(next(6, 0, 5000, 1).reference_ticks == 12);
	assert(next(11, 0, 5000, 1).reference_ticks == 12);
	assert(next(12, 0, 5000, 1).reference_ticks == 60012);
	assert(next(4990 * 12, 0, 5000, 300).reference_ticks == 5300 * 12);
	assert(next(0, 100, 5000, 300).reference_ticks == 200 * 12);
	assert(fractional_attoseconds(12) == 923076923077ULL);
	assert(fractional_attoseconds(reference_hz) == 0);
	assert(fractional_attoseconds(reference_hz + 12) == 923076923077ULL);
	assert(fractional_attoseconds(reference_hz - 1) == 999999923076923077ULL);
	// Independent rational oracle: each13 reference clocks take exactly1us.
	for (std::uint64_t ticks=1; ticks<60000; ++ticks) {
		auto const expected = (ticks / 13) * 1'000'000'000'000ULL
			+ ((ticks % 13) * 1'000'000'000'000ULL + 12) / 13;
		assert(fractional_attoseconds(ticks) == expected);
		assert(next(ticks, 0, 5000, 300).reference_ticks > ticks);
	}
	// Nonzero epoch/base and a short reload use the same free-running phase.
	std::uint64_t const elapsed = 60ULL * reference_hz + 7;
	auto const edge = next(elapsed, 100, 1351, 27);
	assert(edge.reference_ticks > elapsed);
	assert((100 + edge.reference_ticks / 12) % 1351 == 27);
	assert((100 + (edge.reference_ticks - 1) / 12) % 1351 != 27);
}
