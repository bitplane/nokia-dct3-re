// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_NOKIA_DSP_COMPARE_CLOCK_H
#define MAME_NOKIA_NOKIA_DSP_COMPARE_CLOCK_H

#include <cstdint>

// Integer arithmetic for the backend's declared 13 MHz / 12 counter model.
// This establishes consistent phase, not a measured ROM6 peripheral clock.
namespace nokia_dsp_compare_clock {
constexpr std::uint32_t reference_hz = 13'000'000;
constexpr std::uint64_t attoseconds_per_second = 1'000'000'000'000'000'000ULL;

struct deadline {
	std::uint64_t reference_ticks;
	std::uint32_t quarters;
};

constexpr deadline next(std::uint64_t elapsed_reference_ticks,
		std::uint32_t base, std::uint32_t period, std::uint32_t target)
{
	auto const elapsed = elapsed_reference_ticks / 12;
	auto const counter = (base + elapsed) % period;
	auto const distance = (target + period - counter) % period;
	auto const quarters = std::uint32_t(distance ? distance : period);
	return {(elapsed + quarters) * 12, quarters};
}

// Earliest representable time at/after a reference tick, without a wide
// multiplication or accumulated floor(period)*ticks rounding error.
constexpr std::uint64_t fractional_attoseconds(std::uint64_t ticks)
{
	auto const remainder = ticks % reference_hz;
	auto const whole = (attoseconds_per_second / reference_hz) * remainder;
	auto const fraction = (attoseconds_per_second % reference_hz) * remainder;
	return whole + (fraction + reference_hz - 1) / reference_hz;
}
}

#endif // MAME_NOKIA_NOKIA_DSP_COMPARE_CLOCK_H
