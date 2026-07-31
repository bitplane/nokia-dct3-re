// license:BSD-3-Clause
// copyright-holders:Gaz

#include "../driver/gsm_sms_transport.h"

#include <array>
#include <cassert>
#include <iostream>

int main()
{
	using gsm::sms::parse_uplink;
	using gsm::sms::parse_deliver;
	using gsm::sms::alphabet;
	using gsm::sms::uplink_kind;

	constexpr std::array<std::uint8_t, 36> deliver = {
		0x09, 0x01, 0x21,
		0x01, 0x40, 0x06, 0x91, 0x21, 0x43, 0x65, 0x87, 0x09,
		0x00, 0x16,
		0x04, 0x07, 0x81, 0x55, 0x15, 0x32, 0xf4,
		0x00, 0x00,
		0x62, 0x70, 0x42, 0x21, 0x00, 0x00, 0x00,
		0x05, 0xe8, 0x32, 0x9b, 0xfd, 0x06
	};
	const auto parsed_deliver = parse_deliver(deliver.data(), deliver.size());
	assert(parsed_deliver.valid);
	assert(parsed_deliver.cp_transaction == 0x09);
	assert(parsed_deliver.rp_reference == 0x40);
	assert(parsed_deliver.data_alphabet == alphabet::gsm_7bit);
	assert(parsed_deliver.user_data_offset == 31);
	assert(parsed_deliver.user_data_length == 5);

	auto malformed_deliver = deliver;
	malformed_deliver[15] = 0x20;
	assert(!parse_deliver(
			malformed_deliver.data(), malformed_deliver.size()).valid);
	malformed_deliver = deliver;
	malformed_deliver[22] = 0x80;
	assert(!parse_deliver(
			malformed_deliver.data(), malformed_deliver.size()).valid);
	malformed_deliver = deliver;
	malformed_deliver[30] = 0x20;
	assert(!parse_deliver(
			malformed_deliver.data(), malformed_deliver.size()).valid);
	assert(!parse_deliver(deliver.data(), deliver.size() - 2).valid);
	assert(!parse_deliver(deliver.data(), 15).valid);

	constexpr std::array<std::uint8_t, 2> cp_ack = { 0x89, 0x04 };
	assert(parse_uplink(cp_ack.data(), cp_ack.size(), 0x09, 0x40).kind ==
			uplink_kind::cp_ack);

	constexpr std::array<std::uint8_t, 5> rp_ack = {
		0x89, 0x01, 0x02, 0x02, 0x40
	};
	const auto parsed = parse_uplink(
			rp_ack.data(), rp_ack.size(), 0x09, 0x40);
	assert(parsed.kind == uplink_kind::rp_ack);
	assert(parsed.rp_reference == 0x40);

	constexpr std::array<std::uint8_t, 6> rp_error = {
		0x89, 0x01, 0x03, 0x04, 0x40, 0x6f
	};
	assert(parse_uplink(
			rp_error.data(), rp_error.size(), 0x09, 0x40).kind ==
			uplink_kind::rp_error);

	auto malformed = rp_ack;
	malformed[2] = 3;
	assert(parse_uplink(
			malformed.data(), malformed.size(), 0x09, 0x40).kind ==
			uplink_kind::none);

	auto wrong_cp_transaction = rp_ack;
	wrong_cp_transaction[0] = 0x99;
	assert(parse_uplink(
			wrong_cp_transaction.data(), wrong_cp_transaction.size(),
			0x09, 0x40).kind == uplink_kind::none);

	assert(parse_uplink(
			rp_ack.data(), rp_ack.size(), 0x09, 0x41).kind ==
			uplink_kind::none);
	assert(parse_uplink(
			rp_ack.data(), rp_ack.size() - 1, 0x09, 0x40).kind ==
			uplink_kind::none);

	auto trailing_cp_ack = std::array<std::uint8_t, 3>{ 0x89, 0x04, 0x00 };
	assert(parse_uplink(
			trailing_cp_ack.data(), trailing_cp_ack.size(), 0x09, 0x40).kind ==
			uplink_kind::none);

	std::cout << "GSM SMS transport tests passed\n";
}
