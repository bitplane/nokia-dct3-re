// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_GSM_CELL_BROADCAST_H
#define MAME_NOKIA_GSM_CELL_BROADCAST_H

#include <array>
#include <cstdint>

namespace gsm::cell_broadcast
{

constexpr unsigned page_octets = 88;
constexpr unsigned content_octets = 82;
constexpr unsigned block_information_octets = 22;
constexpr unsigned block_octets = 23;
constexpr unsigned blocks_per_page = 4;
using page = std::array<std::uint8_t, page_octets>;
using block = std::array<std::uint8_t, block_octets>;
using blocks = std::array<block, blocks_per_page>;

struct decoded_page
{
	bool valid = false;
	std::uint16_t serial_number = 0;
	std::uint16_t message_identifier = 0;
	std::uint8_t data_coding_scheme = 0;
	std::uint8_t page_index = 0;
	std::uint8_t page_count = 0;
	std::array<std::uint8_t, content_octets> content{};
};

page make_page(std::uint16_t serial_number, std::uint16_t message_identifier,
		std::uint8_t data_coding_scheme, std::uint8_t page_index,
		std::uint8_t page_count,
		const std::array<std::uint8_t, content_octets> &content);
decoded_page decode_page(const page &encoded);

// GSM 04.12 section 3: an 88-octet CBS page is carried as four consecutive
// 22-octet information blocks.  Each gains a one-octet LPD/LB/sequence header.
blocks segment_page(const page &encoded);
bool reassemble_page(const blocks &encoded, page &result);

} // namespace gsm::cell_broadcast

#endif // MAME_NOKIA_GSM_CELL_BROADCAST_H
