// license:BSD-3-Clause
// copyright-holders:Gaz

#include "gsm_sms_transport.h"

namespace gsm::sms
{

namespace
{

bool decode_bcd_address(const std::uint8_t *encoded, unsigned octets,
		unsigned digits, std::array<std::uint8_t, 20> &decoded)
{
	if (digits > decoded.size() || octets != (digits + 1) / 2)
		return false;
	decoded.fill(0);
	for (unsigned index = 0; index < digits; ++index)
	{
		const std::uint8_t digit = index & 1 ?
				(encoded[index / 2] >> 4) : (encoded[index / 2] & 0x0f);
		if (digit > 9)
			return false;
		decoded[index] = digit;
	}
	if ((digits & 1) && (encoded[octets - 1] >> 4) != 0x0f)
		return false;
	return true;
}

bool decode_dcs(std::uint8_t dcs, alphabet &data_alphabet)
{
	if ((dcs & 0xc0) == 0x00)
	{
		if ((dcs & 0x20) != 0 || ((dcs >> 2) & 0x03) == 0x03)
			return false;
		data_alphabet = alphabet((dcs >> 2) & 0x03);
		return true;
	}
	if ((dcs & 0xf0) == 0xf0)
	{
		data_alphabet = (dcs & 0x04) != 0 ?
				alphabet::eight_bit : alphabet::gsm_7bit;
		return true;
	}
	return false;
}

} // anonymous namespace

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

	alphabet data_alphabet;
	if (!decode_dcs(information[dcs_index], data_alphabet))
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

submit_message parse_submit(const std::uint8_t *information, unsigned length)
{
	if (!information || length < 15 ||
			(information[0] & 0x0f) != 0x09 || information[1] != 0x01 ||
			information[2] != length - 3 || information[3] != 0x00)
		return {};

	unsigned offset = 5;
	const unsigned originator_length = information[offset++];
	if (offset + originator_length >= length)
		return {};
	offset += originator_length;
	const unsigned service_center_length = information[offset++];
	if (service_center_length < 2 || offset + service_center_length >= length)
		return {};
	const unsigned service_center_octets = service_center_length - 1;
	const unsigned service_center_digits = service_center_octets * 2 -
			((information[offset + service_center_length - 1] >> 4) == 0x0f);
	submit_message result;
	if (!decode_bcd_address(information + offset + 1, service_center_octets,
			service_center_digits, result.service_center_digits))
		return {};
	result.service_center_digit_count = service_center_digits;
	offset += service_center_length;

	const unsigned tpdu_length = information[offset++];
	if (tpdu_length < 7 || offset + tpdu_length != length)
		return {};
	const unsigned tpdu = offset;
	const std::uint8_t first_octet = information[offset++];
	if ((first_octet & 0x03) != 0x01)
		return {};
	++offset; // TP-MR
	const unsigned destination_digits = information[offset++];
	if (destination_digits > result.destination_digits.size() || offset >= length)
		return {};
	++offset; // TP-DA type-of-address
	const unsigned destination_octets = (destination_digits + 1) / 2;
	if (offset + destination_octets + 3 > length ||
			!decode_bcd_address(information + offset, destination_octets,
				destination_digits, result.destination_digits))
		return {};
	result.destination_digit_count = destination_digits;
	offset += destination_octets;
	++offset; // TP-PID
	alphabet data_alphabet;
	if (!decode_dcs(information[offset++], data_alphabet))
		return {};
	const unsigned validity_format = (first_octet >> 3) & 0x03;
	if (validity_format == 0x02)
		++offset;
	else if (validity_format == 0x01 || validity_format == 0x03)
		offset += 7;
	if (offset >= length)
		return {};
	const unsigned user_data_length = information[offset++];
	const unsigned required_octets = data_alphabet == alphabet::gsm_7bit ?
			(user_data_length * 7 + 7) / 8 : user_data_length;
	if (offset + required_octets != tpdu + tpdu_length)
		return {};

	result.valid = true;
	result.cp_transaction = information[0];
	result.rp_reference = information[4];
	result.data_alphabet = data_alphabet;
	result.user_data_offset = offset;
	result.user_data_length = user_data_length;
	return result;
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
