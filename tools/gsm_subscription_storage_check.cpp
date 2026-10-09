// license:BSD-3-Clause
// copyright-holders:Gaz
#include "gsm_subscription_storage.h"
#include <cassert>

int main()
{
	using namespace gsm::subscription_storage;
	identity subscriber{};
	subscriber[0] = 8;
	records original{};
	for (unsigned i = 0; i < original.size(); ++i)
	{
		original[i].registered = true;
		original[i].active = i & 1;
		original[i].length = 7;
		original[i].number = {'5', '5', '5', '1', '2', '3', std::uint8_t('0' + i)};
		original[i].service = 2;
		original[i].service_code = 0x11;
		original[i].no_reply_time = 5 + i;
	}
	const auto bytes = encode(subscriber, original);
	records restored{};
	assert(decode(bytes, subscriber, restored));
	assert(encode(subscriber, restored) == bytes);
	for (unsigned offset = 0; offset < bytes.size(); ++offset)
		for (unsigned bit = 0; bit < 8; ++bit)
		{
			auto changed = bytes;
			changed[offset] ^= 1U << bit;
			assert(!decode(changed, subscriber, restored));
			assert(encode(subscriber, restored) == bytes);
		}
	auto other = subscriber;
	other[11] = 1;
	assert(!decode(bytes, other, restored));
	assert(encode(subscriber, restored) == bytes);
	auto reject_with_valid_crc = [&](image changed)
	{
		const auto crc = checksum(changed);
		for (unsigned i = 0; i < 4; ++i)
			changed[changed.size() - 4 + i] = std::uint8_t(crc >> (i * 8));
		assert(!decode(changed, subscriber, restored));
		assert(encode(subscriber, restored) == bytes);
	};
	for (unsigned condition = 0; condition < condition_count; ++condition)
	{
		const unsigned base = 20 + condition * 21;
		for (unsigned field = 0; field < 3; ++field)
		{
			auto changed = bytes;
			changed[base + field] = 0xff;
			reject_with_valid_crc(changed);
		}
		for (unsigned flags : {0U, 2U})
		{
			auto changed = bytes;
			changed[base] = flags;
			if (flags == 2)
				changed[base + 1] = 0; // Isolate active-without-registration.
			reject_with_valid_crc(changed);
		}
		auto changed = bytes;
		changed[base + 1] = 0;
		reject_with_valid_crc(changed);
	}
	for (unsigned field = 0; field < 20; ++field)
	{
		auto changed = bytes;
		changed[field] ^= 1;
		reject_with_valid_crc(changed);
	}
	const auto empty = encode(subscriber, records{});
	assert(decode(empty, subscriber, restored));
	assert(encode(subscriber, restored) == empty);
}
