// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_DOMAIN_SCHEDULE_H
#define MAME_NOKIA_TMS320C54X_TEST_DOMAIN_SCHEDULE_H

#include <cstdint>
#include <iterator>
#include <stdexcept>

namespace c54x_test {

struct source_domain_instruction
{
	std::uint16_t opcode;
	std::uint16_t operation;
};

// Independent instruction inventory: SPRU172C's single-data-memory read/RMW
// forms, shifted read extensions, and the four implemented MMR-source forms.
// Do not derive this bank from the CPU's admission predicates.
constexpr source_domain_instruction source_domains[] = {
	{ 0x0000, 0 }, { 0x0100, 0 }, { 0x0200, 0 }, { 0x0300, 0 },
	{ 0x0400, 0 }, { 0x0500, 0 }, { 0x0600, 0 }, { 0x0700, 0 },
	{ 0x0800, 0 }, { 0x0900, 0 }, { 0x0a00, 0 }, { 0x0b00, 0 },
	{ 0x0c00, 0 }, { 0x0d00, 0 }, { 0x0e00, 0 }, { 0x0f00, 0 },
	{ 0x1000, 0 }, { 0x1100, 0 }, { 0x1200, 0 }, { 0x1300, 0 },
	{ 0x1400, 0 }, { 0x1500, 0 }, { 0x1600, 0 }, { 0x1700, 0 },
	{ 0x1800, 0 }, { 0x1900, 0 }, { 0x1a00, 0 }, { 0x1b00, 0 },
	{ 0x1c00, 0 }, { 0x1d00, 0 }, { 0x1e00, 0 }, { 0x1f00, 0 },
	{ 0x2000, 0 }, { 0x2100, 0 }, { 0x2200, 0 }, { 0x2300, 0 },
	{ 0x2400, 0 }, { 0x2500, 0 }, { 0x2600, 0 }, { 0x2700, 0 },
	{ 0x2800, 0 }, { 0x2900, 0 }, { 0x2a00, 0 }, { 0x2b00, 0 },
	{ 0x2c00, 0 }, { 0x2d00, 0 }, { 0x2e00, 0 }, { 0x2f00, 0 },
	{ 0x3000, 0 }, { 0x3100, 0 }, { 0x3200, 0 }, { 0x3300, 0 },
	{ 0x3400, 0 }, { 0x3500, 0 }, { 0x3600, 0 }, { 0x3700, 0 },
	{ 0x3800, 0 }, { 0x3900, 0 }, { 0x3a00, 0 }, { 0x3b00, 0 },
	{ 0x3c00, 0 }, { 0x3d00, 0 }, { 0x3e00, 0 }, { 0x3f00, 0 },
	{ 0x4000, 0 }, { 0x4100, 0 }, { 0x4200, 0 }, { 0x4300, 0 },
	{ 0x4400, 0 }, { 0x4500, 0 }, { 0x4600, 0 }, { 0x4700, 0 },
	{ 0x4b00, 0 }, { 0x4c00, 0 }, { 0x4d00, 0 }, { 0x5000, 0 },
	{ 0x5100, 0 }, { 0x5200, 0 }, { 0x5300, 0 }, { 0x5400, 0 },
	{ 0x5500, 0 }, { 0x5600, 0 }, { 0x5700, 0 }, { 0x5800, 0 },
	{ 0x5900, 0 }, { 0x5a00, 0 }, { 0x5b00, 0 }, { 0x5c00, 0 },
	{ 0x5d00, 0 }, { 0x5e00, 0 }, { 0x5f00, 0 }, { 0x6000, 0 },
	{ 0x6100, 0 }, { 0x6200, 0 }, { 0x6300, 0 }, { 0x6400, 0 },
	{ 0x6500, 0 }, { 0x6600, 0 }, { 0x6700, 0 }, { 0x6800, 0 },
	{ 0x6900, 0 }, { 0x6a00, 0 }, { 0x6b00, 0 }, { 0x7100, 0 },
	{ 0x7500, 0 }, { 0x7800, 0 }, { 0x7900, 0 }, { 0x7a00, 0 },
	{ 0x7b00, 0 }, { 0x7d00, 0 }, { 0x7f00, 0 }, { 0x6f00, 0x0c40 },
	{ 0x6f00, 0x0d40 }, { 0x6f00, 0x0c00 }, { 0x6f00, 0x0d00 }, { 0x6f00, 0x0e00 },
	{ 0x6f00, 0x0f00 }, { 0x6f00, 0x0c20 }, { 0x6f00, 0x0d20 }, { 0x6f00, 0x0e20 },
	{ 0x6f00, 0x0f20 }, { 0x4800, 0 }, { 0x4900, 0 }, { 0x4a00, 0 },
	{ 0x7300, 0 },
};
static_assert(std::size(source_domains) == 125);
constexpr unsigned domain_cases = 125 * 15 * 16;
constexpr unsigned domain_tc0_cases = 4 * 7 * 16;
constexpr unsigned domain_control_cases = (40 + 34 + 14 + 2 + 21) * 8 * 16;
constexpr unsigned domain_mac_source_cases = 80 * 4 * 2 * 2;
constexpr unsigned domain_mac_short_cases = 8 * 4 * 2 * 2;
// Mapped-source admission is distinct from the write-only addressing grammar.
// This table names the independently retained opcodes, not CPU predicates.
struct mapped_rejection_instruction { std::uint16_t opcode; unsigned sources; bool literal_only; };
constexpr mapped_rejection_instruction mapped_rejections[] = {
	{0x0600,80,false}, {0x0700,80,false},
	{0x0200,80,false}, {0x0300,80,false}, {0x0a00,80,false}, {0x0b00,80,false},
	{0x3c00,80,false}, {0x3d00,80,false}, {0x3e00,80,false}, {0x3f00,80,false},
	{0x4000,80,false}, {0x4100,80,false}, {0x4200,80,false}, {0x4300,80,false},
	{0x1800,16,false}, {0x1900,16,false}, {0x1a00,16,false},
	{0x1b00,16,false}, {0x1c00,16,false}, {0x1d00,16,false},
	{0x7208,86,true}, {0x7209,86,true}, {0x720a,86,true},
	{0x720b,86,true}, {0x720c,86,true}, {0x720d,86,true},
};
constexpr unsigned mapped_rejection_cases = 8328;
constexpr unsigned domain_total_cases = domain_cases + domain_tc0_cases + domain_control_cases + domain_mac_source_cases + domain_mac_short_cases + mapped_rejection_cases;

struct domain_input
{
	source_domain_instruction instruction;
	unsigned mode, context;
	std::uint16_t st0 = 0x1804, st1 = 0x0800, prefix = 0, tag = 0;
	std::uint16_t absolute_source = 0xffff;
	unsigned checkpoint = 0;
	std::uint16_t indirect_source = 0xffff;
	std::uint32_t key() const
	{
		return (std::uint32_t(instruction.opcode | mode) << 16) |
			(std::uint32_t(absolute_source != 0xffff ? absolute_source : indirect_source != 0xffff ? indirect_source : instruction.operation) << 4) | tag | context;
	}
};

inline domain_input select_domain(unsigned index)
{
	if (index < domain_cases)
	{
		unsigned const alias = index % 15;
		return {source_domains[index / (15 * 16)], alias < 8 ? 0x98 + alias : 0xf9 + alias - 8,
			(index / 15) % 16};
	}
	index -= domain_cases;
	domain_input input{};
	auto group = [&](unsigned opcode, unsigned operation, unsigned first, unsigned aliases,
		unsigned st0, unsigned st1, unsigned prefix, unsigned tag) {
		unsigned const count = aliases * 16;
		if (index >= count) { index -= count; return false; }
		input = {{std::uint16_t(opcode), std::uint16_t(operation)}, first + index % aliases,
			index / aliases, std::uint16_t(st0), std::uint16_t(st1), std::uint16_t(prefix), std::uint16_t(tag)};
		return true;
	};
	// Preserve the previously banked absolute-alias TC-clear identities first.
	for (unsigned opcode : {0x6800, 0x6900, 0x6a00, 0x6b00})
		if (group(opcode, 0, 0xf9, 7, 0x0804, 0x0800, 0, 0x8000)) return input;
	// Shift is a separate field from source admission. Keep zero in the base
	// census and these field/word-width boundary controls, not all 32 values.
	for (unsigned operation : {0x0c40, 0x0d40, 0x0c00, 0x0d00, 0x0e00, 0x0f00, 0x0c20, 0x0d20, 0x0e20, 0x0f20})
		for (unsigned shift : {1, 15, 16, 31})
			if (group(0x6f00, operation | shift, 0x98, 8, 0x1804, 0x0800, 0, 0)) return input;
	// Nonzero ASM and C16 remain distinct status contexts, not new opcodes.
	for (unsigned opcode : {0x2000, 0x2200, 0x2400, 0x2600, 0x2800, 0x2a00, 0x2c00, 0x2e00,
		0x3000, 0x3200, 0x3400, 0x3600, 0x3800, 0x3a00, 0x6400, 0x6600, 0x4600})
		for (unsigned destination : {0, 0x100})
			if (group(opcode | destination, 0, 0x98, 8, 0x1804, 0x0815, 0, 0x4000)) return input;
	for (unsigned opcode : {0x5000, 0x5200, 0x5400, 0x5800, 0x5a00, 0x5c00, 0x5e00})
		for (unsigned destination : {0, 0x100})
			if (group(opcode | destination, 0, 0x98, 8, 0x1804, 0x0880, 0, 0x2000)) return input;
	for (unsigned opcode : {0x1e00, 0x1f00})
		if (group(opcode, 0, 0x98, 8, 0x1804, 0x0800, 0xec03, 0x1000)) return input;
	for (unsigned opcode : {0x6000, 0x6100, 0x1800, 0x1900, 0x1a00, 0x1b00, 0x1c00, 0x1d00,
		0x7100, 0x7300, 0x7500, 0x7f00, 0x7d00, 0x7900, 0x7b00, 0x4c00, 0x4d00,
		0x6800, 0x6900, 0x6a00, 0x6b00})
		if (group(opcode, 0, 0x98, 8, 0x0804, 0x0800, 0, 0x8000)) return input;
	// Retain the old MAC guard cohort in its original order. Its checkpoint 3
	// retains prefetched instruction state, unlike the base census's 1/10 pair.
	if (index < domain_mac_source_cases + domain_mac_short_cases)
	{
		unsigned const form = index % 88, selectors = (index / 88) % 4;
		unsigned const context = ((index / 352) % 2 ? 2 : 0) | (index >= 704 ? 8 : 0);
		bool const absolute = form < 80;
		return {{std::uint16_t(0x6400 | (selectors << 8)), std::uint16_t(absolute ? 0 : 9)},
			absolute ? 0xf8U : 0x98U + form - 80, context, 0x1028, 0x0800, 0,
			std::uint16_t(absolute ? 0 : 0x0400),
			std::uint16_t(absolute ? form < 8 ? form : form + 16 : 0xffff), index >= 704 ? 5U : 3U};
	}
	index -= domain_mac_source_cases + domain_mac_short_cases;
	unsigned group_index = 0;
	for (auto const &group : mapped_rejections)
	{
		unsigned const forms = group.literal_only ? 1 : 3;
		unsigned const count = group.sources * forms * 2;
		if (index >= count) { index -= count; ++group_index; continue; }
		unsigned const source = index % group.sources, form = (index / group.sources) % forms;
		unsigned const page = index / (group.sources * forms);
		unsigned const point = (source + form + page + group_index) % 2;
		std::uint16_t const address = group.literal_only ? source < 16 ? source : source + 10 : source < 8 ? source : source + 16;
		bool const literal = group.literal_only || form == 1;
		domain_input result{{group.opcode, 0}, group.literal_only ? 0U : form == 0 ? address : form == 1 ? 0xf8U : 0x83U,
			(page << 1) | (point << 3), 0x1800, std::uint16_t(group.literal_only ? 0x900 : 0x800)};
		if (literal) result.absolute_source = address;
		else if (form == 2) result.indirect_source = address;
		result.checkpoint = (literal ? 4 : 3) + point;
		return result;
	}
	throw std::out_of_range("C54x domain case");
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_DOMAIN_SCHEDULE_H
