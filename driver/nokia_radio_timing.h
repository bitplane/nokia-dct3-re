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

// TS45.002 table5 restricts combined CCCH/SDCCH to the first three
// common-control blocks. Report completion, not start.
constexpr std::uint32_t common_control_block_end(std::uint32_t minimum, bool combined = false)
{
	constexpr std::uint32_t endings[] = {9, 15, 19, 25, 29, 35, 39, 45, 49};
	const auto position = minimum % 51;
	for (const auto ending : endings)
		if (ending >= position && (!combined || ending <= 19))
			return minimum + ending - position;
	return minimum + 51 - position + endings[0];
}

// Combined CCCH with BS_AG_BLKS_RES=0: all three blocks may carry PCH.
constexpr std::optional<unsigned> combined_ccch_burst(std::uint64_t frame)
{
	unsigned const position=frame%51;
	constexpr unsigned starts[]={6,12,16};
	for (unsigned start : starts)
		if (position>=start && position<start+4) return position-start;
	return std::nullopt;
}

struct paging_slot
{
	unsigned multiframe_phase;
	unsigned frame_offset;
};

// TS45.002 6.5.2/6.5.3 and table5, one CCCH. Reserved AGCH blocks
// precede the available PCH blocks; multiframes is decoded BS_PA_MFRMS.
constexpr std::optional<paging_slot> subscriber_paging_slot(
		unsigned imsi_mod1000, bool combined, unsigned reserved, unsigned multiframes)
{
	constexpr unsigned starts[]={6,12,16,22,26,32,36,42,46};
	unsigned const total=combined ? 3 : 9;
	if (imsi_mod1000>=1000 || reserved>7 || reserved>=total || multiframes<2 || multiframes>9)
		return std::nullopt;
	unsigned const available=total-reserved;
	unsigned const group=imsi_mod1000%(available*multiframes);
	return paging_slot{group/available,starts[reserved+group%available]};
}

} // namespace nokia::radio
#endif // MAME_NOKIA_NOKIA_RADIO_TIMING_H
