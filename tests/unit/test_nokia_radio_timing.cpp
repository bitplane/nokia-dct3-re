// license:BSD-3-Clause
// copyright-holders:Gaz
#include "../../driver/nokia_radio_timing.h"
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
	static_assert(common_control_block_end(19, true) == 19);

	static_assert(subscriber_paging_slot(789,true,0,2)->multiframe_phase==1);
	static_assert(subscriber_paging_slot(789,true,0,2)->frame_offset==6);
	static_assert(subscriber_paging_slot(789,false,0,2)->multiframe_phase==1);
	static_assert(subscriber_paging_slot(789,false,0,2)->frame_offset==36);
	static_assert(!subscriber_paging_slot(1000,true,0,2));
	static_assert(!subscriber_paging_slot(1,true,3,2));
	static_assert(!subscriber_paging_slot(1,false,8,2));
	static_assert(!subscriber_paging_slot(1,true,0,1));
	static_assert(!subscriber_paging_slot(1,true,0,10));
	for (bool combined : {false,true})
		for (unsigned reserved=0;reserved<(combined ? 3U : 8U);++reserved)
			for (unsigned multiframes=2;multiframes<=9;++multiframes)
				for (unsigned imsi=0;imsi<1000;++imsi)
				{
					auto const slot=subscriber_paging_slot(imsi,combined,reserved,multiframes);
					assert(slot && slot->multiframe_phase<multiframes);
					unsigned index=0;
					for (unsigned start : {6,12,16,22,26,32,36,42,46})
					{
						if (start==slot->frame_offset) break;
						++index;
					}
					unsigned const available=(combined ? 3 : 9)-reserved;
					assert(index>=reserved && index<reserved+available);
					assert(slot->multiframe_phase*available+index-reserved==imsi%(available*multiframes));
				}
	for (unsigned frame=0;frame<204;++frame)
	{
		auto burst=combined_ccch_burst(frame);
		unsigned const position=frame%51;
		bool const active=(position>=6 && position<=9) || (position>=12 && position<=19);
		assert(bool(burst)==active);
		if (burst) assert(*burst==position-(position<10 ? 6 : position<16 ? 12 : 16));
	}
	static_assert(common_control_block_end(20, true) == 60);
	static_assert(common_control_block_end(50, true) == 60);
	static_assert(common_control_block_end(2'715'648, true) == 2'715'648 + 9);
	for (unsigned minimum = 0; minimum < 204; ++minimum)
	{
		const auto ending = common_control_block_end(minimum, true);
		assert(ending >= minimum && ending - minimum <= 40);
		assert(ending % 51 == 9 || ending % 51 == 15 || ending % 51 == 19);
		for (auto earlier = minimum; earlier < ending; ++earlier)
			assert(earlier % 51 != 9 && earlier % 51 != 15 && earlier % 51 != 19);
	}
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
