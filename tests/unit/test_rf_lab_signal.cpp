// license:BSD-3-Clause
// copyright-holders:Gaz
#include "nokia_rf_lab_signal.h"
#include <cassert>
#include <iostream>
#include <string>

int main(int argc, char **argv)
{
	if (argc == 4 && std::string(argv[1]) == "sch")
	{
		auto const samples = nokia_rf_lab::sch_samples(std::stoul(argv[2]), std::stoul(argv[3]));
		for (auto const &sample : samples) std::cout << sample[0] << ' ' << sample[1] << '\n';
		return 0;
	}
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
	unsigned sch_bursts=0;
	for (unsigned frame=0;frame<51;++frame)
	{
		bool const expected=frame==1 || frame==11 || frame==21 || frame==31 || frame==41;
		unsigned samples=0;
		for (unsigned tick=0;tick<60'000;tick+=12) samples+=nokia_rf_lab::sch_active(std::uint64_t(frame)*60'000+tick);
		assert(samples==(expected ? 148*4 : 0));
		sch_bursts+=samples!=0;
		assert(!nokia_rf_lab::sch_active(std::uint64_t(frame)*60'000+7104));
	}
	assert(sch_bursts==5);
	for (auto const parameters : {std::array<unsigned,2>{0,0},{1,64},{nokia_rf_lab::hyperframe,0}})
	{
		bool rejected=false;
		try { nokia_rf_lab::sch_samples(parameters[0],parameters[1]); }
		catch (std::invalid_argument const &) { rejected=true; }
		assert(rejected);
	}
	std::cout<<"fcch_bursts=5 symbols=148 periodic=1 edges=1\n";
}
