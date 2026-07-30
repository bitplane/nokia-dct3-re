// license:BSD-3-Clause
// copyright-holders:Gaz

#include "../driver/gsm_mobility.h"

#include <cassert>
#include <iostream>

int main()
{
	static_assert(gsm::mobility::synchronization_frame(0) == 2'715'638);
	static_assert(gsm::mobility::synchronization_frame(1) == 1);
	static_assert(gsm::mobility::synchronization_frame(10) == 1);
	static_assert(gsm::mobility::synchronization_frame(11) == 11);
	static_assert(gsm::mobility::synchronization_frame(50) == 41);
	static_assert(gsm::mobility::synchronization_frame(52) == 52);
	constexpr auto sch =
			gsm::mobility::synchronization_channel_information(0x22, 11);
	static_assert(sch[0] == 0x88);
	static_assert(sch[1] == 0x00);
	static_assert(sch[2] == 0x2c);
	static_assert(sch[3] == 0x80);

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

	gsm::mobility::downlink_signalling_counter downlink;
	assert(downlink.ceiling() == 45);
	for (unsigned block = 0; block < 11; ++block)
		assert(!downlink.observe(false));
	assert(downlink.value() == 1);
	assert(downlink.observe(false));
	assert(downlink.value() == 0);
	downlink.restore(20);
	assert(!downlink.observe(true));
	assert(downlink.value() == 21);
	downlink.restore(100);
	assert(downlink.value() == downlink.ceiling());
	static_assert(
			gsm::mobility::downlink_signalling_counter::initial_value(9) == 10);

	constexpr gsm::mobility::cell serving{};
	static_assert(serving.arfcn == 1);
	static_assert(serving.bsic == 0x12);
	static_assert(serving.location.lac == 1);

	gsm::mobility::topology cells;
	assert(cells.size() == 1);
	assert(cells.find(1) != nullptr);
	assert(cells.find(1)->rxlev_dbm == -60);
	assert(cells.find(823) == nullptr);

	gsm::mobility::cell neighbour = serving;
	neighbour.arfcn = 2;
	neighbour.bsic = 0x22;
	neighbour.identity = 2;
	neighbour.rxlev_dbm = -55;
	assert(cells.set(1, neighbour));
	assert(cells.size() == 2);
	assert(cells.find(2) != nullptr);
	assert(cells.find(1)->same_location_area(*cells.find(2)));

	neighbour.location.lac = 2;
	assert(cells.set(1, neighbour));
	assert(!cells.find(1)->same_location_area(*cells.find(2)));
	assert(!cells.set(gsm::mobility::topology::maximum_cells, neighbour));

	cells.use_single_cell();
	assert(cells.size() == 1);
	assert(cells.find(2) == nullptr);

	std::cout << "GSM mobility tests passed\n";
}
