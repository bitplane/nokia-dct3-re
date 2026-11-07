// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_NOKIA_HAGAR_COMMAND_STATE_H
#define MAME_NOKIA_NOKIA_HAGAR_COMMAND_STATE_H

#include <cstdint>

// Partial NSM-3 observed-word contract. This does not assign physical register
// bitfields, PLL lock, gain, settling, RX enable or a serial shift-clock rate.
struct nokia_hagar_command_state
{
	std::uint16_t route = 0, pll = 0, configuration = 0;
	std::uint8_t seen = 0;
	std::uint32_t last_word = 0;

	void reset() { *this = {}; }
	void write(std::uint16_t control, std::uint32_t word)
	{
		last_word = word;
		if (control != 0x0100)
		{
			seen = 0;
			return;
		}
		switch (word >> 16)
		{
		case 0x4100: route = word; seen |= 1; break;
		case 0x4400: pll = word; seen |= 2; break;
		case 0x4a80: configuration = word; seen |= 4; break;
		case 0x4680:
			// Observed interleaved control words. Retain the arithmetic
			// candidate, but do not pretend this establishes RF readiness.
			break;
		default:
			// Unknown setup/control may invalidate earlier configuration.
			// Require all three observed families again; never retain a
			// stale channel across an unsupported transaction.
			seen = 0;
			break;
		}
	}

	// A unique firmware-requested RX key, not permission to emit a carrier or
	// feed samples. Requiring both co-varying controls avoids assigning band
	// ownership to either unisolated physical field.
	bool rx_candidate(std::uint16_t &key, std::uint64_t &frequency) const
	{
		if (seen != 7 || pll < 38) return false;
		unsigned const packed = pll - 38;
		if (packed & 0x40) return false;
		int const scalar = (packed >> 7) * 64 + (packed & 0x3f) + 38;
		if (route == 0x1054 && configuration == 0x0402)
		{
			if (scalar & 1) return false;
			int const channel = scalar / 2 - 4675;
			if (!((channel >= 0 && channel <= 124) || (channel >= -49 && channel <= -1))) return false;
			key = channel < 0 ? channel + 1024 : channel;
			frequency = 935'000'000LL + std::int64_t(channel) * 200'000;
			return true;
		}
		if (route == 0x082a && configuration == 0x0208)
		{
			int const channel = scalar - 8514;
			if (channel < 512 || channel > 885) return false;
			key = channel;
			frequency = 1'805'200'000ULL + std::uint64_t(channel - 512) * 200'000;
			return true;
		}
		return false;
	}
};

#endif // MAME_NOKIA_NOKIA_HAGAR_COMMAND_STATE_H
