// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_SMEM_SCHEDULE_H
#define MAME_NOKIA_TMS320C54X_TEST_SMEM_SCHEDULE_H

namespace c54x_test {

// Schedules original raw input identities, never instruction expectations.
// All banks have 18 address-form slots; slot 5 is a write-only source.
struct smem_schedule
{
	enum class kind { test_compare, signed_word, unsigned_add, unsigned_sub, shift16 };
	static constexpr unsigned stride(kind k)
	{
		return k == kind::test_compare ? 144 : k == kind::signed_word ? 576 : k == kind::shift16 ? 1152 : 288;
	}
	static constexpr unsigned raw_inputs(kind k) { return k == kind::test_compare ? 18432 : stride(k) * 64; }
	static constexpr unsigned cases(kind k) { return raw_inputs(k) / 18 * 17; }
	static constexpr bool valid(unsigned raw, kind k = kind::test_compare) { return raw < raw_inputs(k) && raw % 18 != 5; }
	static constexpr unsigned checkpoint(unsigned raw, kind k = kind::test_compare)
	{
		unsigned const width = stride(k), pattern = (raw / width) % 8;
		unsigned const group = raw % width + width * (raw / (width * 8));
		// Each fixed non-pattern group retains all eight value pairs, with
		// two or three at each of the original three ownership boundaries.
		return (pattern + group) % 3;
	}
	static constexpr unsigned key(unsigned raw, kind k = kind::test_compare) { return raw + raw_inputs(k) * checkpoint(raw, k); }
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_SMEM_SCHEDULE_H
