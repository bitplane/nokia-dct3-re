// license:BSD-3-Clause
// copyright-holders:Gaz
#include "../../driver/gsm_normal_p27.h"
#include <iomanip>
#include <iostream>

int main()
{
	unsigned tsc;
	while (std::cin>>std::dec>>tsc)
	{
		std::array<gsm::tch_f::burst_payload,4> bursts{};
		bool valid=true;
		for (auto &burst:bursts)
		{
			std::array<std::uint16_t,15> words{};
			for (auto &word:words) if (!(std::cin>>std::hex>>word)) return 1;
			auto decoded=gsm::normal::decode_p27(words,tsc);
			if (decoded) burst=*decoded;else valid=false;
		}
		auto block=valid ? gsm::normal::decode_control(bursts) : std::nullopt;
		if (!block) { std::cout<<"invalid\n";continue; }
		auto print=[](auto const &bytes) {
			for (auto byte:bytes) std::cout<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(byte);
		};
		print(*block);
		auto ua=gsm::normal::contention_ua(*block);
		if (!ua) ua=gsm::normal::disconnect_ua(*block);
		std::cout<<' ';if (ua) print(*ua);else std::cout<<"none";
		std::cout<<'\n';
	}
}
