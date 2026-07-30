// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_GSM_SMS_TRANSPORT_H
#define MAME_NOKIA_GSM_SMS_TRANSPORT_H

#include <cstdint>

namespace gsm::sms
{

enum class uplink_kind : std::uint8_t
{
	none,
	cp_ack,
	rp_ack,
	rp_error
};

struct uplink_message
{
	uplink_kind kind = uplink_kind::none;
	std::uint8_t rp_reference = 0;
};

// Parse the handset side of one GSM 04.11 CP transaction.  The expected
// transaction and RP reference come from the CP-DATA actually sent, rather
// than from a fixture or handset product.
uplink_message parse_uplink(
		const std::uint8_t *information,
		unsigned length,
		std::uint8_t downlink_transaction,
		std::uint8_t rp_reference);

} // namespace gsm::sms

#endif // MAME_NOKIA_GSM_SMS_TRANSPORT_H
