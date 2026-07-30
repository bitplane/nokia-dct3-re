// license:BSD-3-Clause
// copyright-holders:Gaz
// Standards-owned GSM idle-mode mobility configuration.

#ifndef MAME_NOKIA_GSM_MOBILITY_H
#define MAME_NOKIA_GSM_MOBILITY_H

#include <cstdint>

namespace gsm::mobility
{

// TS 44.018 T3212 is broadcast in SI3 as an eight-bit count of decihours.
// Zero disables periodic Location Updating; each nonzero unit is six minutes.
class periodic_update_timer
{
public:
	constexpr explicit periodic_update_timer(std::uint8_t decihours = 0) :
		m_decihours(decihours)
	{
	}

	constexpr std::uint8_t encoded() const { return m_decihours; }
	constexpr bool enabled() const { return m_decihours != 0; }
	constexpr unsigned minutes() const { return unsigned(m_decihours) * 6; }

	constexpr bool operator==(const periodic_update_timer &other) const
	{
		return m_decihours == other.m_decihours;
	}
	constexpr bool operator!=(const periodic_update_timer &other) const
	{
		return !(*this == other);
	}

private:
	std::uint8_t m_decihours;
};

} // namespace gsm::mobility

#endif // MAME_NOKIA_GSM_MOBILITY_H
