// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_COUNT_RETIREMENTS
#include "c54x_section_common.h"
#include "tms320c54x_test_repeated_accumulator_section.h"
namespace c54x_test { inline std::vector<clocked_register_case> section_cases() { return repeated_accumulator_cases(repeated_accumulator::load_numeric); } }
