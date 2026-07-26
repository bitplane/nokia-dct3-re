// license:BSD-3-Clause
// copyright-holders:Gaz

#include "gsm_mm_authentication.h"

#include <algorithm>

namespace gsm::mm::authentication
{

std::array<std::uint8_t, 19> request(
		std::uint8_t key_sequence, const a3a8::block &rand)
{
	std::array<std::uint8_t, 19> message = {
		0x05, 0x12, std::uint8_t(key_sequence & 0x07)
	};
	std::copy(rand.begin(), rand.end(), message.begin() + 3);
	return message;
}

std::array<std::uint8_t, 2> reject()
{
	return { 0x05, 0x11 };
}

bool response_valid(
		const a3a8::result &expected,
		const std::uint8_t *information, unsigned length)
{
	return length == 6 && (information[0] & 0x0f) == 0x05 &&
			(information[1] & 0x3f) == 0x14 &&
			std::equal(expected.sres.begin(), expected.sres.end(),
					information + 2);
}

} // namespace gsm::mm::authentication
