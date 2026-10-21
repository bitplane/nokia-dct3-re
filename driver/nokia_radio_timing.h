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

// Non-combined CCCH block-ending frame positions, also recovered in the
// NSB-7 MCU's common-control decoder table. Report completion, not start.
constexpr std::uint32_t common_control_block_end(std::uint32_t minimum)
{
	constexpr std::uint32_t endings[] = {9, 15, 19, 25, 29, 35, 39, 45, 49};
	const auto position = minimum % 51;
	for (const auto ending : endings)
		if (ending >= position)
			return minimum + ending - position;
	return minimum + 51 - position + endings[0];
}

} // namespace nokia::radio
#endif // MAME_NOKIA_NOKIA_RADIO_TIMING_H
