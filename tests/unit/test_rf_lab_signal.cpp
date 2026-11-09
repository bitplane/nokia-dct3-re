// license:BSD-3-Clause
// copyright-holders:Gaz
#include "nokia_rf_lab_signal.h"
#include "nokia_rf_lab_bcch.h"
#include "gsm_mobility.h"
#include <cassert>
#include <iostream>
#include <string>

int main(int argc, char **argv)
{
	if (argc==4 && std::string(argv[1])=="pch")
	{
		auto const block=nokia_rf_lab::frame_paging(std::stoul(argv[2]));
		auto const bursts=nokia_rf_lab::bcch_bits(block,2);
		for (auto bit:bursts.at(std::stoul(argv[3]))) std::cout<<bit;
		std::cout<<'\n';
		return 0;
	}
	if (argc == 3 && (std::string(argv[1]) == "bcch" || std::string(argv[1]) == "agch"))
	{
		gsm::tch_f::packed_control_block si3{0x49,0x06,0x1b,0x00,0x01,0x00,0xf1,0x10,
			0x00,0x01,0x40,0,0,0,0,0,0,0,0,0x2b,0x2b,0x2b,0x2b};
		if (std::string(argv[1]) == "agch")
			si3={0x2d,6,0x3f,0,0x20,0,35,11,9,0xae,0,0,
				0x2b,0x2b,0x2b,0x2b,0x2b,0x2b,0x2b,0x2b,0x2b,0x2b,0x2b};
		auto const bursts=nokia_rf_lab::bcch_bits(si3,2);
		auto const &bits=bursts.at(std::stoul(argv[2]));
		for (auto bit:bits) std::cout<<bit;
		std::cout<<'\n';
		for (auto const &sample:nokia_rf_lab::gmsk_samples(bits)) std::cout<<sample[0]<<' '<<sample[1]<<'\n';
		return 0;
	}
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
	for (unsigned frame=0;frame<nokia_rf_lab::hyperframe;++frame)
	{
		if (frame%51!=6 && frame%51!=12 && frame%51!=16) continue;
		auto const page=nokia_rf_lab::frame_paging(frame);
		std::uint32_t const identity=(std::uint32_t(page[6])<<24)|
			(std::uint32_t(page[7])<<16)|(std::uint32_t(page[8])<<8)|page[9];
		assert(identity==(0x80000000U|frame));
		assert(page[0]==0x25 && page[4]==5 && page[5]==0xf4 && page[22]==0x2b);
	}
	for (unsigned frame:{0U,7U,11U,nokia_rf_lab::hyperframe})
	{
		bool rejected=false;
		try { nokia_rf_lab::frame_paging(frame); }
		catch (std::invalid_argument const &) { rejected=true; }
		assert(rejected);
	}
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
	std::array<unsigned,8> const si_by_tc{0,1,2,3,1,1,2,3};
	for (unsigned frame=0; frame<408; ++frame)
	{
		assert(gsm::mobility::bcch_system_information_index(frame)==si_by_tc[frame/51]);
		assert(gsm::mobility::bcch_system_information_index(frame+408)==si_by_tc[frame/51]);
	}
	for (unsigned frame=0; frame<51; ++frame)
	{
		bool const expected=frame>=2 && frame<=5;
		assert(nokia_rf_lab::bcch_active(std::uint64_t(frame)*60'000)==expected);
		assert(nokia_rf_lab::bcch_active(std::uint64_t(frame)*60'000+7103)==expected);
		assert(!nokia_rf_lab::bcch_active(std::uint64_t(frame)*60'000+7104));
		assert(nokia_rf_lab::bcch_active(std::uint64_t(frame+51)*60'000)==expected);
	}
	for (auto const parameters : {std::array<unsigned,2>{0,0},{1,64},{nokia_rf_lab::hyperframe,0}})
	{
		bool rejected=false;
		try { nokia_rf_lab::sch_samples(parameters[0],parameters[1]); }
		catch (std::invalid_argument const &) { rejected=true; }
		assert(rejected);
	}
	std::cout<<"fcch_bursts=5 symbols=148 periodic=1 edges=1\n";
}
