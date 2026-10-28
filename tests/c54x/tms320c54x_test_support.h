// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_SUPPORT_H
#define MAME_NOKIA_TMS320C54X_TEST_SUPPORT_H

#include <cstdint>
#include <initializer_list>

namespace c54x_test {

struct register_setting
{
	int index;
	std::uint64_t value;

	template <typename Index, typename Value>
	constexpr register_setting(Index index, Value value)
		: index(int(index)), value(std::uint64_t(value)) { }
};

// Stimulus only. Do not use a batch when later values depend on earlier writes:
// initializer-list values are evaluated before the first CPU register write.
template <typename Cpu>
void set_registers(Cpu &cpu, std::initializer_list<register_setting> settings)
{
	for (auto const &setting : settings)
		cpu.set_state_int(setting.index, setting.value);
}

struct memory_word
{
	std::uint32_t address;
	std::uint16_t value;

	template <typename Address, typename Value>
	constexpr memory_word(Address address, Value value)
		: address(std::uint32_t(address)), value(std::uint16_t(value)) { }
};

// Like register batches, words must not read values changed by earlier writes.
template <typename Space>
void write_words(Space &space, std::initializer_list<memory_word> words)
{
	for (auto const &word : words)
		space.write_word(word.address, word.value);
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_SUPPORT_H
