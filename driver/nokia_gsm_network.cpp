// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#include "emu.h"
#include "nokia_gsm_network.h"

DEFINE_DEVICE_TYPE(NOKIA_GSM_NETWORK, nokia_gsm_network_device,
		"nokia_gsm_network", "Nokia DCT3 laboratory GSM network")

namespace {

// Minimum broadcast set for a GSM 900 cell on ARFCN 1. The reserved test PLMN
// 001-01 avoids coupling the synthetic network to handset network-lock data;
// LAC and cell ID are both 1.
constexpr std::array<std::array<u8, 24>, 4> SYSTEM_INFORMATION = {{
	// SI1 Cell Channel Description uses GSM bitmap-0 format. ARFCN 1 is
	// bit 0 of the final octet in that 16-octet field.
	{{ 0x55, 0x06, 0x19,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01,
		0, 0, 0, 0x2b, 0 }},
	{{ 0x59, 0x06, 0x1a, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0xff, 0, 0, 0, 0 }},
	{{ 0x49, 0x06, 0x1b, 0x00, 0x01, 0x00, 0xf1, 0x10,
		0x00, 0x01, 0x40, 0, 0, 0, 0, 0, 0, 0, 0,
		0x2b, 0x2b, 0x2b, 0x2b, 0x2b }},
	{{ 0x31, 0x06, 0x1c, 0x00, 0xf1, 0x10, 0x00, 0x01,
		0, 0, 0, 0, 0, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,
		0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b }}
}};

} // anonymous namespace

nokia_gsm_network_device::nokia_gsm_network_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_GSM_NETWORK, tag, owner, clock)
{
}

void nokia_gsm_network_device::device_start()
{
}

const std::array<u8, 24> &nokia_gsm_network_device::system_information(unsigned index) const
{
	return SYSTEM_INFORMATION[index % SYSTEM_INFORMATION.size()];
}

std::array<u8, 24> nokia_gsm_network_device::paging_fill() const
{
	// GSM 04.08 9.1.22: Paging Request Type 1 with a one-octet "no identity"
	// mobile identity keeps the subscriber's PCH group active when no service
	// is pending. The final byte is the Nokia decoded-block padding octet.
	std::array<u8, 24> block;
	block.fill(0x2b);
	block[0] = 0x15;
	block[1] = 0x06;
	block[2] = 0x21;
	block[3] = 0x00;
	block[4] = 0x01;
	block[5] = 0xf0;
	return block;
}

std::array<u8, 24> nokia_gsm_network_device::paging_request(
		const u8 *mobile_identity, unsigned length) const
{
	std::array<u8, 24> block;
	block.fill(0x2b);
	if (length != 8)
		return block;

	// GSM 04.08 9.1.22 and 10.5.2.5. Request an SDCCH and preserve the IMSI
	// mobile-identity contents received organically during registration.
	block[0] = 0x31;
	block[1] = 0x06;
	block[2] = 0x21;
	block[3] = 0x10;
	block[4] = length;
	std::copy_n(mobile_identity, length, block.begin() + 5);
	return block;
}

nokia_gsm_network_device::paging_group
nokia_gsm_network_device::subscriber_paging_group(
		const u8 *mobile_identity, unsigned length) const
{
	// The phase-2 IMSI identity has digit one in the high nibble of octet one,
	// followed by low/high semi-octets. TS 05.02 uses IMSI mod 1000 across nine
	// CCCH blocks and BS_PA_MFRMS=2, as advertised by this cell's SI3.
	if (length != 8 || (mobile_identity[0] & 0x07) != 0x01)
		return { 0, 6 };

	auto digit = [mobile_identity](unsigned index) -> u8
	{
		if (index == 0)
			return mobile_identity[0] >> 4;
		const unsigned packed = index - 1;
		const u8 octet = mobile_identity[1 + packed / 2];
		return (packed & 1) ? octet >> 4 : octet & 0x0f;
	};
	const unsigned last_three = digit(12) * 100 + digit(13) * 10 + digit(14);
	static constexpr std::array<u8, 9> CCCH_BLOCK_OFFSETS = {
		6, 12, 16, 22, 26, 32, 36, 42, 46
	};
	const unsigned group = last_three % (CCCH_BLOCK_OFFSETS.size() * 2);
	return {
		u8(group / CCCH_BLOCK_OFFSETS.size()),
		CCCH_BLOCK_OFFSETS[group % CCCH_BLOCK_OFFSETS.size()]
	};
}

std::array<u8, 24> nokia_gsm_network_device::immediate_assignment(
		u8 random_access, u32 frame_number) const
{
	// GSM 04.08 9.1.18 and 10.5.2.30. Assign SDCCH/8 subchannel 0,
	// timeslot 0 on the non-hopping serving carrier, and echo the exact random
	// access octet and reception frame which identify the phone's request.
	const u8 t1_prime = (frame_number / 1326) & 0x1f;
	const u8 t2 = frame_number % 26;
	const u8 t3 = frame_number % 51;
	std::array<u8, 24> block = {
		0x2d, 0x06, 0x3f, 0x00,
		0x20, 0x00, 0x01,
		random_access, u8((t1_prime << 3) | (t3 >> 3)), u8((t3 << 5) | t2),
		0x00, 0x00,
		0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,
		0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b
	};
	return block;
}

std::array<u8, 17> nokia_gsm_network_device::location_update_accept(
		const u8 *location_update_request, unsigned length) const
{
	// GSM 04.08 9.2.13. The mandatory body is the MM header and Location
	// Area Identification. With no allocated TMSI, the network includes the
	// IMSI mobile-identity IE so the phone discards any stale TMSI. This accepts
	// the subscriber in the laboratory PLMN and LAC advertised by SI3.
	std::array<u8, 17> message = {
		0x05, 0x02, 0x00, 0xf1, 0x10, 0x00, 0x01,
		0x17, 0x08, 0, 0, 0, 0, 0, 0, 0, 0
	};
	// GSM 04.08 Location Updating Request places the mobile-identity length
	// at octet 10. Preserve the received identity when it is the eight-octet
	// IMSI form used by the laboratory subscriber.
	if (length >= 18 && location_update_request[9] == 8)
		std::copy_n(location_update_request + 10, 8, message.begin() + 9);
	return message;
}

std::array<u8, 3> nokia_gsm_network_device::cipher_mode_command() const
{
	// GSM 04.08 9.1.9. This exercises the handset's cipher-control boundary
	// while explicitly selecting SC=0 (no ciphering). Subsequent laboratory
	// frames therefore remain clear; this is not an A5 implementation.
	return { 0x06, 0x35, 0x00 };
}

std::array<u8, 10> nokia_gsm_network_device::mm_information() const
{
	// GSM 04.08 9.2.15. Keep this deterministic: 2026-07-24 12:00:00 UTC.
	// The phone consumes this on the newly established MM connection before
	// entering call control. Octets use GSM's swapped semi-octet BCD form.
	return { 0x05, 0x32, 0x47, 0x62, 0x70, 0x42, 0x21, 0x00, 0x00, 0x00 };
}

std::array<u8, 17> nokia_gsm_network_device::incoming_call_setup() const
{
	// GSM 04.08 9.3.23. Transaction 0 is network-originated. The bearer
	// capability is speech, the SIGNAL IE requests ordinary ringing, and the
	// calling-party BCD digits are the deterministic fixture number 5551234.
	return {
		0x03, 0x05,
		0x04, 0x04, 0x60, 0x02, 0x00, 0x81,
		0x34, 0x01,
		0x5c, 0x05, 0x81, 0x55, 0x15, 0x32, 0xf4
	};
}

std::array<u8, 8> nokia_gsm_network_device::traffic_assignment() const
{
	// GSM 04.08 9.1.2 and 10.5.2.5. Move the call from its temporary SDCCH
	// onto TCH/F timeslot 1 on non-hopping ARFCN 1. TSC 2 is the BCC carried
	// by the laboratory cell's BSIC 0x12; power level 0 is the mandatory
	// initial Power Command. Channel Mode selects GSM full-rate speech v1.
	return { 0x06, 0x2e, 0x09, 0x40, 0x01, 0x00, 0x63, 0x01 };
}

std::array<u8, 36> nokia_gsm_network_device::incoming_sms_cp_data() const
{
	// GSM 04.11/03.40 mobile-terminated CP-DATA containing RP-DATA and one
	// SMS-DELIVER. The deterministic fixture is text "hello" from 5551234,
	// via service centre +1234567890, timestamped 2026-07-24 12:00:00 UTC.
	return {
		0x09, 0x01, 0x21,
		0x01, 0x40, 0x06, 0x91, 0x21, 0x43, 0x65, 0x87, 0x09,
		0x00, 0x16,
		0x04, 0x07, 0x81, 0x55, 0x15, 0x32, 0xf4,
		0x00, 0x00,
		0x62, 0x70, 0x42, 0x21, 0x00, 0x00, 0x00,
		0x05, 0xe8, 0x32, 0x9b, 0xfd, 0x06
	};
}

unsigned nokia_gsm_network_device::incoming_smart_message_part_count() const
{
	return (smart_message_ringtone_payload_length +
			smart_message_multipart_part_capacity - 1) /
			smart_message_multipart_part_capacity;
}

nokia_gsm_network_device::layer3_message
nokia_gsm_network_device::incoming_smart_message_cp_data(
		unsigned part_index) const
{
	// This is a complete Nokia RTPL tone rather than the former four-byte
	// routing marker. RTPL permits more than one command before its zero
	// terminator; composing the same known-valid command twice makes a legal
	// 251-byte long ringtone and exercises Nokia's multipart envelope.
	static constexpr std::array<u8, 125> RINGTONE_COMMAND = {
		0x02, 0x4a, 0x3a, 0x7d, 0x51, 0x95, 0xcd, 0xd0,
		0x81, 0x99, 0xbd, 0xc8, 0x81, 0x11, 0xa1, 0xa5,
		0xc9, 0x85, 0xb4, 0x04, 0x00, 0x9b, 0x28, 0xca,
		0xea, 0x22, 0x82, 0x28, 0x49, 0xa4, 0x1c, 0x41,
		0xa6, 0x1c, 0x41, 0x84, 0x10, 0x42, 0x88, 0xa0,
		0x8a, 0x12, 0x69, 0x07, 0x18, 0x69, 0x84, 0x18,
		0x61, 0x24, 0x10, 0x55, 0x05, 0x50, 0x61, 0x05,
		0x90, 0x55, 0x85, 0x90, 0xa2, 0x2c, 0x49, 0x16,
		0x13, 0x61, 0x54, 0x15, 0x61, 0x56, 0x10, 0x61,
		0x56, 0x18, 0x41, 0x02, 0x28, 0x8a, 0x08, 0xa1,
		0x26, 0x90, 0x71, 0x06, 0x98, 0x71, 0x06, 0x10,
		0x41, 0x0a, 0x22, 0x82, 0x2d, 0x49, 0x08, 0x1a,
		0x41, 0xc6, 0x1a, 0x41, 0x84, 0x90, 0x41, 0x54,
		0x15, 0x41, 0x84, 0x16, 0x41, 0x56, 0x16, 0x42,
		0x88, 0xb1, 0x24, 0x58, 0x4d, 0x85, 0x50, 0x55,
		0x84, 0xd0, 0x4d, 0x84, 0x50
	};
	static constexpr unsigned PAYLOAD_LENGTH =
			RINGTONE_COMMAND.size() * 2 + 1;
	static_assert(PAYLOAD_LENGTH == smart_message_ringtone_payload_length);
	const unsigned part_count = incoming_smart_message_part_count();
	layer3_message result;
	if (part_index >= part_count || part_count > smart_message_maximum_parts)
		return result;

	std::array<u8, PAYLOAD_LENGTH> ringtone{};
	std::copy(RINGTONE_COMMAND.begin(), RINGTONE_COMMAND.end(),
			ringtone.begin());
	std::copy(RINGTONE_COMMAND.begin(), RINGTONE_COMMAND.end(),
			ringtone.begin() + RINGTONE_COMMAND.size());
	ringtone.back() = 0x00;

	const unsigned payload_offset =
			part_index * smart_message_multipart_part_capacity;
	const unsigned payload_length = std::min<unsigned>(
			smart_message_multipart_part_capacity,
			ringtone.size() - payload_offset);
	const unsigned user_data_length = 12 + payload_length;
	const unsigned tpdu_length = 17 + user_data_length;
	const unsigned rpdu_length = 11 + tpdu_length;

	auto append = [&result](u8 value) {
		result.data[result.length++] = value;
	};
	append(0x09);
	append(0x01);
	append(rpdu_length);
	append(0x01);
	append(0x40 + part_index);
	append(0x06);
	append(0x91);
	append(0x21);
	append(0x43);
	append(0x65);
	append(0x87);
	append(0x09);
	append(0x00);
	append(tpdu_length);
	append(part_index + 1 < part_count ? 0x40 : 0x44);
	append(0x07);
	append(0x81);
	append(0x55);
	append(0x15);
	append(0x32);
	append(0xf4);
	append(0x00);
	append(0xf5);
	append(0x62);
	append(0x70);
	append(0x42);
	append(0x21);
	append(0x00);
	append(0x00);
	append(0x00);
	append(user_data_length);
	append(0x0b);
	append(0x05);
	append(0x04);
	append(0x15);
	append(0x81);
	append(0x00);
	append(0x00);
	append(0x00);
	append(0x03);
	append(0x7a);
	append(part_count);
	append(part_index + 1);
	std::copy_n(ringtone.begin() + payload_offset, payload_length,
			result.data.begin() + result.length);
	result.length += payload_length;
	return result;
}

std::array<u8, 2> nokia_gsm_network_device::sms_cp_ack(u8 transaction) const
{
	return { u8(transaction ^ 0x80), 0x04 };
}

std::array<u8, 2> nokia_gsm_network_device::connect_acknowledge(
		u8 transaction) const
{
	return { u8(transaction ^ 0x80), 0x0f };
}

std::array<u8, 2> nokia_gsm_network_device::call_release(u8 transaction) const
{
	return { u8(transaction ^ 0x80), 0x2d };
}

std::array<u8, 3> nokia_gsm_network_device::channel_release() const
{
	// GSM 04.08 9.1.7. Cause 0 is "normal event". Once Location Updating has
	// completed, the network releases the temporary SDCCH and the mobile returns
	// to the already-selected serving cell.
	return { 0x06, 0x0d, 0x00 };
}

s8 nokia_gsm_network_device::serving_rssi(unsigned sample) const
{
	// A real receiver never returns an identical RSSI forever. The ROM retains
	// the previous sample and requires a strict improvement before promoting a
	// background measurement to a usable candidate. Keep the laboratory cell
	// deterministic while representing that measured-signal variation.
	static constexpr std::array<s8, 4> RSSI_PATTERN = { -60, -61, -59, -60 };
	return RSSI_PATTERN[sample % RSSI_PATTERN.size()];
}
