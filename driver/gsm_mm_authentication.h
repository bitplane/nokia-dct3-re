// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_GSM_MM_AUTHENTICATION_H
#define MAME_NOKIA_GSM_MM_AUTHENTICATION_H

#include "gsm_a3a8.h"

#include <array>
#include <cstdint>

namespace gsm::mm::authentication
{

std::array<std::uint8_t, 19> request(
		std::uint8_t key_sequence, const a3a8::block &rand);
std::array<std::uint8_t, 2> reject();
bool response_valid(
		const a3a8::result &expected,
		const std::uint8_t *information, unsigned length);

} // namespace gsm::mm::authentication

#endif // MAME_NOKIA_GSM_MM_AUTHENTICATION_H
