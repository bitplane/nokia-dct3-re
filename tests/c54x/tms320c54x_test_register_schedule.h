// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_REGISTER_SCHEDULE_H
#define MAME_NOKIA_TMS320C54X_TEST_REGISTER_SCHEDULE_H

namespace c54x_test {

struct register_bank_schedule
{
	static constexpr unsigned stimuli = 65536, legacy_standalone = stimuli * 3;
	static constexpr unsigned arithmetic_producers = 288, load_producers = 144;
	static constexpr unsigned arithmetic_cases = stimuli + arithmetic_producers;
	static constexpr unsigned cases = arithmetic_cases + stimuli + load_producers;
	static constexpr unsigned load_first = legacy_standalone + arithmetic_producers;
	static constexpr unsigned end = load_first + legacy_standalone + load_producers;
	static constexpr bool load(unsigned variant) { return variant >= load_first; }
	static constexpr unsigned model_variant(unsigned variant) { return load(variant) ? variant - load_first : variant; }
	static constexpr unsigned next(unsigned variant)
	{
		if (variant + 1 == stimuli) return legacy_standalone;
		if (variant + 1 == load_first + stimuli) return load_first + legacy_standalone;
		return variant + 1;
	}
	static constexpr unsigned checkpoint_index(unsigned variant)
	{
		unsigned const v = model_variant(variant);
		if (v >= legacy_standalone)
			return (v - legacy_standalone) / (load(variant) ? 48 : 96);
		unsigned const stride = load(variant) ? 128 : 256;
		unsigned const pattern = (v / stride) % 8;
		unsigned const group = v % stride + stride * (v / (stride * 8));
		// Every fixed non-pattern group retains all eight inputs. Rotate their
		// Decode/Access/Read checkpoints; each occurs two or three times.
		return (pattern + group) % 3;
	}
	static constexpr unsigned case_key(unsigned variant)
	{
		unsigned const v = model_variant(variant);
		unsigned const in_bank = v < legacy_standalone ? v : stimuli + v - legacy_standalone;
		unsigned const ordinal = (load(variant) ? arithmetic_cases : 0) + in_bank;
		return ordinal * 3 + checkpoint_index(variant);
	}
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_REGISTER_SCHEDULE_H
