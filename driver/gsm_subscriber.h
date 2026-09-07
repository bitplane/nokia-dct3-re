// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_GSM_SUBSCRIBER_H
#define MAME_NOKIA_GSM_SUBSCRIBER_H

#include "gsm_a3a8.h"
#include "gsm_mobility.h"

#include <array>
#include <cstdint>

namespace gsm::subscriber {

enum class authentication_algorithm : std::uint8_t
{
	none,
	aes_example
};

// Immutable subscription and home-network facts shared by the removable SIM
// and the laboratory network. Runtime files such as EF_LOCI and EF_Kc remain
// card-owned; serving ARFCN, BSIC and RF conditions remain cell-owned.
struct profile
{
	std::array<std::uint8_t, 10> iccid{};
	std::array<std::uint8_t, 9> imsi{};
	std::array<std::uint8_t, 3> preferred_plmn{};
	std::array<std::uint8_t, 17> service_provider_name{};
	std::array<std::uint8_t, 2> access_control_class{};
	mobility::location_area_identity home_location{};
	authentication_algorithm authentication = authentication_algorithm::none;
	a3a8::block ki{};

	constexpr bool valid() const
	{
		return imsi[0] == 8 &&
				preferred_plmn[0] == home_location.plmn[0] &&
				preferred_plmn[1] == home_location.plmn[1] &&
				preferred_plmn[2] == home_location.plmn[2];
	}
};

inline constexpr profile laboratory = {
	{ 0x98, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0 },
	// IMSI 001010123456789, encoded as GSM 11.11 EF_IMSI.
	{ 0x08, 0x09, 0x10, 0x10, 0x10, 0x32, 0x54, 0x76, 0x98 },
	{ 0x00, 0xf1, 0x10 },
	{ 0x00, 'D', 'C', 'T', '3', ' ', 'L', 'A', 'B',
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
	{ 0x00, 0x01 },
	{ { 0x00, 0xf1, 0x10 }, 1 },
	authentication_algorithm::aes_example,
	{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f }
};

static_assert(laboratory.valid());

} // namespace gsm::subscriber

#endif // MAME_NOKIA_GSM_SUBSCRIBER_H
