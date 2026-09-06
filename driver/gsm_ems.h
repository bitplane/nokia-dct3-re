// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_GSM_EMS_H
#define MAME_NOKIA_GSM_EMS_H

#include <array>
#include <cstdint>

namespace gsm::ems
{

constexpr unsigned maximum_user_data_octets = 140;

struct user_data
{
	std::array<std::uint8_t, maximum_user_data_octets> data{};
	unsigned length = 0;
};

struct text_formatting
{
	bool valid = false;
	std::uint8_t start = 0;
	std::uint8_t length = 0;
	std::uint8_t mode = 0;
};

// TS 23.040 9.2.3.24.10: a TP-UDH Text Formatting IE followed by UCS-2 text.
user_data formatted_ucs2(const char *ascii, std::uint8_t mode);
text_formatting parse_text_formatting(
		const std::uint8_t *data, unsigned length);

} // namespace gsm::ems

#endif // MAME_NOKIA_GSM_EMS_H
