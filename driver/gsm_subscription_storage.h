// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_GSM_SUBSCRIPTION_STORAGE_H
#define MAME_NOKIA_GSM_SUBSCRIPTION_STORAGE_H

#include <array>
#include <cstdint>

namespace gsm::subscription_storage {

constexpr unsigned condition_count = 4;
constexpr unsigned number_capacity = 16;
using identity = std::array<std::uint8_t, 12>; // EF_IMSI encoding + home PLMN.
struct forwarding_record
{
	bool registered = false, active = false;
	std::uint8_t length = 0, service = 0, service_code = 0, no_reply_time = 0;
	std::array<std::uint8_t, number_capacity> number{};
};
using records = std::array<forwarding_record, condition_count>;
using image = std::array<std::uint8_t, 20 + condition_count * 21 + 4>;

inline std::uint32_t checksum(const image &bytes)
{
	std::uint32_t crc = 0xffffffff;
	for (unsigned i = 0; i < bytes.size() - 4; ++i)
	{
		crc ^= bytes[i];
		for (unsigned bit = 0; bit != 8; ++bit)
			crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320U : 0);
	}
	return ~crc;
}

inline image encode(const identity &subscriber, const records &state)
{
	image bytes{};
	bytes[0] = 'G'; bytes[1] = 'S'; bytes[2] = 'U'; bytes[3] = 'B';
	bytes[4] = 1; bytes[5] = condition_count; bytes[6] = number_capacity;
	for (unsigned i = 0; i < subscriber.size(); ++i)
		bytes[8 + i] = subscriber[i];
	unsigned offset = 20;
	for (const auto &record : state)
	{
		bytes[offset++] = unsigned(record.registered) | (unsigned(record.active) << 1);
		bytes[offset++] = record.length;
		bytes[offset++] = record.service;
		bytes[offset++] = record.service_code;
		bytes[offset++] = record.no_reply_time;
		for (auto digit : record.number)
			bytes[offset++] = digit;
	}
	const auto crc = checksum(bytes);
	for (unsigned i = 0; i != 4; ++i)
		bytes[offset + i] = std::uint8_t(crc >> (8 * i));
	return bytes;
}

inline bool decode(const image &bytes, const identity &subscriber, records &state)
{
	if (bytes[0] != 'G' || bytes[1] != 'S' || bytes[2] != 'U' || bytes[3] != 'B' ||
			bytes[4] != 1 || bytes[5] != condition_count || bytes[6] != number_capacity || bytes[7])
		return false;
	for (unsigned i = 0; i < subscriber.size(); ++i)
		if (bytes[8 + i] != subscriber[i])
			return false;
	std::uint32_t stored = 0;
	for (unsigned i = 0; i != 4; ++i)
		stored |= std::uint32_t(bytes[bytes.size() - 4 + i]) << (8 * i);
	if (stored != checksum(bytes))
		return false;
	records candidate{};
	unsigned offset = 20;
	for (auto &record : candidate)
	{
		const auto flags = bytes[offset++];
		record.registered = flags & 1;
		record.active = flags & 2;
		record.length = bytes[offset++];
		record.service = bytes[offset++];
		record.service_code = bytes[offset++];
		record.no_reply_time = bytes[offset++];
		if (flags > 3 || (record.active && !record.registered) ||
				record.length > number_capacity || record.service > 2 ||
				(record.registered && !record.length) || (!record.registered && record.length))
			return false;
		for (auto &digit : record.number)
			digit = bytes[offset++];
	}
	state = candidate; // Never publish a partially decoded subscription.
	return true;
}

} // namespace gsm::subscription_storage
#endif // MAME_NOKIA_GSM_SUBSCRIPTION_STORAGE_H
