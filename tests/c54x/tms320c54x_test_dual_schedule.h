// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_DUAL_SCHEDULE_H
#define MAME_NOKIA_TMS320C54X_TEST_DUAL_SCHEDULE_H

namespace c54x_test {

struct dual_mac_schedule
{
	static constexpr unsigned address_cases = 18432;
	static constexpr unsigned numeric_inputs = 65536;
	static constexpr unsigned per_family = address_cases + numeric_inputs;
	static constexpr unsigned cases = 8 * per_family;

	// Address banks also vary flags and accumulator seeds: retain them all.
	// Numerical banks differ only by snapshot. Every fixed selector/flags/
	// seed/page group keeps all 64 operand pairs, split 21/21/22 across them.
	static constexpr unsigned key(unsigned ordinal)
	{
		unsigned const family = ordinal / per_family, input = ordinal % per_family;
		if (input < address_cases) return family * 215040 + input;
		unsigned const raw = input - address_cases, pair = raw % 64, group = raw / 64;
		return family * 215040 + address_cases + raw + numeric_inputs * ((pair + group + family) % 3);
	}
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_DUAL_SCHEDULE_H
