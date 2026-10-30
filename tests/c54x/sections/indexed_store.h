// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u64 values[]{0x123456789aULL,0xff80000001ULL,0x7fffffffULL,0xff7fffffffULL};
	for (unsigned kind = 0; kind < 4; ++kind)
	for (unsigned mode = 0; mode < (kind < 2 ? 4U : 2U); ++mode)
	for (unsigned reg = 0; reg < 8; ++reg) for (unsigned cmpt = 0; cmpt < 2; ++cmpt)
	for (unsigned scenario = 0; scenario < 8; ++scenario) for (unsigned selector = 0; selector < 4; ++selector) {
		bool const circular = kind < 2, asm_shift = kind == 1, immediate = kind == 3, b = selector & 1;
		unsigned const selected = cmpt && !reg ? 4 : reg, seed = kind + mode + reg + cmpt + scenario + selector;
		unsigned const flags = seed % 4, read = immediate ? 6 : 5;
		u16 const bk = selected == 0 && scenario % 4 ? 0x2000 : std::array<u16,4>{0,1,7,16}[scenario % 4];
		std::array<u16,8> pointers;
		for (unsigned ar = 0; ar < 8; ++ar) pointers[ar] = 0x1000 + ar * 0x100;
		pointers[0] = circular ? std::array<u16,4>{0,1,0xffff,bk ? u16(-bk) : u16(0x8000)}[scenario / 2] :
			std::array<u16,8>{0,1,0x20,0xffff,0x8000,0x7fff,0x1234,0xfffe}[scenario];
		if (circular || selected) pointers[selected] = scenario == 7 ? 0x20 :
			0x1000 + selected * 0x100 + (circular && selected && bk ? scenario % bk : 0);
		u16 const address = pointers[selected];
		bool const excluded = address < 0x60;
		int const step = circular ? (mode & 1 ? int(s16(pointers[0])) : 1) : int(s16(pointers[0]));
		int const delta = circular ? (mode < 2 ? -step : step) : mode ? step : -step;
		u16 next = address + delta;
		if (circular && bk) {
			unsigned const alignment = bk == 1 ? 2 : bk == 7 ? 8 : bk == 16 ? 32 : 0x4000;
			int const residue = (int(address % alignment) + delta) % int(bk);
			next = address / alignment * alignment + (residue < 0 ? residue + bk : residue);
		}
		int const shift = asm_shift ? std::array<int,4>{-16,-1,0,15}[scenario % 4] : 0;
		unsigned const raw = shift & 31;
		u16 const st1 = 0xa00 | (flags & 1 ? 0x100 : 0) | (cmpt ? 0x20 : 0) | raw;
		u16 const st0 = !excluded && cmpt && reg ? 0x608 | (reg << 13) : 0x8608;
		u64 const value = values[seed % 4];
		u64 shifted = value;
		if (shift >= 0) shifted = (shifted << shift) & 0xffffffffffULL;
		else if ((flags & 1) && (value & 0x8000000000ULL)) {
			s64 const number = s64(value) - (s64(1) << 40), divisor = s64(1) << -shift;
			shifted = u64(number / divisor - (number % divisor != 0)) & 0xffffffffffULL;
		} else shifted >>= -shift;
		u32 stored = u32(shifted);
		if (flags & 2) {
			if (flags & 1) {
				s64 const number = shifted & 0x8000000000ULL ? s64(shifted) - (s64(1) << 40) : s64(shifted);
				stored = u32(std::clamp(number,s64(-2147483648LL),s64(2147483647LL)));
			} else stored = u32(std::min(shifted,u64(0xffffffff)));
		}
		u16 const word = immediate ? std::array<u16,4>{0,1,0x8000,0xffff}[selector] : u16(stored >> (selector & 2 ? 16 : 0));
		u16 const opcode = (immediate ? 0x7600 : (asm_shift ? 0x8400 : 0x8000) | (selector << 8)) |
			(circular ? 0xc0 | (mode << 3) | reg : (mode ? 0xb0 : 0xa8) | reg);
		u32 const base = seed & 1 ? 0x20000 : 0;
		auto initial = initial_case("indexed-store-capture", {opcode,u16(immediate ? word : 0xf4e1),0xf4e1});
		for (auto &entry : initial.program) entry.address += base;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0x8608},{r::STATE_ST1,st1},
			{r::STATE_PMST,flags >> 1},{r::STATE_BK,bk},{r::STATE_T,0x5aa5},
			{r::STATE_A,b ? 0x33445566 : value ^ 0xffffffffffULL},{r::STATE_B,b ? value ^ 0xffffffffffULL : 0x33445566}});
		for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({r::STATE_AR0 + ar,0x3300 + ar});
		clocked_register_case c{{{0,std::move(initial)}},read - 1 + seed % 3,12};
		c.require_idle = !excluded; c.clear_pipeline_before_restore = asm_shift;
		for (unsigned tick = 1; tick <= 12; ++tick) {
			u64 const live = !excluded && tick > read ? value : value ^ 0xffffffffffULL;
			auto state = observation("read-address-execute-data", {{r::STATE_A,b ? 0x33445566 : live},
				{r::STATE_B,b ? live : 0x33445566},{r::STATE_T,0x5aa5},{r::STATE_PMST,flags >> 1},
				{r::STATE_ST0,tick >= read ? unsigned(st0) : 0x8608U},
				{r::STATE_ST1,asm_shift && !excluded && tick > read ? unsigned(st1 ^ 31) : unsigned(st1)},
				{r::STATE_BK,asm_shift && tick > 3 ? 0x1234U : unsigned(bk)}});
			state.expected.pop_back(); state.expected.push_back({r::STATE_ILLEGAL,excluded && tick >= read ? 1U : 0U});
			for (unsigned ar = 0; ar < 8; ++ar) state.expected.push_back({int(r::STATE_AR0 + ar),tick < read ? 0x3300U + ar :
				!excluded && tick > read ? 0x2200U + ar : !excluded && ar == selected ? unsigned(next) : unsigned(pointers[ar])});
			c.states.push_back({tick,std::move(state)});
		}
		c.states.back().observation.expected.push_back({r::STATE_IDLE,excluded ? 0U : 1U});
		if (asm_shift) c.actions.push_back({3,{{r::STATE_BK,0x1234}}});
		if (immediate) c.actions.push_back({4,{},false,{{base + 0x601,0xdead}}});
		clocked_register_action address_action{read - 1,{}};
		for (unsigned ar = 0; ar < 8; ++ar) address_action.values.push_back({r::STATE_AR0 + ar,pointers[ar]});
		c.actions.push_back(std::move(address_action));
		if (!excluded) {
			clocked_register_action data_action{read,{{int(b ? r::STATE_B : r::STATE_A),value}}};
			for (unsigned ar = 0; ar < 8; ++ar) data_action.values.push_back({r::STATE_AR0 + ar,0x2200 + ar});
			if (asm_shift) data_action.values.push_back({r::STATE_ST1,u64(st1 ^ 31)});
			c.actions.push_back(std::move(data_action));
		}
		c.data_events = std::vector<clocked_data_event>{};
		if (!excluded) c.data_events->push_back({true,address,word,read + 1});
		c.program_events = {{false,base + 0x600,opcode,0,false}};
		if (immediate) c.program_events.push_back({false,base + 0x601,word,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
