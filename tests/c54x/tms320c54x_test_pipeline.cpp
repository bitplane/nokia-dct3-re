// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {


void tms320c54x_test_state::start_operand_fetch_case(unsigned index)
{
	m_cpu->reset();
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	unsigned const family = index >= 45 ? 10 : index >= 25 ? 6 + (index - 25) / 4 : index / 4;
	unsigned const form = index == 45 ? 4 : index >= 46 ? 3 : index >= 25 ? (index - 25) % 4 : index % 4;
	static constexpr u16 families[] = { 0x7100, 0x7d00, 0x7500, 0x7400, 0x7e00, 0x7000, 0x6000, 0x6100, 0x8b00, 0x7f00, 0x7c00 };
	static constexpr u8 modes[] = { 0x00, 0x92, 0xe2, 0xf8, 0x9a };
	static constexpr u16 operands[] = { 0x0700, 0x0700, 0x0124, 0x0123, 0, 0x0700, 0xbeef, 0x8000 };
	u16 pc = 0x0800;
	if (index == 24)
	{
		program.write_word(pc++, 0xe001); // FIRS *AR2,*AR3,0900
		program.write_word(pc++, 0x0900);
	}
	else
	{
		program.write_word(pc++, families[family] | (index >= 46 ? 0xf9 + index - 46 : modes[form]));
		if (form == 2)
		{
			if (family == 10)
			{
				program.write_word(pc++, 0x0900);
				program.write_word(pc++, 5);
			}
			else if (family == 0 || family == 4 || family == 5 || family >= 6)
			{
				program.write_word(pc++, 5);
				if (family != 4 && family < 8) program.write_word(pc++, operands[family]);
			}
			else
			{
				program.write_word(pc++, operands[family]);
				program.write_word(pc++, 5);
			}
		}
		else
		{
			if (form == 3) program.write_word(pc++, 0x0600);
			if (family == 10) program.write_word(pc++, 0x0900);
			if (family != 4 && family < 8) program.write_word(pc++, operands[family]);
		}
	}
	m_operand_extension_count = pc - 0x0801;
	program.write_word(pc, 0xf4e1);
	program.write_word(0x0900, family == 9 ? 0xdead : 0xbeef);
	program.write_word(0x0700, 0);
	data.write_word(0x0600, family == 8 || family == 10 ? 0xdead : 0xbeef); data.write_word(0x0601, 0xcafe);
	data.write_word(0x05ff, 0xcafe);
	data.write_word(0x0700, family == 5 || family == 8 ? 0xbeef : 0);
	if (family == 8) m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0700);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_AR2, form == 4 ? 0x05ff : form == 2 ? 0x05fb : 0x0600}, {cpu_regs::STATE_AR3, 0x0601},
		{cpu_regs::STATE_A, index == 24 ? 0 : 0x0900}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_ST0, 0x000c},
		{cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x0800},
	});
	m_port_reads = m_port_writes = 0;
	m_operand_extensions = m_operand_reads = 0;
	m_operand_order_violation = false;
	m_operand_fetch_tap = program.install_read_tap(0x0801, 0x0802, "operand_extension_fetch",
		[this](offs_t address, u16 &, u16) {
			if (!machine().side_effects_disabled() && address - 0x0801 < m_operand_extension_count)
				m_operand_extensions |= 1U << (address - 0x0801);
		});
	auto observe = [this](offs_t, u16 &, u16) {
		if (!machine().side_effects_disabled())
		{
			++m_operand_reads;
			m_operand_order_violation |= m_operand_extensions != ((1U << m_operand_extension_count) - 1);
		}
	};
	m_operand_data_tap = data.install_read_tap(0x0600, 0x0700, "operand_data_read", observe);
	m_operand_io_tap = m_cpu->space(AS_IO).install_read_tap(0x0123, 0x0123, "operand_io_read", observe);
	m_operand_program_tap = program.install_read_tap(0x0900, 0x0900, "operand_program_read", observe);
	m_phase = 86000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}













tms320c54x_test_state::repeated_writa_fixture tms320c54x_test_state::repeated_writa_case(unsigned variant, bool standard_width)
{
	repeated_writa_fixture f{};
	f.standard_width = standard_width;
	if (standard_width) variant = variant % 4800 + (variant / 4800) * 9600;
	f.form = variant % 5; f.setup = (variant / 5) % 5; f.reg = (variant / 25) % 8;
	f.count = std::array<unsigned, 4>{0, 1, 7, 255}[(variant / 200) % 4];
	f.target = (standard_width ? std::array<u32, 3>{0x3200, 0x7ffe, 0xfffe} : std::array<u32, 3>{0x033200, 0x037ffe, 0x7ffffe})[(variant / 800) % 3];
	f.mapped = (variant / 2400) % 2; f.page = (variant / 4800) % 2 ? 0x20000 : 0;
	f.words = f.setup == 0 || f.setup == 2 ? 1 : 2;
	f.read = 5 + f.words + (f.setup >= 2 ? 2 : 0); f.write = f.read + 5;
	f.checkpoint = std::array<unsigned, 4>{f.read, f.read + 1, f.write, f.write + 3}[variant / 9600];
	f.source = f.form < 2 ? 0x1201 : 0x3000; f.direction = f.form == 3 ? -1 : f.form == 4 ? 1 : 0;
	f.count_address = f.setup == 2 ? 0x1202 : f.setup == 3 ? 0x1400 : 0x10 + ((f.reg + 1) % 8);
	return f;
}

u16 tms320c54x_test_state::repeated_writa_word(unsigned reg, unsigned index)
{ return 0x2468 ^ (reg * 0x1111) ^ (index * 0x0101); }

u32 tms320c54x_test_state::repeated_writa_destination(repeated_writa_fixture const &f, unsigned index)
{
	if (f.standard_width) return (f.target + index) & 0xffff;
	u32 const logical = (f.target + index) & 0x7fffff;
	return f.mapped && !(logical & 0x8000) ? logical & 0xffff : logical;
}














tms320c54x_test_state::delay_load_fixture tms320c54x_test_state::delay_dld_case(unsigned v)
{
	auto f = delay_load_case(v);
	bool const odd = (v / 192) % 2 == 0;
	f.address = odd ? 0x5007 : 0x5004;
	f.literal = f.form == 3 ? f.address : odd ? 2 : 0xffff;
	if (f.form == 1 || f.form == 2) f.final[f.selected] = f.address;
	f.st1 = (f.st1 & ~u16(0x200)) | (f.flags & 2 ? 0x80 : 0);
	f.memory_low = std::array<u16, 4>{0x1234, 0xffff, 0, 0x8001}[(v / 768) % 4];
	f.result = (u64(f.memory) << 16) | f.memory_low;
	if ((f.flags & 1) && (f.memory & 0x8000)) f.result |= 0xff00000000ULL;
	return f;
}

tms320c54x_test_state::delay_load_fixture tms320c54x_test_state::delay_add_case(unsigned v)
{
	delay_load_fixture f{};
	f.kind = std::array<unsigned, 4>{0, 3, 4, 5}[v % 4]; v /= 4;
	f.form = v % 6; v /= 6; unsigned const operand = v % 4; v /= 4;
	unsigned const initial = v % 4; v /= 4; f.destination = v % 2; v /= 2; f.flags = v % 4; v /= 4;
	f.caller = v % 2 ? 0x20000 : 0; v /= 2; f.target = f.kind == 4 ? f.caller ^ 0x20000 : f.caller;
	f.address = f.literal = std::array<u16, 6>{0x500, 0x13, 8, 9, 0x0b, 0x0c}[f.form];
	f.memory = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[operand];
	u64 const left = std::array<u64, 4>{0, 0x7fffffff, 0xff80000000ULL, 0xffffffffffULL}[initial];
	f.a = f.destination ? 0x123456789aULL : left; f.b = f.destination ? left : 0xfedcba9876ULL;
	if (f.address == 8 || f.address == 9) { unsigned const shift = f.address == 9 ? 16 : 0; f.a = (f.a & ~(0xffffULL << shift)) | (u64(f.memory) << shift); }
	if (f.address == 0x0b || f.address == 0x0c) { unsigned const shift = f.address == 0x0c ? 16 : 0; f.b = (f.b & ~(0xffffULL << shift)) | (u64(f.memory) << shift); }
	f.st0 = 0xa008 | (operand & 1 ? 0x600 : 0) | (initial & 1 ? 0x800 : 0);
	f.st1 = 0x800 | (f.flags & 1 ? 0x100 : 0) | (f.flags & 2 ? 0x200 : 0);
	u64 const source = f.destination ? f.b : f.a;
	s64 const addend = f.flags & 1 ? s16(f.memory) : f.memory;
	s64 const sum = (source & 0x8000000000ULL ? s64(source) - 0x10000000000LL : s64(source)) + addend;
	bool const overflow = sum > 0x7fffffffLL || sum < -0x80000000LL;
	bool const carry = u64(u32(source)) + u32(addend) > 0xffffffffULL;
	f.result = overflow && (f.flags & 2) ? (sum < 0 ? 0xff80000000ULL : 0x7fffffffULL) : u64(sum) & 0xffffffffffULL;
	f.final_st0 = (f.st0 & ~0x800) | (carry ? 0x800 : 0) | (overflow ? f.destination ? 0x200 : 0x400 : 0);
	for (unsigned i = 0; i < 8; ++i) f.initial[i] = 0x5100 + i;
	if (f.address == 0x13) f.initial[3] = f.memory;
	f.final = f.initial; f.read = delay_multiply_fixture::operand_read(f.kind, 1);
	f.checkpoint = std::array<unsigned, 3>{f.literal_clock(), f.read, f.read + 1}[v];
	return f;
}

tms320c54x_test_state::block_back_fixture tms320c54x_test_state::block_back_case(unsigned v)
{
	block_back_fixture f{};
	f.outer = std::array<unsigned, 4>{0, 1, 2, 7}[v % 4]; f.inner = std::array<unsigned, 4>{1, 2, 4, 8}[(v / 4) % 4];
	f.kind = (v / 16) % 2; f.b = (v / 32) % 2; f.base = (v / 64) % 2 ? 0x20000 : 0; f.interrupt = (v / 128) % 3;
	f.checkpoint = std::array<unsigned, 4>{9, 12, 15, 19}[v / 384]; f.end = f.kind ? 0x60d : 0x60a; f.st1 = f.interrupt ? 0x100 : 0x900;
	return f;
}











} // namespace c54x_test
