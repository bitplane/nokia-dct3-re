// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_NOKIA_MFI_RX_STATE_H
#define MAME_NOKIA_NOKIA_MFI_RX_STATE_H
#include <array>
#include <cstdint>

// Laboratory staging FIFO, not a recovered physical MFI RAM organization.
// The larger backing supports one block-rounded service-gap experiment.
struct nokia_mfi_rx_state
{
	static constexpr unsigned block_words = 32, capacity = 64, storage_words = 96;
	std::array<std::uint16_t, storage_words> words{};
	std::array<std::uint64_t, storage_words> source_ticks{};
	std::uint16_t head = 0, count = 0, last = 0;
	std::uint64_t produced = 0, consumed = 0, overruns = 0, underruns = 0;
	bool enabled = false, fault = false;
	// Optional laboratory request latch: one notification owns a32-word
	// read. Re-announce a complete following block after that read, rather
	// than losing it when CPU interrupt edges coalesce. Not a silicon ACK map.
	bool block_request_active = false, block_request_rearmed = false;

	void reset() { *this = {}; }
	void enable(bool state)
	{
		if (enabled == state) return;
		head = count = 0;
		enabled = state;
		fault = false;
		produced = consumed = overruns = underruns = 0;
		block_request_active = block_request_rearmed = false;
	}
	bool clock(std::uint16_t word, std::uint64_t source_tick, unsigned limit = capacity, bool latch_request = false)
	{
		if (!enabled || fault) return false;
		if (!limit || limit > storage_words || count >= limit) { ++overruns; fault = true; return false; }
		unsigned const tail = (head + count) % storage_words;
		words[tail] = word;
		source_ticks[tail] = source_tick;
		++count;
		if ((++produced % block_words) != 0) return false;
		if (latch_request)
		{
			if (block_request_active) return false;
			block_request_active = true;
		}
		return true;
	}
	std::uint16_t read(bool side_effects = true)
	{
		if (!side_effects) return count ? words[head] : last;
		if (!enabled) return last; // Inactive register retention, no FIFO pop.
		if (!count) { ++underruns; return last; }
		last = words[head];
		head = (head + 1) % storage_words;
		--count;
		++consumed;
		if (block_request_active && consumed % block_words == 0)
		{
			block_request_active = count >= block_words;
			block_request_rearmed = block_request_active;
		}
		return last;
	}
	bool take_rearmed_request()
	{
		bool const result = block_request_rearmed;
		block_request_rearmed = false;
		return result;
	}
};
#endif // MAME_NOKIA_NOKIA_MFI_RX_STATE_H
