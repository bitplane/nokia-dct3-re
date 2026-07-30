// license:BSD-3-Clause
// copyright-holders:Gaz

#include "gsm_sms_transport.h"

namespace gsm::sms
{

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
