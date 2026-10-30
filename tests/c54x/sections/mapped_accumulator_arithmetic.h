// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned producer = 0; producer != 2; ++producer) for (unsigned reg = 0; reg != (producer ? 1U : 8U); ++reg)
		for (unsigned operation = 0; operation != 4; ++operation) for (unsigned flags = 0; flags != 4; ++flags)
			for (unsigned pattern = 0; pattern != (producer ? 2U : 4U); ++pattern) for (unsigned absolute = 0; absolute != 2; ++absolute)
				for (unsigned gap = 0; gap != (producer ? 2U : 1U); ++gap) {
					bool const destination_b = operation & 1, subtract = operation & 2, sxm = flags & 1, saturate = flags & 2;
					u64 const a = producer ? 0x1234568001ULL : std::array<u64,4>{0x20,0xff80008000ULL,0x007fffffff,0x8000000000ULL}[pattern];
					u64 const b = producer ? 0x1e1e2 : std::array<u64,4>{0x1e1e2,0x0040000001,0xff80000000ULL,0x7fffffffffULL}[pattern];
					u16 const t = std::array<u16,4>{0x20,0x8000,0xffff,0x7f}[pattern], trn = std::array<u16,4>{0x10,0xffff,0x8000,0xff}[pattern];
					u16 const produced = pattern ? 0x20 : 0, address = 8+reg;
					// The adjacent direct form samples before LD retires in this model.
					// Keep this as an implementation contract, not a silicon hazard rule.
					u64 const sampled_a = producer && (absolute || gap) ? produced : a;
					u16 const sampled = reg < 3 ? u16(sampled_a >> (reg*16)) : reg < 6 ? u16(b >> ((reg-3)*16)) : reg == 6 ? t : trn;
					unsigned mutation = 0;
					u64 live_a = producer ? produced : a, live_b = b;
					if (!producer) {
						if (reg < 3 && destination_b) { mutation = r::STATE_A; live_a = 0x012345abcdULL; }
						else if (reg >= 3 && reg < 6 && !destination_b) { mutation = r::STATE_B; live_b = 0x012345abcdULL; }
						else if (reg >= 6) mutation = reg == 6 ? r::STATE_T : r::STATE_TRN;
					}
					u64 const left_bits = destination_b ? live_b : live_a;
					s64 const left = (left_bits & 0x8000000000ULL) ? s64(left_bits)-0x10000000000LL : s64(left_bits);
					s64 const right = sxm && sampled >= 0x8000 ? s64(sampled)-0x10000 : sampled;
					s64 const wide = subtract ? left-right : left+right;
					bool const overflow = wide < -0x80000000LL || wide > 0x7fffffffLL;
					bool const carry = subtract ? u32(left_bits) >= u32(right) : u64(u32(left_bits))+u32(right) >= 0x100000000ULL;
					u64 const result = u64(saturate && overflow ? (wide < 0 ? -0x80000000LL : 0x7fffffffLL) : wide) & 0xffffffffffULL;
					u16 const st1 = 0x800 | (sxm ? 0x100 : 0) | (saturate ? 0x200 : 0);
					u16 const result_st0 = 0x1000 | (carry ? 0x800 : 0) | (overflow ? destination_b ? 0x200 : 0x400 : 0);
					unsigned const prefix = producer ? 1+gap : 0, read = (absolute ? 6 : 5)+prefix;
					u32 const base = (reg+operation+flags+pattern+absolute+gap)%2 ? 0x20000 : 0;
					u16 const opcode = (subtract ? 0x800 : 0) | (destination_b ? 0x100 : 0) | (absolute ? 0xf8 : address);
					auto initial = initial_case("mapped-ADD-SUB-sampled-word-and-live-destination",{u16(producer ? 0xe800 | produced : opcode)});
					initial.program[prefix].value = opcode;
					if (absolute) initial.program[prefix+1].value = address;
					initial.program[prefix+1+absolute].value = 0xf4e1;
					for (auto &word : initial.program) word.address += base;
					initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,a},{r::STATE_B,b},
						{r::STATE_T,t},{r::STATE_TRN,trn},{r::STATE_ST0,0x1800},{r::STATE_ST1,st1}});
					clocked_register_case c{{{0,std::move(initial)}},read-1+(reg+operation+flags+pattern+absolute+gap)%3,16};
					std::vector<unsigned> boundaries{1,read,read+1};
					if (producer) boundaries.push_back(6);
					std::sort(boundaries.begin(),boundaries.end());
					boundaries.erase(std::unique(boundaries.begin(),boundaries.end()),boundaries.end());
					for (unsigned tick : boundaries) {
						u64 observed_a = producer && tick >= 6 ? produced : a, observed_b = b;
						if (tick >= read) { observed_a = live_a; observed_b = live_b; }
						if (tick > read) (destination_b ? observed_b : observed_a) = result;
						c.states.push_back({tick,observation("mapped-arithmetic-publication",{{r::STATE_A,observed_a},{r::STATE_B,observed_b},
							{r::STATE_ST0,tick > read ? result_st0 : 0x1800U},{r::STATE_ST1,st1},
							{r::STATE_T,tick >= read && mutation == r::STATE_T ? 0x2222U : t},
							{r::STATE_TRN,tick >= read && mutation == r::STATE_TRN ? 0x2222U : trn}})});
					}
					if (mutation) c.actions = {{read,{{mutation,mutation == r::STATE_A || mutation == r::STATE_B ? 0x012345abcdULL : 0x2222}},true}};
					c.data_events = std::vector<clocked_data_event>{{false,address,sampled,read}};
					if (absolute) c.retained_program_reads = {{base+0x601+prefix,address}};
					cases.push_back(std::move(c));
				}
	return cases;
}
}
