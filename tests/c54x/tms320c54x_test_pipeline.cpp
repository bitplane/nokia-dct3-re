// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"
#include "tms320c54x_test_word_examples.h"

namespace c54x_test {

void tms320c54x_test_state::start_instruction_word_case(unsigned index)
{
	m_cpu->reset();
	prepare_case(*m_cpu, instruction_word_examples().at(index));
	m_phase = 88000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

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

void tms320c54x_test_state::start_pipeline_rmw_case(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_rmw_case = variant;
	m_pipeline_operand_reads = m_pipeline_store_writes = m_vector_acks = 0;
	m_pipeline_extensions_reads = 0;
	m_pipeline_replay = false;
	m_pipeline_expected_cycle = 1;
	unsigned const kind = (variant / 2) % 3;
	bool const absolute = kind == 1;
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i)
	{
		program.write_word(0x600 + i, 0xf495);
		program.write_word(0x40 + i, 0xf495);
	}
	program.write_word(0x600, (variant & 1 ? 0x6900 : 0x6800) | (absolute ? 0xf8 : kind == 2 ? 0x91 : 0x20));
	program.write_word(0x601, absolute ? 0x420 : 0xe833);
	if (absolute) program.write_word(0x602, 0xe833);
	program.write_word(0x40, 0xe955);
	data.write_word(0x420, 0x55aa); data.write_word(0x4a0, 0xbeef);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_ST0, 8}, {cpu_regs::STATE_ST1, 0},
		{cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_IMR, 1}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_SP, 0x1000},
		{cpu_regs::STATE_AR1, 0x420}, {cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {STATE_GENPC, 0x600},
	});
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_fetch_tap = program.install_read_tap(0x601, 0x602, "pipeline_rmw_extensions",
		[this](offs_t address, u16 &, u16) {
			if (machine().side_effects_disabled() || (address == 0x602 && (m_pipeline_rmw_case / 2) % 3 != 1)) return;
			expect(m_cpu->pipeline_cycles() == (address == 0x601 ? 3 : 4),
				"RMW extension words use independent program-bus Fetch clocks");
			++m_pipeline_extensions_reads;
		});
	m_pipeline_operand_tap = data.install_read_tap(0x420, 0x420, "pipeline_rmw_read",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == ((m_pipeline_rmw_case / 2) % 3 == 1 ? 7 : 6),
				"RMW target is sampled in Read, before Execute");
			++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = data.install_write_tap(0x420, 0x420, "pipeline_rmw_write",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == ((m_pipeline_rmw_case / 2) % 3 == 1 ? 8 : 7) &&
				value == (m_pipeline_rmw_case & 1 ? 0xfdbb : 0x4022),
				"RMW writes the captured Read value combined with its owned literal in Execute");
			++m_pipeline_store_writes;
		});
	m_phase = 251110;
	m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_mask_rmw_case()
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i)
	{
		program.write_word(0x600 + i, 0xf495);
		program.write_word(0x40 + i, 0xf495);
	}
	program.write_word(0x600, 0x68f8); // ANDM #fffe,*(IMR).
	program.write_word(0x601, 0); program.write_word(0x602, 0xfffe);
	program.write_word(0x40, 0xe955);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_IMR, 1}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_B, 0},
		{STATE_GENPC, 0x600},
	});
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false;
	m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_store_writes = m_vector_acks = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0, "pipeline_imr_read",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 7 && value == 1, "RMW samples the real IMR register in Read");
			++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0, "pipeline_imr_write",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 8 && value == 0, "Pending RMW clears IMR through its actual Execute-stage bus write");
			++m_pipeline_store_writes;
		});
	m_phase = 251130;
	m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}









void tms320c54x_test_state::start_pipeline_pmst(unsigned variant)
{
	bool const popd = profile_is("pmst-popd");
	bool const repeat = profile_is("pop-repeat");
	bool const iptr = profile_is("pmst-iptr");
	bool const boundary = profile_is("pmst-prefetch");
	bool const pop = popd || repeat || iptr || boundary || profile_is("pmst-pop");
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_pipeline_peripheral_case = variant;
	u16 const value = (repeat ? std::array<u16, 4>{0, 1, 0x8000, 0xffff} : iptr ? std::array<u16, 4>{0, 0x20, 0x2020, 0x3020} : std::array<u16, 4>{0, 0x20, 0x2020, 0xffaa})[variant % 4];
	u16 const destination = repeat ? 0x1a + variant / 64 : 0x1d;
	u32 const base = (BIT(variant, 2) ? 3 : 2) << 16;
	u16 const target = BIT(variant, 3) ? 0x8700 : 0x700;
	u16 const entry = boundary ? 0x600 : 0x9000;
	auto &program = m_cpu->space(AS_PROGRAM);
	if (boundary)
		for (unsigned i = 0; i < 64; ++i) { program.write_word(base + entry + i, 0xf495); program.write_word(entry + i, 0xf495); }
	for (unsigned i = 0; i < 64; ++i) { program.write_word(base + 0x9000 + i, 0xf495); program.write_word(base + target + i, 0xf495); program.write_word(target + i, 0xf495); }
	std::array<u16, 5> const code{0x771d, value, 0xf495, 0xf073, target};
	for (unsigned i = 0; i < code.size(); ++i) program.write_word(base + 0x9000 + i, code[i]);
	if (pop)
	{
		std::array<u16, 6> const pop_code{u16(popd ? 0x8b87 : 0x8a00 | destination), 0xf495, 0xf495, 0xf495, 0xf073, target};
		for (unsigned i = 0; i < pop_code.size(); ++i)
		{
			program.write_word(base + entry + i, pop_code[i]);
			if (boundary) program.write_word(entry + i, pop_code[i]);
		}
		if (boundary)
		{
			// Deliberate debugger-state conformance input, not a firmware fixture.
			// Only the bank not selected by the first Prefetch has a bad opcode.
			program.write_word(BIT(value ^ 0x20, 5) ? base + entry : entry, 0xffff);
		}
		m_cpu->space(AS_DATA).write_word(0x1000, value);
	}
	program.write_word(base + target, 0xe844); program.write_word(target, 0xe811);
	if (iptr)
	{
		u16 const offset = (16 + variant / 64) * 4;
		u16 const old_vector = ((value ^ 0x100) & 0xff80) | offset;
		u16 const new_vector = (value & 0xff80) | offset;
		for (unsigned i = 0; i < 32; ++i)
		{
			program.write_word(base + old_vector + i, 0xf495); program.write_word(old_vector + i, 0xf495);
			program.write_word(base + new_vector + i, 0xf495); program.write_word(new_vector + i, 0xf495);
		}
		program.write_word(base + old_vector, 0xe966); program.write_word(old_vector, 0xe966);
		program.write_word(base + new_vector, 0xe955); program.write_word(new_vector, 0xe955);
		for (unsigned line = 0; line < 16; ++line) m_cpu->set_input_line(line, CLEAR_LINE);
		m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, repeat ? 0 : value ^ (iptr ? 0x120 : 0x20));
	if (repeat)
	{
	set_registers(*m_cpu, {
		{cpu_regs::STATE_BRC, 0x1234}, {cpu_regs::STATE_RSA, 0x5678}, {cpu_regs::STATE_REA, 0x9abc},
	});
	}
	set_registers(*m_cpu, {
		{cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, iptr ? 0 : 0x800},
		{cpu_regs::STATE_SP, 0x1000},
	});
	if (popd) m_cpu->set_state_int(tms320c54x_device::STATE_AR7, 0x1d);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, iptr ? u16(1U << (variant / 64)) : 0},
	});
	if (iptr) m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(STATE_GENPC, base + entry); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_store_writes = m_pipeline_operand_reads = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(destination, destination, "staged_control_pop_publication",
		[this, value, pop](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == value && m_cpu->pipeline_cycles() == (pop ? 6 : 7) && !m_pipeline_store_writes,
				"STM PMST publishes its retained value once, separate from DAGEN early writes"); ++m_pipeline_store_writes;
		});
	m_pipeline_fetch_tap = (pop ? m_cpu->space(AS_DATA) : program).install_read_tap(pop ? 0x1000 : base + 0x9001, pop ? 0x1000 : base + 0x9001, "staged_pmst_source",
		[this, value](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == value && !m_pipeline_operand_reads, "STM PMST fetches its original literal exactly once"); ++m_pipeline_operand_reads;
		});
	m_phase = 252270; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_literal_guard(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u16 const destination = std::array<u16, 3>{0x7cf8, 0x7df8, 0x7ff8}[variant % 3];
	unsigned const count = (variant / 3) % 3;
	u32 const base = (variant / 9) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(base + 0x600 + i, 0xf495);
	std::array<u16, 6> const code{0xf072, 0x604, 0x76f8, destination, 0x1357, 0xf4e1};
	for (unsigned i = 0; i < code.size(); ++i) program.write_word(base + 0x600 + i, code[i]);
	m_cpu->space(AS_DATA).write_word(destination, 0xbeef);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_ST0, 0xa624}, {cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_BRC, count},
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	m_pipeline_store_writes = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "literal_guard_store",
		[this, destination, count](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == destination && word == 0x1357 && m_pipeline_store_writes <= count, "Opcode-shaped extension remains the ST destination through block repetition"); ++m_pipeline_store_writes; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = literal_guard_phase;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
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









void tms320c54x_test_state::start_pipeline_long_rmw(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const peripheral = profile_is("rmw-long-periph");
	bool const xorm = profile_is("xorm-absolute");
	if (!variant && !xorm)
	{
		m_long_rmw_coverage = {};
		m_long_rmw_replays = 0;
	}
	unsigned const operation = xorm ? 3 : variant % 3, arithmetic = (variant / (xorm ? 4 : peripheral ? 576 : 300)) % 64;
	bool const compatibility = (variant / (peripheral ? 288 : 19200)) % 2;
	auto const f = xorm ? absolute_xorm_fixture(variant) : peripheral ? peripheral_long_rmw_fixture(variant) : long_accumulator_store_fixture((variant / 3) % 100 + (compatibility ? 6400 : 0));
	u16 const initial_st0 = (f.st0 & ~0x0400) | (BIT(arithmetic, 2) << 10) | 0x0800;
	u16 const modified_st0 = compatibility && f.reg && f.kind != 3 ? (initial_st0 & 0x1fff) | (f.reg << 13) : initial_st0;
	auto const expected = long_rmw_expected(arithmetic, operation, modified_st0), input = addm_inputs(arithmetic);
	u32 const base = (peripheral ? bool((variant / 36864) % 2) : f.reg == 6) ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, std::array<u16, 4>{0x6b00, 0x6800, 0x6900, 0x6a00}[operation] | u8(f.opcode));
	program.write_word(base + 0x601, f.literal); program.write_word(base + 0x602, input[1]);
	if (xorm) program.write_word(base + 0x603, 0x1080 | f.selected);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_ST0, initial_st0},
		{cpu_regs::STATE_ST1, f.st1 | (BIT(arithmetic, 0) << 8) | (BIT(arithmetic, 1) << 9)},
	});
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, peripheral ? 0x5555 : 0x1007);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.selected, peripheral ? u16(f.address - f.literal) : f.initial);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_BK, f.bk}, {cpu_regs::STATE_A, 0x123456789aULL}, {cpu_regs::STATE_B, 0x2233445566ULL},
		{cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	m_cpu->space(AS_DATA).write_word(f.address, input[0]);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "long_rmw_source",
		[this, f, input](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == f.address && word == input[0] && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == (f.address >= 0x20 && f.address < 0x60 ? 8 : 7),
				"Long RMW retains one Read sample through its captured address, not the changed pointer"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "long_rmw_publication",
		[this, f, expected](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == f.address && word == expected[0] && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == (f.address >= 0x20 && f.address < 0x60 ? 9 : 8) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == expected[1], "Long RMW publishes its retained value and operation-owned flags once in Execute"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x602, "long_rmw_literals",
		[this, f, input](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(m_pipeline_extensions_reads < 2 && (at & 0xffff) == 0x601 + m_pipeline_extensions_reads &&
				word == (m_pipeline_extensions_reads ? input[1] : f.literal), "Long RMW retains the offset before the distinct arithmetic/mask literal"); ++m_pipeline_extensions_reads; });
	m_phase = 252890; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}




void tms320c54x_test_state::start_pipeline_rmw_forward(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const v = variant % 256;
	bool const copy = profile_is("mvkd-forward");
	bool const immediate = copy || profile_is("imm-forward");
	unsigned const consumer = immediate ? (variant / 768) % 3 : 0, form = immediate ? variant / 2304 : 0, extra = form == 2;
	u16 const literal = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[v & 3];
	u16 const initial = immediate ? 0x2468 : std::array<u16, 4>{0, 0x7fff, 0x8000, 0xffff}[v & 3];
	bool const sxm = BIT(v, 2), ovm = BIT(v, 3), compiler = BIT(v, 4), gap = BIT(v, 5), different = BIT(v, 7);
	u32 const base = BIT(v, 6) ? 0x20000 : 0;
	u16 const target = (compiler ? 0x1000 : 0x400) + 2;
	s32 const sum = (sxm ? s16(initial) : s32(initial)) + 1;
	u16 const result = immediate ? literal : ovm ? u16(std::clamp(sum, s32(-0x8000), s32(0x7fff))) : u16(sum);
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, copy ? form == 2 ? 0x70f8 : form == 1 ? 0x7092 : 0x7002 : immediate ? form == 2 ? 0x76f8 : form == 1 ? 0x7692 : 0x7602 : 0x6b02);
	program.write_word(base + 0x601, form == 2 ? target : copy ? 0x11 : immediate ? literal : 1);
	if (form == 2) program.write_word(base + 0x602, copy ? 0x11 : literal);
	program.write_word(base + 0x602 + extra + gap, (consumer == 1 ? 0x0000 : consumer == 2 ? 0x1100 : 0x1000) | (different ? 3 : 2));
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_ST0, 8},
		{cpu_regs::STATE_ST1, 0x800 | (sxm ? 0x100 : 0) | (ovm ? 0x200 : 0) | (compiler ? 0x4000 : 0)},
		{cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x5678},
		{cpu_regs::STATE_AR2, target},
	});
	if (copy) m_cpu->set_state_int(tms320c54x_device::STATE_AR1, literal);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(target, initial); m_cpu->space(AS_DATA).write_word(target + 1, 0x1357);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = 0;
	if (copy) m_operand_data_tap = m_cpu->space(AS_DATA).install_read_tap(0x11, 0x11, "mvkd_forward_source",
		[this, literal, extra](offs_t, u16 &value, u16) { if (machine().side_effects_disabled()) return; expect(value == literal && !m_pipeline_register_writes++ && m_cpu->pipeline_cycles() == 6 + extra, "MVKD captures its mapped source once before forwarding or real destination publication"); });
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(target, target + 1, "rmw_forward_reads",
		[this, target, initial, result, gap, different, immediate, extra](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			if (!immediate && !m_pipeline_operand_reads) expect(address == target && value == initial && m_cpu->pipeline_cycles() == 6, "RMW captures one original RAM operand");
			else expect((gap || different) && address == target + different && value == (different ? 0x1357 : result) && m_cpu->pipeline_cycles() == 7 + extra + gap, "Only a nonforwarded follower performs a real memory read");
			++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(target, target, "rmw_forward_write",
		[this, result, extra](offs_t, u16 &value, u16) { if (machine().side_effects_disabled()) return;
			expect(value == result && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == 7 + extra, "Forwarding leaves the producer Write clock unchanged"); ++m_pipeline_store_writes; });
	m_phase = 253390; m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
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

bool tms320c54x_test_state::check_literal_guard_results()
{
	if (m_phase == literal_guard_phase)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, count = (variant / 3) % 3;
		unsigned const checkpoint = std::array<unsigned, 3>{4, 7, 12}[variant / 18];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Instruction admission must not classify a retained extension as an opcode");
		capture_pipeline_at(checkpoint, {&m_pipeline_store_writes},
			"Save a block with an opcode-shaped extension");
		if (tick == 64)
		{
			expect(m_pipeline_store_writes == count + 1 && m_cpu->state_int(tms320c54x_device::STATE_IDLE), "Literal-bearing block retires each complete ST exactly once");
			{ auto const disable = machine().disable_side_effects(); expect(m_cpu->space(AS_DATA).read_word(std::array<u16, 3>{0x7cf8, 0x7df8, 0x7ff8}[variant % 3]) == 0x1357, "Block literal names the committed RAM location"); }
			if (replay_pipeline_at(literal_guard_phase + 1, checkpoint,
				"Restore a retained extension without duplicate block stores")) return true;
			m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 53) { start_literal_guard(variant + 1); return true; }
			osd_printf_info("TMS320C54x opcode-shaped extensions: PASS cases=54 restore=54 literals=7cf8,7df8,7ff8 block_counts=0,1,2 pages=2 snapshots=3 instruction=ST_absolute writes=count+1 transfer_admission=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated opcode-shaped extensions complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == literal_guard_phase + 1) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = literal_guard_phase; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_pipeline_rmw_results()
{
	if (m_phase == 251110)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		unsigned const kind = (m_pipeline_rmw_case / 2) % 3;
		bool const absolute = kind == 1;
		bool const after_read = m_pipeline_rmw_case >= 6;
		u64 const read_cycle = absolute ? 7 : 6;
		u64 const execute_cycle = read_cycle + 1;
		u64 const isr_cycle = execute_cycle + 4;
		u64 const checkpoint = after_read ? read_cycle : absolute ? 4 : 3;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),
			"RMW frontend advances one clock without atomic fallback");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle >= isr_cycle ? 0x55 : 0),
			"Opcode-shaped RMW literals are data, not accumulator loads");
		expect(m_pipeline_store_writes == (cycle >= execute_cycle ? 1 : 0) &&
			m_cpu->space(AS_DATA).read_word(0x4a0) == 0xbeef,
			"RMW never commits before Execute or redirects to later DP/AR values");
		if (cycle == 2) m_cpu->set_input_line(0, ASSERT_LINE);
		if (cycle == read_cycle - 1)
		{
			if (kind == 2) expect(m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x421,
				"RMW post-increment belongs to Access");
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 9);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x4a0);
		}
		if (cycle == read_cycle)
		{
			// Bench mutation is not an emulated CPU bus transaction.
			auto const suppress = machine().disable_side_effects();
			m_cpu->space(AS_DATA).write_word(0x420, 0x3333);
		}
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE,
				"Save partial RMW Decode or pending RMW write after Read");
		}
		if (cycle == isr_cycle)
		{
			expect(m_vector_acks == 1 && m_pipeline_store_writes == 1 &&
				m_pipeline_operand_reads == (m_pipeline_replay && after_read ? 0 : 1) &&
				m_pipeline_extensions_reads == (absolute ? 2 : 1),
				"RMW replay preserves extensions and captured data without duplicate bus traffic");
			expect(m_cpu->space(AS_DATA).read_word(0xfff) == (absolute ? 0x603 : 0x602),
				"RMW continuation points after every owned extension word");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true;
				m_vector_acks = m_pipeline_store_writes = m_pipeline_operand_reads = 0;
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE && m_cpu->pipeline_cycles() == checkpoint,
					"Restore incomplete RMW assembly or its sampled Read operand");
				return restart_pipeline_observer(251120, checkpoint + 1);
			}
			m_pipeline_fetch_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
			m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_rmw_case < 11) { start_pipeline_rmw_case(m_pipeline_rmw_case + 1); return true; }
			osd_printf_info("TMS320C54x staged RMW: PASS cases=12 forms=DP,postincrement,absolute extension_restore=6 sampled_read_restore=6 duplicate_reads=0\n");
			start_pipeline_mask_rmw_case();
			return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 251120)
	{
		u64 const checkpoint = m_pipeline_rmw_case >= 6 ? ((m_pipeline_rmw_case / 2) % 3 == 1 ? 7 : 6) :
			(m_pipeline_rmw_case / 2) % 3 == 1 ? 4 : 3;
		expect(m_cpu->pipeline_cycles() == checkpoint, "Scheduler settlement preserves pending RMW state");
		m_cpu->resume(SUSPEND_REASON_DISABLE);
		m_phase = 251110;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 251130)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IMR) == (cycle < 8 ? 1 : 0),
			"Mask RMW changes IMR only when its retained operation retires");
		expect(m_pipeline_store_writes == (cycle < 8 ? 0 : 1) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle < 12 ? 0 : 0x55),
			"Accepted entry completes after the staged mask write without a second commit");
		if (cycle == 2) m_cpu->set_input_line(0, ASSERT_LINE);
		if (cycle == 7 && !m_pipeline_replay)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE,
				"Save sampled IMR operand before the hardware mask write");
		}
		if (cycle == 12)
		{
			// Controller conformance: remove INTM as a confound, then pulse
			// another physical IRQ while the firmware-owned IMR write holds.
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_input_line(0, CLEAR_LINE);
			m_cpu->set_input_line(0, ASSERT_LINE);
		}
		if (cycle == 18)
		{
			expect(m_vector_acks == 1 && m_pipeline_operand_reads == (m_pipeline_replay ? 0 : 1) &&
			m_cpu->state_int(tms320c54x_device::STATE_IFR) == 1,
				"New IRQ remains pending under IMR=0 despite INTM being clear");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true;
				m_vector_acks = m_pipeline_operand_reads = m_pipeline_store_writes = 0;
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE && m_cpu->pipeline_cycles() == 7,
					"Restore sampled mask RMW without rereading the MMR");
				return restart_pipeline_observer(251140, 8);
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
			m_cpu->set_pipeline_frontend(false);
			osd_printf_info("TMS320C54x staged mask RMW: PASS read=7 write=8 accepted_irq=1 subsequent_masked=1 pending_write_restore=1 duplicate_reads=0\n");
			start_pipeline_return_case();
			return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 251140)
	{
		expect(m_cpu->pipeline_cycles() == 7, "Scheduler settlement does not perform a pending mask write");
		m_cpu->resume(SUSPEND_REASON_DISABLE);
		m_phase = 251130;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 251150)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		bool const pending_irq = m_pipeline_return_case >= 12;
		bool const multiword = m_pipeline_return_case >= 8 && !pending_irq;
		bool const delayed = multiword || !(m_pipeline_return_case & 1);
		u64 const acceptance = delayed ? 12 : 10;
		u64 const checkpoint = pending_irq ? 11 : multiword ? (m_pipeline_return_case & 1 ? 9 : 8) : (m_pipeline_return_case & 4 ? 8 : 7);
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Fast return advances one frontend cycle");
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == ((cycle >= 5 && cycle < 8) || (pending_irq && cycle >= acceptance + 2) ? 0xfff : 0x1000), "Fast return and pending entry modify SP in their Access stages only");
		expect(bool(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x800) == ((cycle >= 6 && cycle < 10) || (pending_irq && cycle >= acceptance + 3)), "Fast return clears INTM before the next accepted interrupt sets it");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle < 6 ? 0 : cycle >= 13 ? 0x44 : multiword ? (cycle >= 12 ? 0x1234 : 0x11) : delayed && cycle >= 11 ? 0x22 : 0x11), "Fast return preserves or discards the two delay words");
		expect(m_cpu->state_int(tms320c54x_device::STATE_B) == (!multiword && delayed && cycle >= 12 ? 0x33 : 0), "Return delay words belong to complete instructions, not independent literal opcodes");
		if (cycle == 2) m_cpu->set_input_line(0, ASSERT_LINE);
		if (pending_irq && cycle == 9)
		{
			m_cpu->set_input_line(0, CLEAR_LINE);
			m_cpu->set_input_line(0, ASSERT_LINE);
		}
		// Input-line writes are scheduler-synchronized after this callback.
		if (pending_irq && cycle > 9)
		{
			if (m_cpu->state_int(tms320c54x_device::STATE_IFR) != (cycle < acceptance + 3 ? 1 : 0))
				osd_printf_info("Pending return IRQ mismatch: case=%u cycle=%llu ifr=%04x ack=%u\n", m_pipeline_return_case, (unsigned long long)cycle, unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)), m_vector_acks);
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == (cycle < acceptance + 3 ? 1 : 0), "Pending return IRQ remains latched until its vector Fetch");
			expect(m_vector_acks == (cycle < acceptance + 3 ? 1 : 2), "Return delay words finish before the second vector acknowledgment");
		}
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save fast return around Access");
		}
		if (cycle == (pending_irq ? acceptance + 4 : 13))
		{
			expect(m_vector_acks == (pending_irq ? 2 : 1) && !m_pipeline_operand_reads && m_pipeline_drained_fetches == 2, "Fast return uses RTN without reading stack memory and fetches its continuation once");
			if (pending_irq)
			{
				auto const suppress = machine().disable_side_effects();
				expect(m_cpu->space(AS_DATA).read_word(0xfff) == (delayed ? 0x604 : 0x602), "Second IRQ stacks the first not-decoded continuation after return delay retirement");
			}
			expect(!multiword || m_pipeline_extensions_reads == 1, "Restored delay instruction does not reread its retained literal");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true;
				m_vector_acks = 1;
				m_pipeline_drained_fetches = checkpoint >= 9 ? 2 : 1;
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE && m_cpu->pipeline_cycles() == checkpoint, "Restore fast-return redirect and delay ownership");
				return restart_pipeline_observer(251160, checkpoint + 1);
			}
			m_pipeline_operand_tap.remove();
			m_pipeline_fetch_tap.remove();
			if (multiword) m_pipeline_write_tap.remove();
			m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_return_case < 15) { start_pipeline_return_case(m_pipeline_return_case + 1); return true; }
			osd_printf_info("TMS320C54x staged fast returns: PASS cases=8 target_fetch=9 return_execute=10 delay_execute=11,12 resumed_execute=13 stack_reads=0 restore=8\n");
			osd_printf_info("TMS320C54x staged return multiword delay: PASS cases=4 partial_decode_restore=2 completed_decode_restore=2 delay_execute=12 resumed_execute=13\n");
			osd_printf_info("TMS320C54x staged pending return IRQ: PASS cases=4 request=9 vector_fetch=13,15 stack_write=14,16 continuation=0602,0604 pending_restore=4\n");
			start_pipeline_branch_case();
			return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 251160)
	{
		u64 const checkpoint = m_pipeline_return_case >= 12 ? 11 : m_pipeline_return_case >= 8 ? (m_pipeline_return_case & 1 ? 9 : 8) : (m_pipeline_return_case & 4 ? 8 : 7);
		expect(m_cpu->pipeline_cycles() == checkpoint, "Settlement retains the fast-return checkpoint");
		m_cpu->resume(SUSPEND_REASON_DISABLE);
		m_phase = 251150;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 251170)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		bool const pending_irq = m_pipeline_branch_case >= 8;
		u64 const checkpoint = pending_irq ? (m_pipeline_branch_case & 4 ? 8 : 6) : (m_pipeline_branch_case & 4 ? 5 : 3);
		bool const delayed = !(m_pipeline_branch_case & 1);
		u64 const acceptance = delayed ? 9 : 7;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Branch frontend advances without executing flushed words");
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == (pending_irq && cycle >= acceptance + 2 ? 0xfff : 0x1000) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (!pending_irq || cycle >= acceptance + 3 ? 0x800 : 0), "Only accepted IRQ entry changes SP and INTM around the branch");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle >= 10 ? 0x44 : delayed && cycle >= 8 ? 0x22 : 0), "Branch delay or flush precedes target Execute");
		expect(m_cpu->state_int(tms320c54x_device::STATE_B) == (pending_irq && cycle >= acceptance + 7 ? 0x55 : delayed && cycle >= 9 ? 0x33 : 0), "Branch delay words drain before the ISR executes");
		if (pending_irq && cycle == 5) m_cpu->set_input_line(0, ASSERT_LINE);
		if (pending_irq && cycle > 5)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == (cycle < acceptance + 3 ? 1 : 0), "Branch IRQ remains pending until vector Fetch");
			expect(m_vector_acks == (cycle < acceptance + 3 ? 0 : 1), "Branch IRQ vector is acknowledged once after the protected operation");
		}
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save branch partial Decode or redirected Prefetch");
		}
		if (cycle == (pending_irq ? acceptance + 7 : 10))
		{
			expect(m_pipeline_drained_fetches == 1 && m_pipeline_extensions_reads == 1, "Branch restoration duplicates neither literal nor target fetch");
			if (pending_irq)
				expect(m_cpu->space(AS_DATA).read_word(0xfff) == (delayed ? 0x703 : 0x701), "Branch IRQ stacks the first not-decoded target continuation");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true;
				m_pipeline_drained_fetches = checkpoint >= 6 ? 1 : 0;
				m_vector_acks = 0;
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE && m_cpu->pipeline_cycles() == checkpoint, "Restore branch target and flush/delay ownership");
				return restart_pipeline_observer(251180, checkpoint + 1);
			}
			m_pipeline_fetch_tap.remove(); m_pipeline_operand_tap.remove();
			m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_branch_case < 15) { start_pipeline_branch_case(m_pipeline_branch_case + 1); return true; }
			osd_printf_info("TMS320C54x staged branches: PASS cases=8 literal_fetch=3 target_fetch=6 delay_execute=8,9 target_execute=10 restore=8\n");
			osd_printf_info("TMS320C54x staged branch IRQ: PASS cases=8 request=5 vector_fetch=10,12 stack_write=11,13 continuation=0701,0703 pending_restore=8\n");
			start_pipeline_stm_case();
			return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 251180)
	{
		u64 const checkpoint = m_pipeline_branch_case >= 8 ? (m_pipeline_branch_case & 4 ? 8 : 6) : (m_pipeline_branch_case & 4 ? 5 : 3);
		expect(m_cpu->pipeline_cycles() == checkpoint, "Settlement does not advance branch Prefetch");
		m_cpu->resume(SUSPEND_REASON_DISABLE);
		m_phase = 251170;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 311000)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = delay_asm_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (f.st1() | (tick > f.read() ? f.raw : f.raw ^ 31)), "Delayed ASM publishes only its five bits at Execute");
		if (f.long_form() && tick == f.literal_clock()) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x603 + f.prefix(), 0xdead); }
		if (tick == f.read() - 1) { expect(m_cpu->state_int(tms320c54x_device::STATE_AR3) == (f.form == 2 ? 0x1003 : f.form == 3 ? 0xfff : 0x1002), "ASM source updates its selected pointer once in Access"); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x4444); }
		if (tick == f.read()) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x1002, 0xdead); }
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_register_writes, &m_pipeline_extensions_reads},
			"Save ASM source, control-delay words and adjacent shift across publication");
		if (tick == 24)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + f.store() + 2 && m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_register_writes == f.prefix() && m_pipeline_extensions_reads == unsigned(f.long_form()) && m_cpu->state_int(tms320c54x_device::STATE_A) == f.result && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.pattern ? 0xfffffffff9ULL : 7) && m_cpu->state_int(tms320c54x_device::STATE_TRN) == u16(f.result) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1002 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa620 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x4444, "ASM delay and selected target preserve the sampled shift, source, status and one observed result");
			if (replay_pipeline_at(311010, f.checkpoint,
				"Replay delayed ASM without repeated source reads or target writes")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 30719) { start_delay_asm(variant + 1); return true; }
			osd_printf_info("TMS320C54x delayed LD ASM: PASS cases=30720 restore=30720 controls=BD,BCD_taken,BCD_untaken,BD_store_prefix forms=DP,SP,postincrement,offset,absolute shifts=32 patterns=2 sxm_ovm=4 pages=2 snapshots=3 source_reads=1 extension_fetches=owned t_preserved=1 adjacent_consumer=LD_B_ASM_A target_observes_result=1 call_interrupt_repeat_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed LD ASM complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 311010) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 311000; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}








bool tms320c54x_test_state::check_ram_forwarding_results()
{
	if (m_phase == 253390)
	{
		unsigned const variant = m_pipeline_peripheral_case, v = variant % 256, tick = ++m_idle_fixture_tick;
		bool const copy = profile_is("mvkd-forward");
		bool const immediate = copy || profile_is("imm-forward");
		unsigned const consumer = immediate ? (variant / 768) % 3 : 0, form = immediate ? variant / 2304 : 0, extra = form == 2;
		unsigned const checkpoint = 5 + (variant / 256) % 3 + extra;
		bool const sxm = BIT(v, 2), ovm = BIT(v, 3), compiler = BIT(v, 4), gap = BIT(v, 5), different = BIT(v, 7);
		u16 const literal = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[v & 3];
		u16 const initial = immediate ? 0x2468 : std::array<u16, 4>{0, 0x7fff, 0x8000, 0xffff}[v & 3];
		s32 const sum = (sxm ? s16(initial) : s32(initial)) + 1;
		u16 const result = immediate ? literal : ovm ? u16(std::clamp(sum, s32(-0x8000), s32(0x7fff))) : u16(sum), loaded = different ? 0x1357 : result;
		u64 const a = sxm ? u64(s64(s16(loaded))) & 0xffffffffffULL : loaded;
		u64 const added = (0x1234 + a) & 0xffffffffffULL;
		u16 const st0 = immediate ? 8 | (consumer == 1 && u64(0x1234) + u32(a) > 0xffffffffULL ? 0x800 : 0) : 8 | (sum < -0x8000 || sum > 0x7fff ? 0x400 : 0) |
			(u64(u32(sxm ? s16(initial) : s32(initial))) + 1 > 0xffffffffULL ? 0x800 : 0);
		bool const retired = tick >= 8 + extra + gap;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_A) == (retired && consumer != 2 ? consumer == 1 ? added : a : 0x1234) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (retired && consumer == 2 ? a : 0x5678) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == ((immediate ? retired : tick >= 7) ? st0 : 8), "Adjacent RAM consumer sees the pending producer without early accumulator or flag retirement");
		if (immediate) expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) == (compiler ? 0x1002 : 0x402) + unsigned(form == 1 && tick >= (copy ? 5 : 6)) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (sxm ? 0x100 : 0) | (ovm ? 0x200 : 0) | (compiler ? 0x4000 : 0)), "Immediate store updates its pointer once and preserves addressing/arithmetic modes");
		if (copy && tick == 6 + extra) m_cpu->set_state_int(tms320c54x_device::STATE_AR1, literal ^ 0x5a5a);
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save dependency before Read, before Write or after forwarding");
			if (immediate)
				// A forwarded value must use the captured literal, not program RAM.
				m_cpu->space(AS_PROGRAM).write_word((BIT(v, 6) ? 0x20000 : 0) + 0x601 + extra, (copy ? 0x11 : literal) ^ 0x5a5a);
		}
		if (tick == 12 + extra)
		{
			expect(m_pipeline_operand_reads == unsigned(!immediate) + unsigned(bool(gap || different)) && m_pipeline_store_writes == 1, "Internal-bus bypass avoids stale RAM reads and duplicate writes");
			if (copy) expect(m_pipeline_register_writes == 1 && m_cpu->state_int(tms320c54x_device::STATE_AR1) == (literal ^ 0x5a5a), "MVKD forwarding uses its retained source, not the changed mapped register");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2];
				return restore_pipeline_at(253400, "Restore dependency with its retained producer operand and follower");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); if (copy) m_operand_data_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (immediate ? 6911 : 767)) { start_pipeline_rmw_forward(variant + 1); return true; }
			if (copy)
			{
				osd_printf_info("TMS320C54x staged MVKD forwarding: PASS cases=6912 restore=6912 forms=direct,postincrement,absolute consumers=LD_A,ADD_A,LD_B same_and_different=1 gap_words=0,1 sxm_ovm=4 pages=2 cpl=2 source_reads=1 writes_each=1 retained_source=1 silicon_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated MVKD forwarding complete");
			}
			if (immediate)
			{
				osd_printf_info("TMS320C54x staged immediate forwarding: PASS cases=6912 restore=6912 forms=direct,postincrement,absolute consumers=LD_A,ADD_A,LD_B same_and_different=1 gap_words=0,1 sxm_ovm=4 pages=2 cpl=2 writes_each=1 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated immediate forwarding complete");
			}
			osd_printf_info("TMS320C54x staged RMW forwarding: PASS cases=768 restore=768 same_and_different=1 gap_words=0,1 flags=4 pages=2 cpl=2 silicon_phase_claim=0\n");
			if (profile_is("rmw-forward")) throw emu_fatalerror(0, "TMS320C54x isolated RMW forwarding complete");
			start_pipeline_mapped_word_arithmetic(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253400)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253390; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

} // namespace c54x_test
