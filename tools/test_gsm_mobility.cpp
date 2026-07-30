// license:BSD-3-Clause
// copyright-holders:Gaz

#include "../driver/gsm_mobility.h"

#include <cassert>
#include <iostream>

int main()
{
	constexpr gsm::mobility::periodic_update_timer disabled;
	static_assert(!disabled.enabled());
	static_assert(disabled.encoded() == 0);
	static_assert(disabled.minutes() == 0);

	constexpr gsm::mobility::periodic_update_timer shortest(1);
	static_assert(shortest.enabled());
	static_assert(shortest.encoded() == 1);
	static_assert(shortest.minutes() == 6);

	constexpr gsm::mobility::periodic_update_timer longer(10);
	static_assert(longer.minutes() == 60);
	static_assert(longer != shortest);

	std::cout << "GSM mobility timer tests passed\n";
}
