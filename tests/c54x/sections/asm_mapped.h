// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned source = 8; source < 24; ++source) for (unsigned form = 0; form < 4; ++form)
	for (unsigned raw = 0; raw < 32; ++raw) {
		unsigned const seed = source + form + raw, flags = seed % 4, ar = source == 23 ? 6 : 7, read = form >= 2 ? 6 : 5;
		u16 const word = (std::array<u16,4>{0,0xaaa0,0x5540,0xffe0}[flags] | raw) & (source == 10 || source == 13 ? 0xff : 0xffff);
		u16 const st1 = 0x800 | (flags & 1 ? 0x140 : 0) | (flags & 2 ? 0x200 : 0) | (raw ^ 31);
		u64 a = 0x123456789aULL, b = 0xfeabcd9876ULL;
		u16 t = 0x5aa5, trn = 0x1357;
		std::array<u16,8> pointers;
		for (unsigned i = 0; i < 8; ++i) pointers[i] = 0x1000 + i;
		if (source < 14) {
			unsigned const part = (source - 8) % 3;
			u64 &accumulator = source < 11 ? a : b;
			u64 const mask = part == 2 ? 0xff : 0xffff;
			accumulator = (accumulator & ~(mask << (16 * part))) | (u64(word) << (16 * part));
		} else if (source == 14) t = word; else if (source == 15) trn = word; else pointers[source - 16] = word;
		pointers[ar] = form == 2 ? source - 3 : source;
		u16 const mode = form == 0 ? source : form == 1 ? 0x90 | ar : form == 2 ? 0xe0 | ar : 0xf8;
		u16 const literal = form == 2 ? 3 : source;
		u32 const base = seed & 1 ? 0x20000 : 0;
		auto initial = initial_case("mapped-load-ASM", {u16(0x3200 | mode),form >= 2 ? literal : u16(0xf4e1),0xf4e1});
		for (auto &entry : initial.program) entry.address += base;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0x8800},{r::STATE_ST1,st1},
			{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,t},{r::STATE_TRN,trn}});
		for (unsigned i = 0; i < 8; ++i) initial.initial.push_back({r::STATE_AR0 + i,pointers[i]});
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,3>{3,read,read + 1}[seed % 3],16};
		for (unsigned tick = 1; tick <= 16; ++tick) {
			auto state = observation("mapped-source-retained-until-publication", {{r::STATE_ST0,0x8800},
				{r::STATE_ST1,tick > read ? unsigned((st1 & ~31) | raw) : unsigned(st1)},
				{r::STATE_A,tick > read ? 0x1122334455ULL : a},{r::STATE_B,tick > read ? 0xeeddccbbaaULL : b},
				{r::STATE_T,tick > read ? 0xbeefU : unsigned(t)},{r::STATE_TRN,tick > read ? 0x2222U : unsigned(trn)}});
			for (unsigned i = 0; i < 8; ++i) state.expected.push_back({int(r::STATE_AR0 + i),tick > read ? 0x2200U + i :
				unsigned(pointers[i]) + unsigned(form == 1 && i == ar && tick >= read - 1)});
			c.states.push_back({tick,std::move(state)});
		}
		if (form >= 2) c.actions.push_back({3,{},false,{{base + 0x601,0xdead}}});
		clocked_register_action poison{read,{{r::STATE_A,0x1122334455ULL},{r::STATE_B,0xeeddccbbaaULL},{r::STATE_T,0xbeef},{r::STATE_TRN,0x2222}}};
		for (unsigned i = 0; i < 8; ++i) poison.values.push_back({r::STATE_AR0 + i,0x2200 + i});
		c.actions.push_back(std::move(poison));
		c.data_events = std::vector<clocked_data_event>{{false,source,word,read}};
		if (form >= 2) c.program_events = {{false,base + 0x601,literal,3}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
