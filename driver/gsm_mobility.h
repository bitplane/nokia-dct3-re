// license:BSD-3-Clause
// copyright-holders:Gaz
// Standards-owned GSM idle-mode mobility configuration.

#ifndef MAME_NOKIA_GSM_MOBILITY_H
#define MAME_NOKIA_GSM_MOBILITY_H

#include <array>
#include <cstdint>

namespace gsm::mobility
{

// TS 45.010 synchronization-channel frame positions and TS 44.018
// Synchronization Channel Information. The four information octets carry the
// six-bit BSIC together with the reduced TDMA frame number.
constexpr std::uint32_t synchronization_frame(std::uint32_t frame_number)
{
	constexpr std::uint32_t modulus = 2'715'648;
	frame_number %= modulus;
	const std::uint32_t position = frame_number % 51;
	if (position >= 1)
		return frame_number - position + 1 + ((position - 1) / 10) * 10;
	return (frame_number + modulus - 10) % modulus;
}

constexpr std::array<std::uint8_t, 4> synchronization_channel_information(
		std::uint8_t bsic, std::uint32_t frame_number)
{
	const std::uint32_t sch_frame = synchronization_frame(frame_number);
	const std::uint32_t t1 = (sch_frame / 1326) % 2048;
	const std::uint32_t t2 = sch_frame % 26;
	const std::uint32_t t3_prime = ((sch_frame % 51) - 1) / 10;
	bsic &= 0x3f;
	return {
		std::uint8_t((bsic << 2) | (t1 >> 9)),
		std::uint8_t(t1 >> 1),
		std::uint8_t(((t1 & 1) << 7) | (t2 << 2) | (t3_prime >> 1)),
		std::uint8_t((t3_prime & 1) << 7)
	};
}

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

// TS 45.008 idle-mode downlink signalling failure criterion. The counter is
// initialized to the nearest integer to 90 / BS_PA_MFRMS, incremented for a
// successfully decoded paging block and decremented by four for a failed one.
// Layer 1 reports loss when the counter reaches zero.
class downlink_signalling_counter
{
public:
	constexpr explicit downlink_signalling_counter(
			std::uint8_t bs_pa_mfrms = 2) :
		m_ceiling(initial_value(bs_pa_mfrms)),
		m_value(m_ceiling)
	{
	}

	static constexpr std::int16_t initial_value(std::uint8_t bs_pa_mfrms)
	{
		return bs_pa_mfrms >= 2 && bs_pa_mfrms <= 9 ?
				std::int16_t((90 + bs_pa_mfrms / 2) / bs_pa_mfrms) : 45;
	}

	constexpr std::int16_t value() const { return m_value; }
	constexpr std::int16_t ceiling() const { return m_ceiling; }
	constexpr bool failed() const { return m_value <= 0; }

	constexpr bool observe(bool decoded)
	{
		if (decoded)
		{
			if (m_value < m_ceiling)
				++m_value;
		}
		else if (m_value > 0)
		{
			m_value -= 4;
			if (m_value < 0)
				m_value = 0;
		}
		return failed();
	}

	constexpr void restore(std::int16_t value)
	{
		m_value = value < 0 ? 0 : (value > m_ceiling ? m_ceiling : value);
	}

private:
	std::int16_t m_ceiling;
	std::int16_t m_value;
};

// Standards-level identity and radio data for a broadcast cell.  These values
// belong to network topology, not to a handset acquisition strategy or Nokia
// DSP packet grammar.
struct location_area_identity
{
	// TS 24.008 encodes MCC/MNC as three semi-octet PLMN bytes.
	std::array<std::uint8_t, 3> plmn{ 0x00, 0xf1, 0x10 };
	std::uint16_t lac = 1;

	constexpr bool operator==(const location_area_identity &other) const
	{
		return plmn[0] == other.plmn[0] &&
				plmn[1] == other.plmn[1] &&
				plmn[2] == other.plmn[2] &&
				lac == other.lac;
	}
};

struct cell
{
	std::uint16_t arfcn = 1;
	std::uint8_t bsic = 0x12;
	location_area_identity location{};
	std::uint16_t identity = 1;
	std::int8_t rxlev_dbm = -60;
	std::uint8_t rxlev_access_min = 0;
	std::uint16_t access_class_barred = 0;
	bool cell_barred = false;
	bool available = true;

	constexpr bool same_location_area(const cell &other) const
	{
		return location == other.location;
	}

	constexpr bool operator==(const cell &other) const
	{
		return arfcn == other.arfcn &&
				bsic == other.bsic &&
				location == other.location &&
				identity == other.identity &&
				rxlev_dbm == other.rxlev_dbm &&
				rxlev_access_min == other.rxlev_access_min &&
				access_class_barred == other.access_class_barred &&
				cell_barred == other.cell_barred &&
				available == other.available;
	}
};

// A deliberately bounded laboratory topology.  Selection and reselection
// remain firmware-owned; this object only supplies independently addressable
// cells and never chooses one on the handset's behalf.
class topology
{
public:
	static constexpr unsigned maximum_cells = 2;

	constexpr topology() :
		m_cells{},
		m_count(1)
	{
	}

	constexpr unsigned size() const { return m_count; }

	constexpr const cell *at(unsigned index) const
	{
		return index < m_count ? &m_cells[index] : nullptr;
	}

	constexpr cell *at(unsigned index)
	{
		return index < m_count ? &m_cells[index] : nullptr;
	}

	constexpr const cell *find(std::uint16_t arfcn) const
	{
		for (unsigned index = 0; index < m_count; ++index)
			if (m_cells[index].arfcn == arfcn)
				return &m_cells[index];
		return nullptr;
	}

	constexpr bool set(unsigned index, const cell &value)
	{
		if (index >= maximum_cells)
			return false;
		m_cells[index] = value;
		if (m_count <= index)
			m_count = index + 1;
		return true;
	}

	constexpr void use_single_cell()
	{
		m_count = 1;
	}

private:
	std::array<cell, maximum_cells> m_cells;
	unsigned m_count;
};

} // namespace gsm::mobility

#endif // MAME_NOKIA_GSM_MOBILITY_H
