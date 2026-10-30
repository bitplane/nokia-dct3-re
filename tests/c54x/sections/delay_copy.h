// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 values[]{1,1,1,0xffff,0x8000,0xffff,0x1234,0xffff};
	for (unsigned control = 0; control != 7; ++control)
	for (unsigned form = 0; form != 5; ++form) {
		if (control && form == 1) continue; // SP-relative DELAY is not admitted in these delay packets.
		for (unsigned reg = 0; reg != 8; ++reg)
		for (unsigned pattern = 0; pattern != 8; ++pattern)
		for (unsigned sxm = 0; sxm != 2; ++sxm)
		for (unsigned cmpt = 0; cmpt != 2; ++cmpt) {
			unsigned const seed = control + form + reg + pattern + sxm + cmpt;
			u32 const base = seed % 2 ? 0x20000 : 0, target = control == 5 ? base ^ 0x20000 : base;
			unsigned const read = control ? control == 6 ? 8 : 7 : 5;
			unsigned const selected = cmpt && reg == 0 ? 3 : reg;
			u16 const mode = form < 2 ? reg * 16 + pattern + sxm * 8 : (form == 2 ? 0x80 : form == 3 ? 0x88 : 0x90) | reg;
			u16 const address = form == 0 ? 0x200 + mode : form == 1 ? 0x1000 + mode : 0x5007;
			u16 const next = form == 3 ? 0x5006 : form == 4 ? 0x5008 : 0x5007;
			u16 const st0 = 0x1804 | (cmpt ? 0x6000 : 0) | (pattern & 1 ? 0x600 : 0);
			u16 const result_st0 = cmpt && reg && form >= 2 ? (st0 & 0x1fff) | (reg << 13) : st0;
			u16 const st1 = 0x800 | (sxm ? 0x100 : 0) | (seed & 1 ? 0x200 : 0) | (cmpt ? 0x20 : 0) | (form == 1 ? 0x4000 : 0);
			u64 const a = control == 2 ? 0 : control == 3 ? 1 : 0x123456789aULL;
			auto initial = initial_case("DELAY-retains-copy-across-pointer-and-source-poison", {0xf495});
			for (auto &word : initial.program) word.address += base;
			unsigned const delay = control ? control == 6 ? 1 : 2 : 0;
			if (control) {
				initial.program[0].value = control == 5 ? 0xfb80 | (target >> 16) :
					std::array<u16,7>{0,0xf273,0xfa45,0xfa45,0xf274,0,0xfe00}[control];
				if (control != 6) initial.program[1].value = 0x700;
				initial.program[4].value = 0xf4e1;
				initial.program.push_back({target + 0x700,0xf4e1});
			} else initial.program[2].value = 0xf4e1;
			initial.program[delay].value = 0x4d00 | mode;
			initial.data = {{address,values[pattern]},{u32(address + 1),0xbeef}};
			if (control) initial.data.push_back({0x1000,0x700});
			initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
				{r::STATE_SP,0x1000},{r::STATE_BK,16},{r::STATE_A,a},{r::STATE_B,0xfedcba9876ULL},
				{r::STATE_T,0x5aa5},{r::STATE_TRN,0xa55a}});
			auto access = observation("Access-updates-only-owned-pointer", {{r::STATE_ST0,result_st0}});
			auto retained = observation("poisoned-pointers-are-not-written-back", {});
			auto final = observation("one-successor-write-and-owned-control-frame", {{STATE_GENPC,control ? control == 3 ? base + 0x605 : target + 0x701 : base + 0x603},
				{r::STATE_SP,control == 4 ? 0xfffU : control == 5 ? 0xffeU : control == 6 ? 0x1001U : 0x1000U},
				{r::STATE_ST0,result_st0},{r::STATE_ST1,st1},{r::STATE_A,a},{r::STATE_B,0xfedcba9876ULL},
				{r::STATE_T,0x5aa5},{r::STATE_TRN,0xa55a}},{{address,0xdead},{u32(address + 1),values[pattern]}});
			std::vector<register_setting> poison;
			for (unsigned ar = 0; ar != 8; ++ar) {
				initial.initial.push_back({int(r::STATE_AR0) + int(ar),0x5007});
				access.expected.push_back({int(r::STATE_AR0) + int(ar),form >= 2 && ar == selected ? next : 0x5007U});
				retained.expected.push_back({int(r::STATE_AR0) + int(ar),0x6000 + ar});
				final.expected.push_back({int(r::STATE_AR0) + int(ar),0x6000 + ar});
				poison.push_back({int(r::STATE_AR0) + int(ar),0x6000 + ar});
			}
			clocked_register_case c{{{0,std::move(initial)},{read - 1,std::move(access)},
				{read,std::move(retained)},{20,std::move(final)}},std::array<unsigned,3>{3,read - 1,read}[seed % 3],20};
			c.actions = {{read - 1,std::move(poison)},{read,{},false,{}, {},{{address,0xdead}}}};
			c.data_events = std::vector<clocked_data_event>{};
			if (control == 4 || control == 5) c.data_events->push_back({true,0xfff,0x604,0,false});
			if (control == 6) c.data_events->push_back({false,0x1000,0x700,0,false});
			c.data_events->push_back({false,address,values[pattern],read});
			if (control == 5) c.data_events->push_back({true,0xffe,u16(base >> 16),0,false});
			c.data_events->push_back({true,u32(address + 1),values[pattern],read + 1});
			cases.push_back(std::move(c));
		}
	}
	return cases;
}
}
