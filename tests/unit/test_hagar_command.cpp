// license:BSD-3-Clause
// copyright-holders:Gaz
#include "nokia_hagar_command_state.h"
#include <cassert>
#include <iostream>

static unsigned pll_word(int scalar)
{
	unsigned const unbiased = scalar - 38;
	return ((unbiased / 64) << 7) + (unbiased % 64) + 38;
}

int main()
{
	nokia_hagar_command_state state;
	std::uint16_t key = 999;
	std::uint64_t frequency = 0;
	assert(!state.rx_candidate(key, frequency) && key == 999);
	unsigned cases = 0;
	for (unsigned channel = 0; channel <= 1023; ++channel)
	{
		bool const gsm = channel <= 124 || channel >= 975;
		bool const dcs = channel >= 512 && channel <= 885;
		if (!gsm && !dcs) continue;
		int const signed_key = channel >= 975 ? int(channel) - 1024 : int(channel);
		int const scalar = gsm ? 2 * (signed_key + 4675) : int(channel) + 8514;
		state.reset();
		state.write(0x100, 0x44000000 | pll_word(scalar));
		assert(!state.rx_candidate(key, frequency));
		state.write(0x100, gsm ? 0x41001054 : 0x4100082a);
		assert(!state.rx_candidate(key, frequency));
		state.write(0x100, gsm ? 0x4a800402 : 0x4a800208);
		assert(state.rx_candidate(key, frequency) && key == channel);
		std::uint64_t const expected = gsm ? 935'000'000LL + std::int64_t(signed_key) * 200'000
			: 1'805'200'000ULL + std::uint64_t(channel - 512) * 200'000;
		assert(frequency == expected);
		auto restored = state;
		state.write(0x100, 0x4100ffff);
		assert(!state.rx_candidate(key, frequency));
		assert(restored.rx_candidate(key, frequency) && key == channel);
		++cases;
	}
	assert(cases == 548);
	// Same PLL word, two real keys; neither is selected without both controls.
	state.reset(); state.write(0x100, 0x440048c8);
	state.write(0x100, 0x41001054); state.write(0x100, 0x4a800402);
	assert(state.rx_candidate(key, frequency) && key == 1);
	state.write(0x100, 0x4100082a); assert(!state.rx_candidate(key, frequency));
	state.write(0x100, 0x4a800208);
	assert(state.rx_candidate(key, frequency) && key == 838);
	state.write(0x100, 0x44009044); assert(!state.rx_candidate(key, frequency));
	state.write(0x100, 0x440048c8); state.write(0x100, 0x4a802018);
	assert(!state.rx_candidate(key, frequency));
	state.write(0x100, 0x4a800208); state.write(0, 0x440048c8);
	assert(!state.rx_candidate(key, frequency));
	state.write(0x100, 0x440048c8); state.write(0x100, 0x4100082a);
	state.write(0x100, 0x4a800208); state.write(0x100, 0x600048c8);
	assert(!state.rx_candidate(key, frequency));
	state.reset(); assert(!state.seen && !state.last_word);
	std::cout << "HAGAR observed command state: PASS keys=548 unmatched=1 disagreement=1 unknown=1 snapshot=1 reset=1 rf_readiness_claim=0\n";
}
