// license:BSD-3-Clause
// copyright-holders:Gaz
#include "../driver/nokia_radio_timing.h"
#include <cassert>

int main()
{
	using namespace nokia::radio;
	static_assert(access_delay(2288, 0, 0, 102) == 102);
	static_assert(access_delay(2391, 1, 2446, 2) == 57);
	static_assert(access_delay(2'715'647, 1, std::uint16_t(2'715'647 + 55), 2) == 57);
	static_assert(access_delay(0xfffe, 1, 2, 3) == 7);
	static_assert(access_delay(0x10000, 1, 0, 0) == 0);
	static_assert(!access_delay(1000, 1, 999, 2));
	static_assert(!access_delay(0, 1, 0x8000, 0));
	static_assert(!access_delay(0, 2, 0, 0));
	static_assert(!frame_due(99, 100));
	static_assert(frame_due(100, 100));
	static_assert(frame_due(101, 100));
	static_assert(!frame_due(0xffffffffU, 1));
	static_assert(frame_due(1, 1));
	static_assert(frame_due(0, 0xffffffffU));
	for (unsigned delay = 0; delay < 256; ++delay)
	{
		const auto deadline = 0xfffffff0U + delay;
		for (unsigned elapsed = 0; elapsed < 256; ++elapsed)
			assert(frame_due(0xfffffff0U + elapsed, deadline) == (elapsed >= delay));
	}
}
