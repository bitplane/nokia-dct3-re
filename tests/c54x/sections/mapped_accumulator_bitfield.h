// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned producer = 0; producer != 2; ++producer) for (unsigned reg = 0; reg != (producer ? 1U : 8U); ++reg)
		for (unsigned pattern = 0; pattern != (producer ? 2U : 4U); ++pattern) for (unsigned mask_index = 0; mask_index != 4; ++mask_index)
			for (unsigned tc = 0; tc != 2; ++tc) for (unsigned absolute = 0; absolute != 2; ++absolute)
				for (unsigned gap = 0; gap != (producer ? 2U : 1U); ++gap) {
					u64 const a = producer ? 0x1234568001ULL : std::array<u64,4>{0x20,0xff80008000ULL,0x007fffffff,0x8000000000ULL}[pattern];
					u64 const b = producer ? 0x1e1e2 : std::array<u64,4>{0x1e1e2,0x0040000001,0xff80000000ULL,0x7fffffffffULL}[pattern];
					u16 const t = std::array<u16,4>{0x20,0x8000,0xffff,0x7f}[pattern], trn = std::array<u16,4>{0x10,0xffff,0x8000,0xff}[pattern];
					u16 const address = 8+reg, mask = std::array<u16,4>{0,1,0x7f,0xffff}[mask_index];
					u16 const sampled = producer ? pattern : reg < 3 ? u16(a >> (reg*16)) : reg < 6 ? u16(b >> ((reg-3)*16)) : reg == 6 ? t : trn;
					unsigned const mutation = producer ? 0 : reg < 3 ? r::STATE_A : reg < 6 ? r::STATE_B : reg == 6 ? r::STATE_T : r::STATE_TRN;
					u16 const st0 = 0x800 | (tc ? 0x1000 : 0), result_st0 = 0x800 | ((sampled & mask) ? 0x1000 : 0);
					// SXM/OVM do not participate in BITF; distribute their four settings.
					u16 const st1 = 0x800 | (((reg+pattern+mask_index+tc)%4) << 8);
					unsigned const prefix = producer ? 1+gap : 0, read = (absolute ? 7 : 6)+prefix;
					u32 const base = (reg+pattern+mask_index+tc+absolute+gap)%2 ? 0x20000 : 0;
					u16 const opcode = 0x6100 | (absolute ? 0xf8 : address);
					auto initial = initial_case("BITF-mapped-source-capture-and-TC-publication",{u16(producer ? 0xe800 | pattern : opcode)});
					initial.program[prefix].value = opcode;
					if (absolute) initial.program[prefix+1].value = address;
					initial.program[prefix+1+absolute].value = mask;
					initial.program[prefix+2+absolute].value = 0xf4e1;
					for (auto &word : initial.program) word.address += base;
					initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,a},{r::STATE_B,b},
						{r::STATE_T,t},{r::STATE_TRN,trn},{r::STATE_ST0,st0},{r::STATE_ST1,st1}});
					clocked_register_case c{{{0,std::move(initial)}},read-1+(reg+pattern+mask_index+tc+absolute+gap)%3,18};
					std::vector<unsigned> boundaries{1,read,read+1};
					if (producer) boundaries.push_back(6);
					std::sort(boundaries.begin(),boundaries.end());
					boundaries.erase(std::unique(boundaries.begin(),boundaries.end()),boundaries.end());
					for (unsigned tick : boundaries) {
						u64 const observed_a = tick >= read && mutation == r::STATE_A ? 0x012345abcdULL : producer && tick >= 6 ? pattern : a;
						u64 const observed_b = tick >= read && mutation == r::STATE_B ? 0x012345abcdULL : b;
						c.states.push_back({tick,observation("BITF-retains-source-word-not-live-register",{{r::STATE_A,observed_a},{r::STATE_B,observed_b},
							{r::STATE_ST0,tick > read ? result_st0 : st0},{r::STATE_ST1,st1},
							{r::STATE_T,tick >= read && mutation == r::STATE_T ? 0x2222U : t},
							{r::STATE_TRN,tick >= read && mutation == r::STATE_TRN ? 0x2222U : trn}})});
					}
					if (mutation) c.actions = {{read,{{mutation,mutation == r::STATE_A || mutation == r::STATE_B ? 0x012345abcdULL : 0x2222}},true}};
					c.data_events = std::vector<clocked_data_event>{{false,address,sampled,read}};
					if (absolute) c.retained_program_reads.push_back({base+0x601+prefix,address});
					c.retained_program_reads.push_back({base+0x601+prefix+absolute,mask});
					cases.push_back(std::move(c));
				}
	return cases;
}
}
