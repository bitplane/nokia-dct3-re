// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr std::array<u64,8> sources{0,1,0x7fffffff,0x80000000ULL,0xff80000000ULL,0xffffffffULL,0x123456789aULL,0xffffffffffULL};
	for (unsigned mode = 0; mode != 16; ++mode)
	for (unsigned shift = 0; shift != 16; ++shift)
	for (unsigned pattern = 0; pattern != sources.size(); ++pattern)
	for (bool sxm : {false,true})
	for (bool sst : {false,true})
	for (bool high : {false,true})
	for (bool b : {false,true}) {
		unsigned const parity = mode + shift + pattern + sxm + sst + high + b;
		u32 const base = parity % 2 ? 0x20000 : 0;
		u16 const st0 = 0xa002 | (parity % 2 ? 0x1e00 : 0);
		u16 const st1 = 0x800 | (sxm ? 0x100 : 0) | ((mode + pattern) % 2 ? 0x200 : 0) | (pattern % 2 ? 0x4020 : 0);
		u16 const address = 0x100f + 0x100 * (mode & 3);
		bool const linear = pattern >= 4;
		u16 const next = address + std::array<int,4>{0,-1,1,linear ? 3 : -13}[mode >> 2];
		// Architectural width reduction: shift within 40 bits, then optionally
		// saturate to signed/unsigned 32 bits before selecting the stored half.
		u64 projected = (sources[pattern] << shift) & 0xffffffffffULL;
		if (sst && sxm) {
			s64 const signed_value = projected & 0x8000000000ULL ? s64(projected) - (s64(1) << 40) : s64(projected);
			projected = u32(std::clamp<s64>(signed_value,-2147483648LL,2147483647LL));
		} else if (sst) projected = std::min<u64>(projected,0xffffffffULL);
		u16 const word = projected >> (high ? 16 : 0);
		u16 const opcode = 0x9800 | (high ? 0x200 : 0) | (b ? 0x100 : 0) | (mode << 4) | shift;
		auto initial = initial_case("Xmem-store-Read-address-Execute-data", {opcode,0xf4e1});
		for (auto &instruction : initial.program) instruction.address += base;
		initial.data = {{address,0xbeef}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_PMST,unsigned(sst)},{r::STATE_SP,0x4000},{r::STATE_AR0,3},{r::STATE_BK,linear ? 0U : 16U},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},{r::STATE_T,0xdead}});
		auto before = observation("Access-has-not-updated-pointer", {});
		auto read = observation("Read-captures-EAB-and-updates-one-pointer", {});
		auto final = observation("one-Execute-write-and-no-destination-read", {{STATE_GENPC,base + 0x602},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_PMST,unsigned(sst)},{r::STATE_SP,0x4000},
			{r::STATE_AR0,3},{r::STATE_BK,linear ? 0U : 16U},{r::STATE_T,0xdead},
			{r::STATE_A,b ? 0x123456789aULL : sources[pattern]},{r::STATE_B,b ? sources[pattern] : 0xfeabcd9876ULL}},{{address,word}});
		for (unsigned ar = 2; ar <= 5; ++ar) {
			u16 const pointer = 0x100f + 0x100 * (ar - 2);
			initial.initial.push_back({int(r::STATE_AR0) + int(ar),pointer});
			before.expected.push_back({int(r::STATE_AR0) + int(ar),pointer});
			read.expected.push_back({int(r::STATE_AR0) + int(ar),ar == 2 + (mode & 3) ? next : pointer});
			final.expected.push_back({int(r::STATE_AR0) + int(ar),ar == 2 + (mode & 3) ? 0x2222U : pointer});
		}
		clocked_register_case c{{{0,std::move(initial)},{4,std::move(before)},{5,std::move(read)},
			{6,observation("Execute-uses-late-accumulator-not-poisoned-pointer", {})},{12,std::move(final)}},3 + parity % 3,12};
		c.actions = {{5,{{int(r::STATE_AR0) + 2 + int(mode & 3),0x2222},{b ? r::STATE_B : r::STATE_A,sources[pattern]}}}};
		c.data_events = std::vector<clocked_data_event>{{true,address,word,6}};
		c.program_events = {{false,base + 0x600,opcode,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
