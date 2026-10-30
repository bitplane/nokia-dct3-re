// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned reverse = 0; reverse < 2; ++reverse) for (unsigned ar = 0; ar < 8; ++ar)
	for (unsigned add = 0; add < 2; ++add) for (unsigned compatibility = 0; compatibility < 2; ++compatibility)
	for (unsigned index = 0; index < 8; ++index) for (unsigned high = 0; high < 2; ++high) {
		unsigned const selected = compatibility && !ar ? 4 : ar;
		unsigned const seed = reverse + ar + add + compatibility + index + high;
		u32 const base = seed & 1 ? 0x20000 : 0;
		u16 const step = reverse ? std::array<u16,8>{1,2,4,8,0x80,0x100,0x4000,0x8000}[index] :
			std::array<u16,8>{0,1,0xffff,3,0x8000,0x7fff,0x1234,0xfffe}[index];
		std::array<u16,8> pointers;
		for (unsigned i = 0; i < 8; ++i) pointers[i] = high ? 0xfffe - i : 0x1000 + i;
		pointers[0] = step;
		u16 result = add ? pointers[selected] + step : pointers[selected] - step;
		if (reverse) {
			// Carry/borrow propagates from bit 15 toward bit 0, independent of reverse/add/reverse.
			result = 0; int carry = 0;
			for (int bit = 15; bit >= 0; --bit) {
				int const sum = int((pointers[selected] >> bit) & 1) +
					(add ? int((step >> bit) & 1) : -int((step >> bit) & 1)) + carry;
				result |= (sum & 1) << bit;
				carry = add ? int(sum >= 2) : sum < 0 ? -1 : 0;
			}
		}
		u16 const opcode = (reverse ? (add ? 0x6db8 : 0x6da0) : (add ? 0x6db0 : 0x6da8)) | ar;
		u16 const st1 = 0x800 | (compatibility ? 0x20 : 0);
		u16 const st0 = compatibility && ar ? 0x608 | (ar << 13) : 0x8608;
		auto initial = initial_case("indexed-MAR", {opcode,0xf4e1});
		for (auto &entry : initial.program) entry.address += base;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0x8608},{r::STATE_ST1,st1},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},{r::STATE_T,0x5aa5}});
		for (unsigned i = 0; i < 8; ++i) initial.initial.push_back({int(r::STATE_AR0) + int(i),pointers[i]});
		auto unchanged = observation("preserve-before-Access", {{r::STATE_ST0,0x8608},{r::STATE_ST1,st1},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},{r::STATE_T,0x5aa5}});
		auto publication = observation("single-Access-publication", {{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},{r::STATE_T,0x5aa5}});
		auto retained = observation("no-late-auxiliary-update", {{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},{r::STATE_T,0x5aa5}});
		std::vector<register_setting> poison;
		for (unsigned i = 0; i < 8; ++i) {
			unchanged.expected.push_back({int(r::STATE_AR0) + int(i),pointers[i]});
			publication.expected.push_back({int(r::STATE_AR0) + int(i),i == selected ? result : pointers[i]});
			retained.expected.push_back({int(r::STATE_AR0) + int(i),0x2200 + i});
			poison.push_back({int(r::STATE_AR0) + int(i),0x2200 + i});
		}
		clocked_register_case c{{{0,std::move(initial)},{1,std::move(unchanged)},{4,std::move(publication)},
			{5,std::move(retained)}},3 + seed % 3,12};
		c.actions = {{4,std::move(poison)}};
		c.forbid_data_access = true;
		c.program_events = {{false,base + 0x600,opcode,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
