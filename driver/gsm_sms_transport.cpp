// license:BSD-3-Clause
// copyright-holders:Gaz

#include "gsm_sms_transport.h"

namespace gsm::sms
{

deliver_message parse_deliver(
		const std::uint8_t *information,
		unsigned length)
{
	if (!information || length < 16 ||
			information[0] != 0x09 || information[1] != 0x01 ||
			information[2] != length - 3 ||
			information[3] != 0x01 ||
			information[13] != length - 14 ||
			(information[14] & 0x03) != 0x00)
		return {};

	const unsigned address_digits = information[15];
	if (address_digits > 20)
		return {};
	const unsigned address_octets = (address_digits + 1) / 2;
	const unsigned pid_index = 17 + address_octets;
	const unsigned dcs_index = pid_index + 1;
	const unsigned udl_index = dcs_index + 1 + 7;
	if (udl_index >= length)
		return {};

	const std::uint8_t dcs = information[dcs_index];
	alphabet data_alphabet;
	if ((dcs & 0xc0) == 0x00)
	{
		if ((dcs & 0x20) != 0 || ((dcs >> 2) & 0x03) == 0x03)
			return {};
		switch ((dcs >> 2) & 0x03)
		{
		case 0:
			data_alphabet = alphabet::gsm_7bit;
			break;
		case 1:
			data_alphabet = alphabet::eight_bit;
			break;
		case 2:
			data_alphabet = alphabet::ucs2;
			break;
		default:
			return {};
		}
	}
	else if ((dcs & 0xf0) == 0xf0)
		data_alphabet =
				(dcs & 0x04) != 0 ? alphabet::eight_bit : alphabet::gsm_7bit;
	else
		return {};

	const unsigned user_data_length = information[udl_index];
	const unsigned required_octets =
			data_alphabet == alphabet::gsm_7bit ?
			(user_data_length * 7 + 7) / 8 : user_data_length;
	if (udl_index + 1 + required_octets != length)
		return {};

	return {
		true,
		information[0],
		information[4],
		data_alphabet,
		udl_index + 1,
		user_data_length
	};
}

uplink_message parse_uplink(
		const std::uint8_t *information,
		unsigned length,
		std::uint8_t downlink_transaction,
		std::uint8_t rp_reference)
{
	if (!information || length < 2 ||
			information[0] != (downlink_transaction ^ 0x80) ||
			(information[0] & 0x0f) != 0x09)
		return {};

	const std::uint8_t message_type = information[1] & 0x3f;
	if (message_type == 0x04 && length == 2)
		return { uplink_kind::cp_ack, 0 };

	if (message_type != 0x01 || length < 5 ||
			information[2] != length - 3 ||
			information[4] != rp_reference)
		return {};

	// RP-ACK has only its type and message reference.  RP-ERROR additionally
	// carries a cause and is deliberately terminal-but-unsuccessful to callers.
	if (information[3] == 0x02 && length == 5)
		return { uplink_kind::rp_ack, information[4] };
	if (information[3] == 0x04 && length >= 6)
		return { uplink_kind::rp_error, information[4] };
	return {};
}

} // namespace gsm::sms
