// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_GSM_SMS_TRANSPORT_H
#define MAME_NOKIA_GSM_SMS_TRANSPORT_H

#include <cstdint>
#include <array>

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

enum class alphabet : std::uint8_t
{
	gsm_7bit,
	eight_bit,
	ucs2
};

struct deliver_message
{
	bool valid = false;
	std::uint8_t cp_transaction = 0;
	std::uint8_t rp_reference = 0;
	alphabet data_alphabet = alphabet::gsm_7bit;
	unsigned user_data_offset = 0;
	unsigned user_data_length = 0;
};

struct submit_message
{
	bool valid = false;
	std::uint8_t cp_transaction = 0;
	std::uint8_t rp_reference = 0;
	std::array<std::uint8_t, 20> service_center_digits{};
	unsigned service_center_digit_count = 0;
	std::array<std::uint8_t, 20> destination_digits{};
	unsigned destination_digit_count = 0;
	alphabet data_alphabet = alphabet::gsm_7bit;
	unsigned user_data_offset = 0;
	unsigned user_data_length = 0;
};

// Validate a complete GSM 04.11 CP-DATA/RP-DATA carrying one GSM 03.40
// SMS-DELIVER. All TPDU offsets are derived from the originating-address
// length; malformed fixture data cannot shift later fields into plausible
// positions.
deliver_message parse_deliver(
		const std::uint8_t *information,
		unsigned length);

// Validate a complete mobile-originated CP-DATA/RP-DATA carrying one
// SMS-SUBMIT. RP and TPDU address lengths are followed rather than assuming
// the Nokia fixture's service-centre or destination sizes.
submit_message parse_submit(
		const std::uint8_t *information,
		unsigned length);

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
