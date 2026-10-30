// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u64 inputs[]{0,1,0x1200,0xffff,0x7fffffff,0xff80000000ULL,0x0180001200ULL,0xff7fff1200ULL};
	constexpr u16 words[]{0,1,0x8000,0xffff,0x7fff,0x8000,0xffff,0x5678};
	constexpr u16 plain[]{0,1,0x1200,0xffff,0xffff,0,0x1200,0x1200};
	constexpr u16 unsigned_sat[]{0,1,0x1200,0xffff,0xffff,0xffff,0xffff,0xffff};
	constexpr u16 signed_sat[]{0,1,0x1200,0xffff,0xffff,0,0xffff,0};
	for (unsigned target = 0; target < 3; ++target)
	for (unsigned kind = 0; kind < (target ? 1U : 3U); ++kind)
	for (unsigned source = 0; source < 2; ++source)
	for (unsigned pattern = 0; pattern < 8; ++pattern)
	for (unsigned flags = 0; flags < (target ? 4U : 16U); ++flags)
	{
		bool const sst = flags & 1, sxm = flags & 2;
		bool const ovm = target ? pattern & 2 : flags & 4;
		bool const frct = target ? pattern & 4 : flags & 8;
		u16 const stored = sst ? sxm ? signed_sat[pattern] : unsigned_sat[pattern] : plain[pattern];
		u16 const address = target == 2 ? 0x1a : 0x0e + target;
		int const reg = target == 2 ? r::STATE_BRC : target ? r::STATE_TRN : r::STATE_T;
		// SPRU131G table7-13 gives T-to-multiply zero latency. Use separated
		// LDM consumers for TRN/BRC, not the old short-gap old-value projection.
		unsigned const gap = target ? 3 : (source + pattern + flags) % 2;
		unsigned const read = 6 + gap;
		s64 product = kind == 2 ? s64(stored) * words[pattern] : s64(s16(stored)) * s16(words[pattern]);
		if (frct) product *= 2;
		if (kind == 1) product = (product + 32768) & ~s64(65535);
		bool const overflow = !target && (product < -2147483648LL || product > 2147483647LL);
		u64 const result = target ? stored : u64(ovm && overflow ?
			std::clamp(product,s64(-2147483648LL),s64(2147483647LL)) : product) & 0xffffffffffULL;
		u16 const st0 = 0x1804 | (pattern & 1 ? 0x600 : 0);
		u16 const st1 = 0x800 | (sxm ? 0x100 : 0) | (ovm ? 0x200 : 0) |
			(frct ? 0x40 : 0) | (pattern & 2 ? 0x4000 : 0);
		u16 const consumer = target ? 0x4800 | ((source ^ 1) << 8) | address :
			(0x2000 + kind * 0x200) | ((source ^ 1) << 8) | 0x81;
		auto initial = initial_case("stlm-register", {u16(0x8800 | (source << 8) | address)});
		initial.program[1 + gap].value = consumer;
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_PMST,unsigned(sst)},{r::STATE_A,source ? 0xdead : 0x1111},
			{r::STATE_B,source ? 0x1111 : 0xdead},{r::STATE_T,0x9abc},
			{r::STATE_TRN,0x1357},{r::STATE_BRC,0x55aa},{r::STATE_AR1,0x5000}});
		initial.data = {{0x5000,words[pattern]}};
		auto final = observation("consumed", {{r::STATE_A,source ? result : inputs[pattern]},
			{r::STATE_B,source ? inputs[pattern] : result},{reg,0xbeef},
			{r::STATE_ST0,unsigned(st0 | (overflow ? source ? 0x400 : 0x200 : 0))},
			{r::STATE_ST1,st1},{r::STATE_PMST,unsigned(sst)}});
		for (int other : {int(r::STATE_T),int(r::STATE_TRN),int(r::STATE_BRC)})
			if (other != reg) final.expected.push_back({other,other == r::STATE_T ? 0x9abcU : other == r::STATE_TRN ? 0x1357U : 0x55aaU});
		clocked_register_case c{{{0,std::move(initial)},
			{6,observation("publication", {{reg,stored}})},{read + 1,std::move(final)}},
			std::array<unsigned,3>{3,5,read}[(source + pattern + flags + kind + target) % 3],18};
		c.require_idle = false;
		c.actions = {{5,{{source ? r::STATE_B : r::STATE_A,inputs[pattern]}}},
			{read,{{reg,0xbeef}}}};
		if (!target) c.actions.back().data = {{0x5000,0xdead}};
		clocked_data_event const write{true,address,stored,6};
		clocked_data_event const load{false,target ? address : 0x5000U,target ? stored : words[pattern],read};
		// Same-clock order is an implementation regression, not silicon timing.
		c.data_events = gap ? std::vector<clocked_data_event>{write,load} : std::vector<clocked_data_event>{load,write};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
