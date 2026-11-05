// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_DISASSEMBLY
#include "c54x_section_common.h"
#include "tms320c54x_test_disassembly.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	auto initial=initial_case("disassembly-runner-is-independent-of-instruction-sequences",{0xf4e1});
	clocked_register_case c{{{0,std::move(initial)}},2,8};c.data_events=std::vector<clocked_data_event>{};
	return {std::move(c)};
}
}
