// license:BSD-3-Clause
// copyright-holders:Gaz

#include "gsm_ems.h"

#include <cstring>

namespace gsm::ems
{

user_data formatted_ucs2(const char *ascii, std::uint8_t mode)
{
	user_data result;
	if (!ascii)
		return result;
	const unsigned characters = std::strlen(ascii);
	if (characters > 67)
		return result;

	// UDHL=5, IEI=0x0a, IEDL=3, start, formatted length, format mode.
	result.data[0] = 5;
	result.data[1] = 0x0a;
	result.data[2] = 3;
	result.data[3] = 0;
	result.data[4] = characters;
	result.data[5] = mode;
	result.length = 6;
	for (unsigned index = 0; index < characters; ++index)
	{
		result.data[result.length++] = 0;
		result.data[result.length++] = std::uint8_t(ascii[index]);
	}
	return result;
}

text_formatting parse_text_formatting(
		const std::uint8_t *data, unsigned length)
{
	if (!data || length < 6 || data[0] + 1 > length)
		return {};
	const unsigned end = data[0] + 1;
	for (unsigned offset = 1; offset + 2 <= end; )
	{
		const std::uint8_t identifier = data[offset++];
		const unsigned ie_length = data[offset++];
		if (offset + ie_length > end)
			return {};
		if (identifier == 0x0a && ie_length == 3)
			return { true, data[offset], data[offset + 1], data[offset + 2] };
		offset += ie_length;
	}
	return {};
}

} // namespace gsm::ems
