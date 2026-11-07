// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_NOKIA_RF_LAB_SIGNAL_H
#define MAME_NOKIA_NOKIA_RF_LAB_SIGNAL_H
#include <cstdint>

namespace nokia_rf_lab {
// GSM 05.02 5.2.4 and table 3: 148 zero bits in TS0, frames 0/10/20/30/40.
// Ideal rectangular ADC-pin stimulus only, not a BTS power envelope or air link.
constexpr bool fcch_active(std::uint64_t reference_tick)
{
	unsigned const frame = (reference_tick / 60'000) % 51;
	return frame <= 40 && frame % 10 == 0 && reference_tick % 60'000 < 148 * 48;
}
}
#endif // MAME_NOKIA_NOKIA_RF_LAB_SIGNAL_H
