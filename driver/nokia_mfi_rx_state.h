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

	void reset() { *this = {}; }
	void enable(bool state)
	{
		if (enabled == state) return;
		head = count = 0;
		enabled = state;
		fault = false;
		produced = consumed = overruns = underruns = 0;
	}
	bool clock(std::uint16_t word, std::uint64_t source_tick, unsigned limit = capacity)
	{
		if (!enabled || fault) return false;
		if (!limit || limit > storage_words || count >= limit) { ++overruns; fault = true; return false; }
		unsigned const tail = (head + count) % storage_words;
		words[tail] = word;
		source_ticks[tail] = source_tick;
		++count;
		return (++produced % block_words) == 0;
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
		return last;
	}
};
#endif // MAME_NOKIA_NOKIA_MFI_RX_STATE_H
