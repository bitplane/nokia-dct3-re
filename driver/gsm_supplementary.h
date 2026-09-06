// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_GSM_SUPPLEMENTARY_H
#define MAME_NOKIA_GSM_SUPPLEMENTARY_H

#include <array>
#include <cstdint>

namespace gsm::ss
{

constexpr unsigned maximum_message_length = 64;
constexpr unsigned maximum_ussd_length = 40;

enum class operation : std::uint8_t
{
	unknown = 0,
	register_ss = 0x0a,
	erase_ss = 0x0b,
	activate_ss = 0x0c,
	deactivate_ss = 0x0d,
	interrogate_ss = 0x0e,
	process_uss_request = 0x3b
};

struct request
{
	bool valid = false;
	std::uint8_t transaction = 0;
	std::uint8_t invoke_id = 0;
	operation operation_code = operation::unknown;
	std::uint8_t service_code = 0;
	std::uint8_t data_coding_scheme = 0;
	unsigned ussd_length = 0;
	std::array<std::uint8_t, maximum_ussd_length> ussd{};
};

struct message
{
	unsigned length = 0;
	std::array<std::uint8_t, maximum_message_length> data{};
};

request parse_register(const std::uint8_t *data, unsigned length);
message interrogate_result(const request &request, bool active);
message process_uss_request_result(const request &request,
		const char *response);

} // namespace gsm::ss

#endif // MAME_NOKIA_GSM_SUPPLEMENTARY_H
