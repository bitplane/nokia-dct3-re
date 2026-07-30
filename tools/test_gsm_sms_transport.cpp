// license:BSD-3-Clause
// copyright-holders:Gaz

#include "../driver/gsm_sms_transport.h"

#include <array>
#include <cassert>
#include <iostream>

int main()
{
	using gsm::sms::parse_uplink;
	using gsm::sms::uplink_kind;

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
