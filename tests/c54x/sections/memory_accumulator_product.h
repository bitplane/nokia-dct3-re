// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
#include "tms320c54x_test_memory_product_section.h"
namespace c54x_test { inline std::vector<clocked_register_case> section_cases() { return memory_product_cases(memory_product::accumulator); } }
