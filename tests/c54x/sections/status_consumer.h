// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_XF
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned shift = 0; shift < 2; ++shift) for (unsigned sxm = 0; sxm < 2; ++sxm)
	for (unsigned field = 0; field < 32; ++field) {
		u16 const status = 0x800 | (sxm << 8) | field;
		u64 const source = 0x8012340000ULL | status;
		int const count = field < 16 ? int(field) : int(field) - 32;
		u64 expected = sxm ? 0xffffff8001ULL : 0x8001;
		if (shift) {
			if (count >= 0) expected = (source << count) & 0xffffffffffULL;
			else if (!sxm) expected = source / (u64(1) << -count);
			else {
				s64 const signed_source = s64(source) - (s64(1) << 40), divisor = s64(1) << -count;
				expected = u64(signed_source / divisor - (signed_source % divisor != 0)) & 0xffffffffffULL;
			}
		}
		unsigned const seed = shift + sxm + field;
		u32 const base = seed & 1 ? 0x20000 : 0;
		auto initial = initial_case("MVMD-ST1-to-adjacent-SXM-or-ASM-consumer", {0x730b,7,u16(shift ? 0xf682 : 0x1080),0xf4e1});
		for (auto &entry : initial.program) entry.address += base;
		initial.data = {{0x1400,0x8001}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa200},
			{r::STATE_ST1,status ^ 0x11fU},{r::STATE_A,0x55},{r::STATE_B,source},{r::STATE_AR0,0x1400}});
		auto final = observation("consumer-uses-new-status-fields", {{STATE_GENPC,base + 0x604},{r::STATE_A,expected},
			{r::STATE_B,source},{r::STATE_ST1,status},{r::STATE_ST0,0xa200,~u64(0x400)},{r::STATE_AR0,0x1400}});
		clocked_register_case c{{{0,std::move(initial)},{14,std::move(final)}},std::array<unsigned,3>{4,6,7}[seed % 3],14};
		c.data_events = std::vector<clocked_data_event>{{false,0xb,status,5},{true,7,status,6}};
		if (!shift) c.data_events->push_back({false,0x1400,0x8001,7});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
