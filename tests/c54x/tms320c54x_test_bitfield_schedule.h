// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_BITFIELD_SCHEDULE_H
#define MAME_NOKIA_TMS320C54X_TEST_BITFIELD_SCHEDULE_H

namespace c54x_test {

struct bitfield_schedule
{
	static constexpr unsigned raw_inputs = 18432, cases = 17408;
	static constexpr bool valid(unsigned raw) { return raw < raw_inputs && raw % 18 != 5; }
	static constexpr unsigned checkpoint(unsigned raw)
	{
		unsigned const pattern = (raw / 144) % 8;
		unsigned const group = raw % 144 + 144 * (raw / 1152);
		// Each form/register/TC/flags/compatibility group retains all eight
		// value/mask pairs, with two or three at each ownership boundary.
		return (pattern + group) % 3;
	}
	static constexpr unsigned key(unsigned raw) { return raw + raw_inputs * checkpoint(raw); }
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_BITFIELD_SCHEDULE_H
