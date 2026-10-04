// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_NOKIA_RECORD_CODEC_H
#define MAME_NOKIA_NOKIA_RECORD_CODEC_H

#include <array>
#include <cstdint>

namespace nokia_dct3_record_codec {
using record = std::array<std::uint8_t, 12>;

inline void rotate(record &data, unsigned start, unsigned count)
{
	std::uint32_t value = 0;
	for (unsigned i = 0; i < 4; ++i)
		value = (value << 8) | data[start + i];
	value = (value >> count) | (value << (32 - count));
	for (unsigned i = 0; i < 4; ++i)
		data[start + i] = value >> (24 - 8 * i);
}

inline record inverse(record data, record const &table, record const &schedule)
{
	// GF(2) inverse of the native-observed ROM4 linear helper. These rows
	// are independently regenerated/checked by the Python word-model tests.
	static constexpr std::array<std::uint16_t, 12> rows = {
		0x4ed, 0x89f, 0x3b5, 0x27e, 0xed4, 0x9f8,
		0xb53, 0x7e2, 0xd4e, 0xf89, 0x53b, 0xe27
	};
	record reversed{};
	for (unsigned i = 0; i < 12; ++i)
		for (unsigned bit = 0; bit < 8; ++bit)
			reversed[i] |= ((data[11 - i] >> bit) & 1) << (7 - bit);
	data = reversed;
	std::array<unsigned, 8> nonlinear_inverse{};
	for (unsigned value = 0; value < 8; ++value)
	{
		unsigned output = 0;
		for (unsigned i = 0; i < 3; ++i)
			output |= (((value >> i) & 1) ^ (((value >> ((i + 1) % 3)) & 1) |
					(((value >> ((i + 2) % 3)) & 1) ^ 1))) << i;
		nonlinear_inverse[output] = value;
	}
	for (int round = 11; round >= 0; --round)
	{
		if (round < 11)
		{
			rotate(data, 0, 1);
			rotate(data, 8, 22);
			record decoded{};
			for (unsigned group = 0; group < 4; ++group)
				for (unsigned bit = 0; bit < 8; ++bit)
				{
					unsigned value = 0;
					for (unsigned i = 0; i < 3; ++i)
						value |= ((data[group + 4 * i] >> bit) & 1) << i;
					value = nonlinear_inverse[value];
					for (unsigned i = 0; i < 3; ++i)
						decoded[group + 4 * i] |= ((value >> i) & 1) << bit;
				}
			data = decoded;
			rotate(data, 0, 22);
			rotate(data, 8, 1);
		}
		record mixed{};
		for (unsigned i = 0; i < 12; ++i)
			for (unsigned j = 0; j < 12; ++j)
				if (rows[i] & (1U << j))
					mixed[i] ^= data[j];
		for (unsigned i : {2U, 3U, 8U, 9U})
			mixed[i] ^= schedule[round];
		for (unsigned i = 0; i < 12; ++i)
			data[i] = mixed[i] ^ table[i];
	}
	return data;
}
} // namespace nokia_dct3_record_codec
#endif // MAME_NOKIA_NOKIA_RECORD_CODEC_H
