// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 sizes[]{1,2,3,13,16,31,32,250};
	for (unsigned mode = 0; mode < 4; ++mode)
	for (unsigned reg = 0; reg < 8; ++reg)
	for (unsigned compatibility = 0; compatibility < 2; ++compatibility)
	for (unsigned size_index = 0; size_index < 8; ++size_index)
	for (unsigned position = 0; position < 4; ++position)
	for (unsigned stride = 0; stride < 4; ++stride)
	{
		unsigned const selected = compatibility && !reg ? 4 : reg;
		unsigned const size = sizes[size_index];
		// SPRU131G 5.5.3 requires alignment strictly greater than BK.
		unsigned const alignment = std::array<unsigned,8>{2,4,4,16,32,32,64,256}[size_index];
		unsigned const buffer = selected == 0 ? 0 : position >= 2 ? 0xff00 : 0x1200;
		unsigned const index = std::array<unsigned,4>{0,size - 1,size / 2,1U % size}[position];
		std::array<u16,8> pointers;
		for (unsigned ar = 0; ar < 8; ++ar) pointers[ar] = 0x4000 + ar;
		pointers[0] = std::array<u16,4>{0,1,u16(size - 1),u16(1 - size)}[stride];
		pointers[selected] = buffer + index;
		int const step = mode == 0 ? -1 : mode == 2 ? 1 : (mode == 1 ? -int(s16(pointers[0])) : int(s16(pointers[0])));
		if (buffer % alignment || std::abs(step) > size) throw emu_fatalerror("Invalid circular MAR test domain");
		int const residue = (int(index) + step) % int(size);
		u16 const result = buffer + (residue < 0 ? residue + size : residue);
		u16 const st0 = compatibility && reg ? 0x608 | (reg << 13) : 0x8608;
		u16 const st1 = 0x800 | (compatibility ? 0x20 : 0);
		unsigned const seed = mode + reg + compatibility + size_index + position + stride;
		u32 const page = seed & 1 ? 0x20000 : 0;
		auto initial = initial_case("circular-mar", {u16(0x6dc0 | (mode << 3) | reg),0xf4e1});
		for (auto &word : initial.program) word.address += page + 0x8a00;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,page + 0x9000},{r::STATE_ST0,0x8608},{r::STATE_ST1,st1},
			{r::STATE_BK,size},{r::STATE_A,0xff12345678ULL},{r::STATE_B,0x81abcdef01ULL},{r::STATE_T,0x5aa5}});
		for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({r::STATE_AR0 + ar,pointers[ar]});
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,3>{3,4,6}[seed % 3],16};
		for (unsigned tick = 1; tick <= 16; ++tick) {
			auto state = observation("circular-access-publication", {{r::STATE_ST0,tick >= 4 ? unsigned(st0) : 0x8608U},
				{r::STATE_BK,size},{r::STATE_ST1,st1},{r::STATE_T,0x5aa5},{r::STATE_A,0xff12345678ULL},{r::STATE_B,0x81abcdef01ULL}});
			for (unsigned ar = 0; ar < 8; ++ar) state.expected.push_back({int(r::STATE_AR0 + ar),
				tick >= 4 && ar == selected ? unsigned(result) : unsigned(pointers[ar])});
			c.states.push_back({tick,std::move(state)});
		}
		c.data_events = std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
