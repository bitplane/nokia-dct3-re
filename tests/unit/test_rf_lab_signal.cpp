// license:BSD-3-Clause
// copyright-holders:Gaz
#include "nokia_rf_lab_signal.h"
#include <cassert>
#include <iostream>

int main()
{
	unsigned bursts=0;
	for (unsigned frame=0;frame<51;++frame)
	{
		bool const expected=frame==0 || frame==10 || frame==20 || frame==30 || frame==40;
		unsigned samples=0;
		for (unsigned tick=0;tick<60'000;tick+=24)
		{
			auto const absolute=std::uint64_t(frame)*60'000+tick;
			bool const active=nokia_rf_lab::fcch_active(absolute);
			assert(active==nokia_rf_lab::fcch_active(absolute+51*60'000));
			samples+=active;
		}
		assert(samples==(expected ? 148*2 : 0));
		bursts+=samples!=0;
		assert(nokia_rf_lab::fcch_active(std::uint64_t(frame)*60'000+7103)==expected);
		assert(!nokia_rf_lab::fcch_active(std::uint64_t(frame)*60'000+7104));
	}
	assert(bursts==5);
	std::cout<<"fcch_bursts=5 symbols=148 periodic=1 edges=1\n";
}
