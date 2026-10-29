// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_DUAL_SCHEDULE_H
#define MAME_NOKIA_TMS320C54X_TEST_DUAL_SCHEDULE_H

namespace c54x_test {

template<unsigned NumericInputs, unsigned Blocks>
struct dual_operand_schedule
{
	static constexpr unsigned address_cases = 18432;
	static constexpr unsigned numeric_inputs = NumericInputs;
	static constexpr unsigned per_family = address_cases + numeric_inputs;
	static constexpr unsigned cases = Blocks * per_family;

	// Address banks also vary flags and accumulator seeds: retain them all.
	// Numerical banks differ only by snapshot. Every fixed selector/flags/
	// seed/page group keeps all 64 operand pairs, split 21/21/22 across them.
	static constexpr unsigned key(unsigned ordinal)
	{
		unsigned const family = ordinal / per_family, input = ordinal % per_family;
		unsigned const old_span = address_cases + 3 * numeric_inputs;
		if (input < address_cases) return family * old_span + input;
		unsigned const raw = input - address_cases, pair = raw % 64, group = raw / 64;
		return family * old_span + address_cases + raw + numeric_inputs * ((pair + group + family) % 3);
	}
};

using dual_mpy_schedule = dual_operand_schedule<8192, 1>;
using dual_alu_schedule = dual_operand_schedule<8192, 2>;
using dual_macsu_schedule = dual_operand_schedule<65536, 1>;
using dual_mac_schedule = dual_operand_schedule<65536, 8>;

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_DUAL_SCHEDULE_H
