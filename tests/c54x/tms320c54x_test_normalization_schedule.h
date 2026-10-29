// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_NORMALIZATION_SCHEDULE_H
#define MAME_NOKIA_TMS320C54X_TEST_NORMALIZATION_SCHEDULE_H

#include <string_view>

namespace c54x_test {

enum class normalization_kind { none, documented, aliases, producer, exponent };

inline normalization_kind select_normalization(std::string_view name)
{
	return name == "norm" ? normalization_kind::documented : name == "norm-edge" ? normalization_kind::aliases :
		name == "exp-norm" ? normalization_kind::producer : name == "exp-t" ? normalization_kind::exponent : normalization_kind::none;
}

struct normalization_schedule
{
	static constexpr unsigned count(normalization_kind kind)
	{
		return kind == normalization_kind::documented ? 24576 : kind == normalization_kind::aliases ? 8192 :
			kind == normalization_kind::producer ? 1536 : 3072;
	}
	// Keep every numeric/context tuple once. Balance replay across value patterns,
	// rather than executing the same tuple at every checkpoint. EXP/NORM's sticky
	// overflow seed depends on pattern parity, so balance each parity separately.
	static constexpr unsigned key(normalization_kind kind, unsigned ordinal)
	{
		unsigned checkpoint = 0;
		if (kind == normalization_kind::documented)
			checkpoint = ((ordinal / 192) % 8 + ordinal % 4 + (ordinal / 4) % 48 + (ordinal / 1536) % 8 + ordinal / 12288) % 4;
		else if (kind == normalization_kind::aliases)
			checkpoint = ((ordinal / 64) % 8 + ordinal % 4 + (ordinal / 4) % 16 + (ordinal / 512) % 8 + ordinal / 4096) % 2;
		else if (kind == normalization_kind::producer)
			checkpoint = (((ordinal / 4) % 16) / 2 + ordinal % 4 + (ordinal / 64) % 3 + (ordinal / 192) % 4 + ordinal / 768) % 4;
		else
			checkpoint = (ordinal % 16 + (ordinal / 16) % 2 + (ordinal / 32) % 3 + (ordinal / 96) % 8 + (ordinal / 768) % 2 + ordinal / 1536) % 4;
		return ordinal + count(kind) * checkpoint;
	}
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_NORMALIZATION_SCHEDULE_H
