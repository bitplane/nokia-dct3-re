// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 xs[]{0xffff,0,0x8000,0x7fff}, ys[]{1,0xffff,0x7fff,0x8000};
	for (unsigned index = 0; index < 40; ++index) {
		bool const accumulation = index < 8;
		unsigned const variant = accumulation ? index : index - 8, pair = variant % 4;
		u16 const x = accumulation ? 0 : xs[pair], y = accumulation ? 0 : ys[pair];
		u16 const st0 = accumulation ? 0x1400 | (variant & 4 ? 0x200 : 0) :
			0x1200 | (x < y ? 0x800 : 0) | (variant >= 16 ? 0x400 : 0);
		u16 const st1 = accumulation ? 0x140 | (variant & 1 ? 0x200 : 0) :
			(variant & 4 ? 0x100 : 0) | (variant & 8 ? 0x200 : 0);
		u64 result_a, result_b;
		u16 expected_status, status_mask;
		if (accumulation) {
			result_a = 0;
			// FRCT doubles (-32768)^2; adding -1 avoids signed32 overflow.
			result_b = variant & 2 ? 0x7fffffff : variant & 1 ? 0x7fffffff : 0x80000000;
			expected_status = 0x400 | (!(variant & 2) || variant & 4 ? 0x200 : 0); status_mask = 0x600;
		} else {
			s64 const difference = (st1 & 0x100 ? s64(s16(x)) : s64(x)) - (st1 & 0x100 ? s64(s16(y)) : s64(y));
			s64 const shifted = difference * 65536;
			bool const overflow = shifted < -2147483648LL || shifted > 2147483647LL;
			result_a = u64(overflow && (st1 & 0x200) ? std::clamp(shifted,s64(-2147483648LL),s64(2147483647LL)) : shifted) & 0xffffffffffULL;
			result_b = 1; status_mask = 0xffff;
			expected_status = 0x1200 | (x >= y ? 0x800 : 0) | (overflow || variant >= 16 ? 0x400 : 0);
		}
		auto initial = initial_case("square-difference", {0xe201,0xf4e1});
		initial.initial.insert(initial.initial.end(), {{r::STATE_AR2,0x600},{r::STATE_AR3,0x601},
			{r::STATE_A,accumulation ? 0x80000000ULL : 0x10000ULL},{r::STATE_B,accumulation && (variant & 2) ? 0xffffffffffULL : 0},
			{r::STATE_T,0x1234},{r::STATE_ST0,st0},{r::STATE_ST1,st1}});
		initial.data = {{0x600,x},{0x601,y}};
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 3U : 1U,24};
		c.pipeline_frontend = false;
		c.states.push_back({24,observation("difference-and-square-boundary", {{r::STATE_A,result_a},{r::STATE_B,result_b},
			{r::STATE_ST0,expected_status,status_mask},{r::STATE_T,0x1234},{r::STATE_AR2,0x600},{r::STATE_AR3,0x601},{STATE_GENPC,0x602}})});
		c.data_events = std::vector<clocked_data_event>{{false,0x600,x,0,false},{false,0x601,y,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
