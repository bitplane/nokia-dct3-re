// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned destination = 0; destination != 2; ++destination)
		for (unsigned value = 0; value != 256; ++value) {
			bool const boundary = value == 0 || value == 0x7f || value == 0x80 || value == 0xff;
			for (unsigned flag_variant = 0; flag_variant != (boundary ? 2U : 1U); ++flag_variant) {
				u16 const st1 = ((value ^ destination ^ flag_variant) & 1) ? 0x100 : 0;
				auto initial = initial_case("atomic-short-immediate-zero-extension-and-destination",{0xf495,
					0x75f8,0x100,0x124,u16(0xe800 | (destination << 8) | value),0x75f8,0x100,0x124,0xf4e1});
				initial.initial.insert(initial.initial.end(),{{r::STATE_A,0x11111111},{r::STATE_B,0x22222222},
					{r::STATE_ST0,0x1800},{r::STATE_ST1,st1}});
				initial.data = {{0x100,0x1111}};
				auto final = observation("byte-result-preserves-other-accumulator-and-status",{{STATE_GENPC,0x609},
					{r::STATE_A,destination ? 0x11111111U : value},{r::STATE_B,destination ? value : 0x22222222U},
					{r::STATE_ST0,0x1800},{r::STATE_ST1,st1}},{{0x100,0x1111}});
				clocked_register_case c{{{0,std::move(initial)},{1,observation("before-byte-markers",{{STATE_GENPC,0x601}})},
					{2,observation("byte-markers-running",{})},{32,std::move(final)}},3,32};
				c.pipeline_frontend = false; c.observed_data_space = AS_IO;
				c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},{true,0x124,0x1111,0,false,{},4}};
				cases.push_back(std::move(c));
			}
		}
	return cases;
}
}
