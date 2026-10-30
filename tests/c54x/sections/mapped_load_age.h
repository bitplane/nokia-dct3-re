// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	// LDM, LD, LDU, ADD, SUB, and LD shift16.
	for (unsigned operation = 0; operation != 6; ++operation)
	for (unsigned form = 0; form != (operation ? 4U : 1U); ++form)
	for (unsigned ar = 0; ar != 8; ++ar)
	for (unsigned b = 0; b != 2; ++b)
	for (u16 value : {0,0x7fff,0x8000,0xffff})
	for (unsigned younger = 0; younger != 3; ++younger)
	for (unsigned sxm = 0; sxm != 2; ++sxm) {
		unsigned const pointer = (ar + 2) % 8, words = form >= 2 ? 2 : 1, read = 4 + words;
		unsigned const seed = operation + form + ar + b + value + younger + sxm;
		u32 const base = seed % 2 ? 0x20000 : 0;
		u16 const mode = form == 0 ? 0x10 + ar : form == 1 ? 0x80 | pointer : form == 2 ? 0xf8 : 0xe0 | pointer;
		u16 const opcode = std::array<u16,6>{0x4800,0x1000,0x1200,0x0000,0x0800,0x4400}[operation] | (b << 8) | mode;
		u64 const sample = operation != 0 && operation != 2 && sxm && (value & 0x8000) ? 0xffffff0000ULL | value : value;
		u64 const result = operation == 4 ? (0 - sample) & 0xffffffffffULL : operation == 5 ? (sample * 65536) & 0xffffffffffULL : sample;
		bool const arithmetic = operation == 3 || operation == 4;
		u16 const status = arithmetic ? 0x1400 | (operation == 4 && value == 0 ? 0x800 : 0) : 0x1c00;
		u16 const updated = younger == 2 ? 0x3333 : u16(value + (younger ? -1 : 1));
		auto initial = initial_case("older-mapped-load-precedes-younger-register-update", {opcode});
		for (auto &word : initial.program) word.address += base;
		if (words == 2) initial.program[1].value = form == 2 ? 0x10 + ar : 0;
		initial.program[words].value = younger == 2 ? 0xe700 | (((ar + 1) % 8) << 4) | ar : (younger ? 0x6d88 : 0x6d90) | ar;
		initial.program[words + 1].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0x1c00},
			{r::STATE_ST1,0x800 | (sxm ? 0x100U : 0U)},
			{r::STATE_A,arithmetic && !b ? 0 : 0x123456789aULL},{r::STATE_B,arithmetic && b ? 0 : 0x123456789aULL}});
		auto final = observation("retained-old-word-and-surviving-younger-update", {{STATE_GENPC,base + 0x602 + words},
			{int(b ? r::STATE_B : r::STATE_A),result},{int(b ? r::STATE_A : r::STATE_B),0x123456789aULL},
			{r::STATE_ST0,status},{r::STATE_ST1,0x800 | (sxm ? 0x100U : 0U)}});
		for (unsigned i = 0; i != 8; ++i) {
			u16 const initial_value = i == ar ? value : (form == 1 || form == 3) && i == pointer ? 0x10 + ar : 0x3333;
			initial.initial.push_back({int(r::STATE_AR0) + int(i),initial_value});
			final.expected.push_back({int(r::STATE_AR0) + int(i),i == ar ? updated : initial_value});
		}
		clocked_register_case c{{{0,std::move(initial)},{16,std::move(final)}},read - 1 + seed % 3,16};
		c.data_events = std::vector<clocked_data_event>{};
		if (younger == 2) c.data_events->push_back({false,0x10 + (ar + 1) % 8,0x3333,read - 1});
		c.data_events->push_back({false,0x10 + ar,value,read});
		if (younger == 2) c.data_events->push_back({true,0x10 + ar,0x3333,read});
		if (words == 2) c.program_events = {{false,base + 0x601,u16(form == 2 ? 0x10 + ar : 0),0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
