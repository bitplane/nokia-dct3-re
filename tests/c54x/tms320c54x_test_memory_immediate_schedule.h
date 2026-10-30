// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_MEMORY_IMMEDIATE_SCHEDULE_H
#define MAME_NOKIA_TMS320C54X_TEST_MEMORY_IMMEDIATE_SCHEDULE_H

namespace c54x_test {


inline constexpr unsigned mapped_mac_inputs = 262144;

// Thirty-two value/literal pairs per fixed source/form/flags/selectors/page
// group cover all three checkpoints (10/11/11), not three copies of each pair.
constexpr unsigned mapped_mac_key(unsigned ordinal)
{
	unsigned v = ordinal;
	unsigned const address = v % 16; v /= 16;
	unsigned const form = v % 4; v /= 4;
	unsigned const pattern = v % 4; v /= 4;
	unsigned const literal = v % 8; v /= 8;
	unsigned const flags = v % 16; v /= 16;
	unsigned const selectors = v % 4; v /= 4;
	unsigned const page = v % 2;
	return ordinal + mapped_mac_inputs * ((address + form + pattern + literal + flags + selectors + page) % 3);
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_MEMORY_IMMEDIATE_SCHEDULE_H
