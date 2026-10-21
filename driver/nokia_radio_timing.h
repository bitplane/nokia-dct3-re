// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_NOKIA_RADIO_TIMING_H
#define MAME_NOKIA_NOKIA_RADIO_TIMING_H

#include <cstdint>
#include <optional>

namespace nokia::radio {

// Recovered IDLE_RA timing for the qualified non-combined CCCH profile.
// frame is the current GSM frame for mode 0, the previous access frame for
// mode 1. Combined-CCCH RACH-slot mapping is a separate contract.
constexpr std::optional<std::uint32_t> access_delay(
		std::uint32_t frame, std::uint8_t mode,
		std::uint16_t base, std::uint8_t slots)
{
	if (mode == 0)
		return slots;
	if (mode != 1)
		return std::nullopt;
	const std::uint16_t delta = base - std::uint16_t(frame);
	if (delta >= 0x8000)
		return std::nullopt;
	return std::uint32_t(delta) + slots;
}

constexpr bool frame_due(std::uint32_t frame, std::uint32_t deadline)
{
	return std::uint32_t(frame - deadline) < 0x80000000U;
}

} // namespace nokia::radio
#endif // MAME_NOKIA_NOKIA_RADIO_TIMING_H
