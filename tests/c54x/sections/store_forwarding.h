// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned family = 0; family < 3; ++family)
	for (unsigned producer = 0; producer < (family == 2 ? 512U : 6U); ++producer)
	for (unsigned destination = 0; destination < 2; ++destination)
	for (unsigned gap = 0; gap < 2; ++gap) for (unsigned alias = 0; alias < 2; ++alias) {
		bool const asm_store = family == 2;
		unsigned const selector = producer % 4, raw = (producer / 4) % 32, flags = producer / 128;
		u64 const a = 0x123456789aULL, b = 0xfedcba9876ULL;
		u16 opcode, word;
		if (!family) {
			opcode = std::array<u16,6>{0x8082,0x8182,0x8282,0x8382,0x8c82,0x8d82}[producer];
			word = std::array<u16,6>{0x789a,0x9876,0x3456,0xdcba,0xbeef,0x8001}[producer];
		} else {
			u64 value = asm_store ? selector & 1 ? b : a : producer & 1 ? b : a;
			int const shift = asm_store ? int(raw) - (raw >= 16 ? 32 : 0) : producer < 4 ? 8 : producer == 4 ? 0 : 15;
			if (shift >= 0) value = (value << shift) & 0xffffffffffULL;
			else if ((flags & 1) && (value & 0x8000000000ULL)) {
				s64 const number = s64(value) - (s64(1) << 40), divisor = s64(1) << -shift;
				value = u64(number / divisor - (number % divisor != 0)) & 0xffffffffffULL;
			} else value >>= -shift;
			if (asm_store && (flags & 2)) {
				if (flags & 1) { s64 const number = value & 0x8000000000ULL ? s64(value) - (s64(1) << 40) : s64(value);
					value = u64(std::clamp(number,s64(-2147483648LL),s64(2147483647LL))); }
				else value = std::min(value,u64(0xffffffff));
			}
			word = u16(value >> ((asm_store ? selector & 2 : producer == 2 || producer == 3 || producer == 5) ? 16 : 0));
			opcode = asm_store ? 0x8482 | (selector << 8) : std::array<u16,6>{0x9808,0x9908,0x9a08,0x9b08,0x9800,0x9b0f}[producer];
		}
		unsigned const seed = family + producer + destination + gap + alias;
		u32 const base = seed & 1 ? 0x20000 : 0;
		u16 const st1 = asm_store ? 0x800 | (flags & 1 ? 0x100 : 0) | raw : 0x900;
		auto initial = initial_case("store-to-load-forwarding", {opcode,0xf495,0xf495,0xf4e1});
		initial.program[1 + gap].value = 0x1283 | (destination << 8);
		initial.program[2 + gap].value = 0xf4e1;
		for (auto &entry : initial.program) entry.address += base;
		initial.data = {{0x5000,0xdead},{0x5001,0xcafe}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,a},{r::STATE_B,b},
			{r::STATE_AR2,0x5000},{r::STATE_AR3,alias ? 0x5000U : 0x5001U},{r::STATE_T,0xbeef},{r::STATE_TRN,0x8001},
			{r::STATE_ST0,0xa608},{r::STATE_ST1,st1},{r::STATE_PMST,asm_store && (flags & 2) ? 1U : 0U}});
		u16 const loaded = alias ? word : 0xcafe;
		auto final = observation("no-stale-load-or-duplicate-transaction", {{r::STATE_A,destination ? a : loaded},
			{r::STATE_B,destination ? loaded : b},{r::STATE_AR2,0x6000},{r::STATE_AR3,0x6001},
			{r::STATE_T,0xbeef},{r::STATE_TRN,0x8001},{r::STATE_ST0,0xa608},{r::STATE_ST1,asm_store ? unsigned(st1 ^ 31) : unsigned(st1)}});
		clocked_register_case c{{{0,std::move(initial)},{18,std::move(final)}},5 + seed % 3,18};
		c.actions = {{5,{{r::STATE_AR2,0x6000}}}};
		if (asm_store) c.actions[0].values.push_back({r::STATE_ST1,unsigned(st1 ^ 31)});
		if (gap) c.actions.push_back({6,{{r::STATE_AR3,0x6001}}});
		else c.actions[0].values.push_back({r::STATE_AR3,0x6001});
		c.data_events = std::vector<clocked_data_event>{};
		if (gap) c.data_events->push_back({true,0x5000,word,6});
		if (gap || !alias) c.data_events->push_back({false,alias ? 0x5000U : 0x5001U,loaded,6 + gap});
		if (!gap) c.data_events->push_back({true,0x5000,word,6});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
