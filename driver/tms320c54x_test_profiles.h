// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_PROFILES_H
#define MAME_NOKIA_TMS320C54X_TEST_PROFILES_H

#include <array>
#include <cstdint>
#include <string_view>

namespace c54x_test {

struct source_alias_profile
{
	std::string_view bios;
	std::array<std::uint16_t, 12> families;
	unsigned family_count;
	bool read_modify_write = false;

	// Seven aliases, two destinations (or TC values), CMPT, pages, frontends
	// and save checkpoints. Family selection must not leak into those flags.
	static constexpr unsigned cases_per_family = 7 * 2 * 2 * 2 * 2 * 2;
	constexpr unsigned cases() const { return family_count * cases_per_family; }
	constexpr std::uint16_t opcode(unsigned variant) const
	{
		unsigned const flags = variant / 7;
		return families[variant / cases_per_family] |
				(!read_modify_write && (flags & 1) ? 0x100 : 0) | (0xf9 + variant % 7);
	}
	constexpr std::uint16_t st0(unsigned variant) const
	{
		return read_modify_write && ((variant / 7) & 1) ? 0x0804 : 0x1804;
	}
};

inline constexpr source_alias_profile source_alias_profiles[] = {
	{ "mpy-abs-edge", { 0x6200 }, 1 },
	{ "mac-abs-edge", { 0x6400, 0x6600 }, 2 },
	{ "delay-abs-edge", { 0x4c00 }, 1 },
	{ "pmac-abs-edge", { 0x7800, 0x7a00 }, 2 },
	{ "mem-t-abs-edge", { 0x2000, 0x2200, 0x2400, 0x2600, 0x2800, 0x2a00, 0x2c00, 0x2e00 }, 8 },
	{ "load-abs-edge", { 0x1000, 0x1200, 0x1400, 0x1600, 0x4400, 0x5600 }, 6 },
	{ "rmw-abs-edge", { 0x6800, 0x6900, 0x6a00, 0x6b00 }, 4, true },
	{ "logic-abs-edge", { 0x1800, 0x1a00, 0x1c00 }, 3 },
	{ "arith-abs-edge", { 0x0000, 0x0200, 0x0400, 0x0600, 0x0800, 0x0a00, 0x0c00, 0x1e00, 0x3c00, 0x3e00, 0x4000, 0x4200 }, 12 },
	{ "long-abs-edge", { 0x5000, 0x5200, 0x5400, 0x5800, 0x5a00, 0x5c00, 0x5e00 }, 7 },
};

inline constexpr source_alias_profile const *find_source_alias_profile(std::string_view bios)
{
	for (auto const &profile : source_alias_profiles)
		if (profile.bios == bios)
			return &profile;
	return nullptr;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_PROFILES_H
