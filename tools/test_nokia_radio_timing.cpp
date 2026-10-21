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
	static_assert(common_control_block_end(2391 + 4) == 2395);
	static_assert(common_control_block_end(2396) == 2406);
	static_assert(common_control_block_end(2'715'647 + 4) == 2'715'648 + 9);
	for (unsigned minimum = 0; minimum < 102; ++minimum)
	{
		const auto ending = common_control_block_end(minimum);
		assert(ending >= minimum);
		assert(ending - minimum <= 10);
		assert(ending % 51 == 9 || ending % 51 == 15 || ending % 51 == 19 ||
				ending % 51 == 25 || ending % 51 == 29 || ending % 51 == 35 ||
				ending % 51 == 39 || ending % 51 == 45 || ending % 51 == 49);
	}
	for (unsigned delay = 0; delay < 256; ++delay)
	{
		const auto deadline = 0xfffffff0U + delay;
		for (unsigned elapsed = 0; elapsed < 256; ++elapsed)
			assert(frame_due(0xfffffff0U + elapsed, deadline) == (elapsed >= delay));
	}
}
