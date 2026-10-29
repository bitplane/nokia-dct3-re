// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_MEMORY_IMMEDIATE_SCHEDULE_H
#define MAME_NOKIA_TMS320C54X_TEST_MEMORY_IMMEDIATE_SCHEDULE_H

namespace c54x_test {

inline constexpr unsigned memory_immediate_mpy_inputs = 24576;
inline constexpr unsigned memory_immediate_mac_inputs = 65536;
inline constexpr unsigned memory_immediate_inputs = memory_immediate_mpy_inputs + memory_immediate_mac_inputs;

// Preserve original operand identities and decoder numbering; discard only
// checkpoint copies. Every structural group has 64 value/literal pairs, with
// sixteen at each of the four checkpoints. This schedules inputs, not results.
constexpr unsigned memory_immediate_key(unsigned ordinal)
{
	bool const mac = ordinal >= memory_immediate_mpy_inputs;
	unsigned const input = mac ? ordinal - memory_immediate_mpy_inputs : ordinal;
	unsigned const forms = mac ? 8 : 12, flags = mac ? 16 : 8, selectors = mac ? 4 : 2;
	unsigned v = input;
	unsigned const form = v % forms; v /= forms;
	unsigned const value = v % 8; v /= 8;
	unsigned const literal = v % 8; v /= 8;
	unsigned const status = v % flags; v /= flags;
	unsigned const selector = v % selectors; v /= selectors;
	unsigned const page = v % 2;
	unsigned const checkpoint = (form + value + 2 * literal + status + selector + page) % 4;
	return (mac ? 98304 : 0) + input +
		(mac ? memory_immediate_mac_inputs : memory_immediate_mpy_inputs) * checkpoint;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_MEMORY_IMMEDIATE_SCHEDULE_H
