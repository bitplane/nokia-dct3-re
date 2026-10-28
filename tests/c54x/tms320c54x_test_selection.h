// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_SELECTION_H
#define MAME_NOKIA_TMS320C54X_TEST_SELECTION_H

#include <initializer_list>
#include <string_view>

namespace c54x_test {

template <typename Owner>
struct profile_start
{
	std::initializer_list<std::string_view> names;
	void (*start)(Owner &);
};

// The table and alias lists are consumed within this call, never retained.
template <typename Owner>
bool start_profile(Owner &owner, std::string_view selected,
		std::initializer_list<profile_start<Owner>> profiles)
{
	for (auto const &profile : profiles)
		for (auto const name : profile.names)
			if (selected == name)
			{
				profile.start(owner);
				return true;
			}
	return false;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_SELECTION_H
