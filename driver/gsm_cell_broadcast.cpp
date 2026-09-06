// license:BSD-3-Clause
// copyright-holders:Gaz

#include "gsm_cell_broadcast.h"

#include <algorithm>

namespace gsm::cell_broadcast
{

page make_page(std::uint16_t serial_number, std::uint16_t message_identifier,
		std::uint8_t data_coding_scheme, std::uint8_t page_index,
		std::uint8_t page_count,
		const std::array<std::uint8_t, content_octets> &content)
{
	page result{};
	if (!page_index || page_index > 15 || !page_count || page_count > 15 ||
			page_index > page_count)
		return result;
	result[0] = serial_number >> 8;
	result[1] = serial_number;
	result[2] = message_identifier >> 8;
	result[3] = message_identifier;
	result[4] = data_coding_scheme;
	result[5] = (page_index << 4) | page_count;
	std::copy(content.begin(), content.end(), result.begin() + 6);
	return result;
}

decoded_page decode_page(const page &encoded)
{
	decoded_page result;
	result.serial_number = (encoded[0] << 8) | encoded[1];
	result.message_identifier = (encoded[2] << 8) | encoded[3];
	result.data_coding_scheme = encoded[4];
	result.page_index = encoded[5] >> 4;
	result.page_count = encoded[5] & 0x0f;
	result.valid = result.page_index && result.page_count &&
			result.page_index <= result.page_count;
	std::copy(encoded.begin() + 6, encoded.end(), result.content.begin());
	return result;
}

blocks segment_page(const page &encoded)
{
	blocks result{};
	for (unsigned sequence = 0; sequence < result.size(); ++sequence)
	{
		// LPD=01 identifies GSM 04.12.  LB is set only on the fourth block.
		result[sequence][0] = 0x40 | (sequence == 3 ? 0x10 : 0) | sequence;
		std::copy_n(encoded.begin() + sequence * block_information_octets,
				block_information_octets, result[sequence].begin() + 1);
	}
	return result;
}

bool reassemble_page(const blocks &encoded, page &result)
{
	for (unsigned sequence = 0; sequence < encoded.size(); ++sequence)
	{
		const std::uint8_t type = encoded[sequence][0];
		if ((type & 0x60) != 0x40 || (type & 0x0f) != sequence ||
				bool(type & 0x10) != (sequence == 3))
			return false;
		std::copy_n(encoded[sequence].begin() + 1,
				block_information_octets,
				result.begin() + sequence * block_information_octets);
	}
	return true;
}

} // namespace gsm::cell_broadcast
