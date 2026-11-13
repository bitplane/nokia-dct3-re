// license:BSD-3-Clause
// copyright-holders:Gaz
#include "nokia_rf_lab_tch.h"
#include <cassert>
#include <iostream>

namespace {
using nokia_rf_lab::traffic_channel;

traffic_channel::block octets(std::initializer_list<std::uint8_t> head)
{
	traffic_channel::block result{};
	result.fill(0x2b);
	std::copy(head.begin(),head.end(),result.begin());
	return result;
}

gsm::tch_f::burst_payload payload(traffic_channel::bits const &bits)
{
	gsm::tch_f::normal_burst burst{};
	std::copy(bits.begin(),bits.end(),burst.bits.begin());
	return gsm::tch_f::unpack_normal_burst(burst);
}
}

int main()
{
	auto const si5=octets({0,0,3,3,0x49,6,0x1d}),si6=octets({0,0,3,3,0x2d,6,0x1e});
	traffic_channel bts,ms;
	// Frame 4718 is position 12 (TS1 idle); the first block starts at B0 of 13.
	bts.start(4718,1,2,{si5,si6});
	ms.start(4718,1,2,{si5,si6});
	assert(bts.first_frame()==4719 && 4719%26==13);
	assert(!bts.downlink(4718));
	gsm::tch_f::packed_speech_frame speech{};
	speech[0]=0xd0;speech[1]=0x12;speech[20]=0x5a;
	bts.set_speech_source([&] { return speech; });
	bts.send_facch(octets({1,0x73,1}));
	unsigned facch=0,sacch=0,good_speech=0,idle=0;
	std::uint64_t facch_frame=0;
	for (std::uint64_t frame=4719;frame<4719+312;++frame)
	{
		auto const bits=bts.downlink(frame);
		// A repeated request in the same frame returns the same burst.
		assert(bits==bts.downlink(frame));
		if (!bits) { assert(frame%26==12);++idle;continue; }
		// Every other frame is skipped by the handset; the receiver must still step.
		if (frame%26==3) continue;
		for (auto const &event:ms.uplink(frame,payload(*bits)))
		{
			switch (event.kind)
			{
			case traffic_channel::uplink_kind::facch:
				assert(event.octets==octets({1,0x73,1}));
				facch_frame=event.frame;++facch;
				break;
			case traffic_channel::uplink_kind::sacch:
				// SACCH/TF on TS1 alternates SI5 and SI6 per 104-frame block.
				assert(event.frame%104==25);
				assert(event.octets==((event.frame/104)&1 ? si6 : si5));
				++sacch;
				break;
			case traffic_channel::uplink_kind::speech:
				assert(event.speech==speech);
				++good_speech;
				break;
			default:
				break;
			}
		}
	}
	assert(idle==12 && facch==1 && facch_frame==4719 && sacch==2);
	// Skipped bursts corrupt some speech blocks; most still decode exactly.
	assert(good_speech>=50);
	std::cout<<"tch_lab facch="<<facch<<" sacch="<<sacch<<" speech="<<good_speech<<'\n';
	// Traffic uplink keeps cleared stealing flags; SDCCH decode rejects them.
	gsm::tch_f::burst_payload clear{};
	clear.data[0]=1;
	auto const packed=gsm::tch_f::pack_normal_burst(clear,gsm::tch_f::training_sequence(2));
	std::array<std::uint16_t,15> words{};
	for (unsigned bit=0;bit<148;++bit) words[(bit+5)/12]|=packed.bits[bit]<<((bit+5)%12);
	auto const traffic=gsm::normal::decode_p27_traffic(words,2);
	assert(traffic && !traffic->hl && !traffic->hu && traffic->data==clear.data);
	assert(!gsm::normal::decode_p27(words,2));
	return 0;
}
