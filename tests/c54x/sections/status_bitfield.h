// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned source = 0; source != 2; ++source) for (unsigned pattern = 0; pattern != 4; ++pattern)
		for (unsigned mask_index = 0; mask_index != 4; ++mask_index) for (unsigned tc = 0; tc != 2; ++tc)
			for (unsigned absolute = 0; absolute != 2; ++absolute) {
				u16 const st0 = std::array<u16,4>{0,0x0800,0xe600,0xee00}[pattern] | (tc ? 0x1000 : 0);
				u16 const st1 = std::array<u16,4>{0,0x0800,0x2000,0x3b80}[pattern];
				u16 const address = 6+source, sampled = source ? st1 : st0;
				u16 const mask = std::array<u16,4>{0,0x0800,0x1000,0xffff}[mask_index];
				u32 const base = (source+pattern+mask_index+tc+absolute)%2 ? 0x20000 : 0;
				auto initial = initial_case("BITF-captures-status-before-debugger-mutation",{u16(0x6100 | (absolute ? 0xf8 : address))});
				unsigned extension = 1;
				if (absolute) initial.program[extension++].value = address;
				initial.program[extension++].value = mask;
				initial.program[extension].value = 0xf4e1;
				for (auto &word : initial.program) word.address += base;
				initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
					{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfedcba9876ULL},{r::STATE_T,0x1234},{r::STATE_TRN,0x5678}});
				unsigned const read = absolute ? 7 : 6;
				u16 const live_st0 = source ? st0 : 0x2222, live_st1 = source ? 0x2222 : st1;
				u16 const result = (live_st0 & ~0x1000U) | ((sampled & mask) ? 0x1000 : 0);
				clocked_register_case c{{{0,std::move(initial)},
					{1,observation("before-status-read",{{r::STATE_ST0,st0},{r::STATE_ST1,st1}})},
					{read,observation("source-mutated-after-read",{{r::STATE_ST0,live_st0},{r::STATE_ST1,live_st1}})},
					{read+1,observation("TC-from-captured-word-other-status-bits-preserved",{{r::STATE_ST0,result},{r::STATE_ST1,live_st1},
						{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfedcba9876ULL},{r::STATE_T,0x1234},{r::STATE_TRN,0x5678}})}},
					read-1+(source+pattern+mask_index+tc+absolute)%3,18};
				c.actions = {{read,{{source ? r::STATE_ST1 : r::STATE_ST0,0x2222}},true}};
				c.data_events = std::vector<clocked_data_event>{{false,address,sampled,read}};
				if (absolute) c.retained_program_reads.push_back({base+0x601,address});
				c.retained_program_reads.push_back({base+0x601+absolute,mask});
				cases.push_back(std::move(c));
			}
	return cases;
}
}
