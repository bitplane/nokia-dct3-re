// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 6144; ++index) {
		bool const consumer = index >= 3072;
		unsigned const v = consumer ? index - 3072 : index;
		bool const immediate = consumer && v >= 1536;
		unsigned const raw = consumer ? v % 32 : (v / 48) % 32, form = consumer ? 0 : v % 6, ar = consumer ? 2 : (v / 6) % 8;
		unsigned const flags = consumer ? (v / 192) % 4 : (raw + ar + form) % 4;
		bool const compatibility = !consumer && v / 1536, negative = consumer && (v / 96) % 2;
		unsigned const operation = consumer ? (v / 32) % 3 : 0, gap = consumer ? (v / 768) % 2 : 0, selected = compatibility && !ar ? 4 : ar;
		u16 const word = std::array<u16,4>{0,0xaaa0,0x5540,0xffe0}[flags] | raw;
		u16 const st0 = 0x8820 | (!consumer && (flags & 1) ? 0x1600 : 0);
		u16 const st1 = 0x800 | (consumer ? (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0) :
			std::array<u16,4>{0,0x1c0,0x200,0x3c0}[flags]) | (compatibility ? 0x20 : 0) | (form == 1 ? 0x4000 : 0) | (raw ^ 31);
		u16 final_st0 = compatibility && ar && form >= 2 && form <= 4 ? (st0 & 0x1fff) | (ar << 13) : st0;
		u64 const a = consumer ? negative ? 0xfffffffff9ULL : 7 : 0x123456789aULL, b = consumer ? 0x31 : 0xfeabcd9876ULL;
		u64 result = b;
		if (consumer) {
			int const shift = int(raw) - (raw >= 16 ? 32 : 0);
			u64 operand = shift >= 0 ? (a << shift) & 0xffffffffffULL :
				(flags & 1) && negative ? u64(-((7 + (s64(1) << -shift) - 1) >> -shift)) & 0xffffffffffULL : a >> -shift;
			result = operand;
			if (operation) {
				s64 const right = operand & 0x8000000000ULL ? s64(operand) - (s64(1) << 40) : s64(operand);
				s64 const sum = operation == 2 ? 0x31 - right : 0x31 + right;
				bool const carry = operation == 2 ? u32(b) >= u32(operand) : u64(u32(b)) + u32(operand) > 0xffffffffULL;
				bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
				final_st0 = (st0 & ~0x800) | (carry ? 0x800 : 0) | (overflow ? 0x200 : 0);
				result = u64(overflow && (flags & 2) ? std::clamp(sum,s64(-2147483648LL),s64(2147483647LL)) : sum) & 0xffffffffffULL;
			}
		}
		unsigned const read = 5 + unsigned(form >= 3), execute = read + 2 + gap;
		u32 const base = (raw + ar + form + flags) & 1 ? 0x20000 : 0;
		u16 const mode = form < 2 ? 2 : form == 2 ? 0x90 | ar : form == 3 ? 0xe0 | ar : form == 4 ? 0xe8 | ar : 0xf8;
		auto initial = initial_case("ASM-load-and-consumer", {u16(immediate ? 0xed00 | raw : 0x3200 | mode)});
		for (auto &entry : initial.program) entry.address += base;
		if (form >= 3) initial.program[1].value = form == 5 ? 0x1002 : 3;
		unsigned const words = form >= 3 ? 2 : 1;
		if (consumer) initial.program[words + gap].value = std::array<u16,3>{0xf582,0xf580,0xf581}[operation];
		initial.program[words + (consumer ? 1 + gap : 0)].value = 0xf4e1;
		initial.data = {{0x1002,word}};
		std::array<u16,8> pointers{};
		for (unsigned i = 0; i < 8; ++i) pointers[i] = i == selected ? form == 3 || form == 4 ? 0xfff : 0x1002 : 0x1200 + i;
		auto final_pointers = pointers;
		if (form == 2) ++final_pointers[selected]; else if (form == 4) final_pointers[selected] = 0x1002;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_SP,0x1000},{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,0x5aa5}});
		for (unsigned i = 0; i < 8; ++i) initial.initial.push_back({int(r::STATE_AR0 + i),pointers[i]});
		unsigned const checkpoint = consumer ? std::array<unsigned,3>{read,read + 1,execute}[(raw + ar + flags) % 3] :
			std::array<unsigned,3>{3,read,read + 1}[(raw + ar + flags) % 3];
		clocked_register_case c{{{0,std::move(initial)}},checkpoint,16};
		for (unsigned tick = 1; tick <= 16; ++tick) {
			auto state = observation("ASM-publication-and-zero-latency-consumer", {{r::STATE_ST1,tick > read ? u64((st1 & ~31) | raw) : st1},
				{r::STATE_ST0,consumer && tick >= execute ? final_st0 : !consumer && tick >= read - 1 ? final_st0 : st0},
				{r::STATE_A,a},{r::STATE_B,consumer && tick >= execute ? result : b},{r::STATE_T,0x5aa5}});
			for (unsigned i = 0; i < 8; ++i) state.expected.push_back({int(r::STATE_AR0 + i),tick < read - 1 ? pointers[i] : tick == read - 1 ? final_pointers[i] : 0x2200U + i});
			c.states.push_back({tick,std::move(state)});
		}
		for (unsigned tick = 2; tick <= read; ++tick) {
			clocked_register_action action{tick,{},false};
			if (immediate && tick == 2) action.program.push_back({base + 0x600,u16(0xed00 | (raw ^ 31))});
			if (form >= 3 && tick == 3) action.program.push_back({base + 0x601,0xdead});
			if (tick == read - 1) for (unsigned i = 0; i < 8; ++i) action.values.push_back({int(r::STATE_AR0 + i),0x2200U + i});
			if (tick == read) action.data.push_back({0x1002,u16(word ^ 31)});
			if (!action.program.empty() || !action.values.empty() || !action.data.empty()) c.actions.push_back(std::move(action));
		}
		c.data_events = std::vector<clocked_data_event>{};
		if (!immediate) c.data_events->push_back({false,0x1002,word,read});
		if (form >= 3) c.program_events = {{false,base + 0x601,u16(form == 5 ? 0x1002 : 3),3}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
