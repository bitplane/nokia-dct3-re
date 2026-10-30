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








void tms320c54x_test_state::start_pipeline_explicit_sp(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const kind = variant % 5, gap = (variant / 5) % 4;
	bool const store = BIT(variant / 20, 0);
	u32 const base = BIT(variant / 40, 0) ? 0x20000 : 0;
	unsigned const words = kind == 1 || kind == 2 || kind == 4 ? 2 : 1;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	if (kind == 0) program.write_word(base + 0x600, 0x8a18);
	if (kind == 1) { program.write_word(base + 0x600, 0x7718); program.write_word(base + 0x601, 0x1020); }
	if (kind == 2) { program.write_word(base + 0x600, 0x7182); program.write_word(base + 0x601, 0x0018); }
	if (kind == 3) program.write_word(base + 0x600, 0xee20);
	if (kind == 4) { program.write_word(base + 0x600, 0x8081); program.write_word(base + 0x601, 0x8a18); }
	program.write_word(base + 0x600 + words + gap, store ? 0x8020 : 0x1020);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0xa600}, {cpu_regs::STATE_ST1, 0x4800}, {cpu_regs::STATE_SP, 0x1000},
		{cpu_regs::STATE_A, 0x7777}, {cpu_regs::STATE_B, 0x2233445566ULL}, {cpu_regs::STATE_AR1, 0x11},
		{cpu_regs::STATE_AR2, 0x70}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	auto &data = m_cpu->space(AS_DATA);
	data.write_word(0x70, 0x1020); data.write_word(0x1000, 0x1020);
	data.write_word(0x1020, 0x1111); data.write_word(0x1040, 0x2222);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = 0;
	unsigned const access = 4 + words + gap;
	bool const new_sp = gap >= (kind == 0 ? 2 : kind == 4 ? 3 : 1);
	u16 const address = new_sp ? 0x1040 : 0x1020;
	m_pipeline_operand_tap = data.install_read_tap(0x70, 0x1040, "explicit_sp_sources",
		[this, kind, store, access, address, new_sp](offs_t at, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			bool const source = at == (kind == 2 ? 0x70 : 0x1000);
			expect(source ? (kind == 0 || kind == 2 || kind == 4) && value == 0x1020 && m_cpu->pipeline_cycles() == (kind == 4 ? 6 : 5) :
				!store && at == address && value == (new_sp ? 0x2222 : 0x1111) && m_cpu->pipeline_cycles() == access + 1,
				"Explicit SP source and basic compiler operand use their documented separate visibility");
			++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = data.install_write_tap(0x18, 0x1040, "explicit_sp_publications",
		[this, kind, store, access, address](offs_t at, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			if (at == 0x18)
			{
				expect(kind != 3 && value == 0x1020 && m_cpu->pipeline_cycles() == (kind == 4 ? 7 : 5), "Explicit SP publishes once; only DAGEN conflict defers POPM");
				++m_pipeline_register_writes;
			}
			else
			{
				expect(store && at == address && value == 0x7777 && m_cpu->pipeline_cycles() == access + 2, "Compiler store retains its old or new SP-based address");
				++m_pipeline_store_writes;
			}
		});
	m_phase = 252190; m_cpu->resume(SUSPEND_REASON_DISABLE);
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

void tms320c54x_test_state::start_pipeline_sp_rmw(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u16 const initial = std::array<u16, 4>{0x1200, 0xfffe, 0, 0x8000}[variant % 4];
	u16 const delta = std::array<u16, 4>{0x3ff, 1, 0xffff, 0x8000}[(variant / 4) % 4];
	u16 const sum = initial + delta;
	u32 const base = BIT(variant, 5) ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 40; ++i) program.write_word(base + 0x600 + i, 0xf495);
	std::array<u16, 8> const words{0x7718, initial, 0x6bf8, 0x18, delta, 0x68f8, 0x18, 0xfffe};
	for (unsigned i = 0; i < words.size(); ++i) program.write_word(base + 0x600 + i, words[i]);
	program.write_word(base + 0x60b, 0x4918);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0xa608},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | (BIT(variant, 4) ? 0x100 : 0));
	set_registers(*m_cpu, {
		{cpu_regs::STATE_SP, 0x4000}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x18, 0x18, "sp_rmw_sources",
		[this, initial, sum](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			std::array<u16, 3> const values{initial, sum, u16(sum & 0xfffe)};
			std::array<unsigned, 3> const cycles{9, 12, 16};
			expect(m_pipeline_operand_reads < 3 && value == values[m_pipeline_operand_reads] && m_cpu->pipeline_cycles() == cycles[m_pipeline_operand_reads],
				"Original SP setup reads the published STM value, then ADDM result, then aligned SP once each"); ++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x18, 0x18, "sp_rmw_publications",
		[this, initial, sum](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			std::array<u16, 3> const values{initial, sum, u16(sum & 0xfffe)};
			std::array<unsigned, 3> const cycles{5, 10, 13};
			expect(m_pipeline_store_writes < 3 && value == values[m_pipeline_store_writes] && m_cpu->pipeline_cycles() == cycles[m_pipeline_store_writes],
				"STM, absolute ADDM and ANDM own distinct SP publications with unsigned 16-bit wrap"); ++m_pipeline_store_writes;
		});
	m_phase = 252330; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_arp(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const arp = variant % 8, encoded = BIT(variant, 3) ? 2 : 0;
	bool const compatibility = BIT(variant, 4), advance = BIT(variant, 5);
	unsigned const selected = compatibility && !encoded ? arp : encoded;
	u32 const base = BIT(variant, 6) ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0xf4a0 | arp);
	program.write_word(base + 0x601, 0x1000 | (advance ? 0x90 : 0x80) | encoded);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, 0x800 | (compatibility ? 0x20 : 0)},
		{cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_A, 0}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	for (unsigned i = 0; i < 8; ++i)
	{
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x420 + i * 0x10);
		m_cpu->space(AS_DATA).write_word(0x420 + i * 0x10, 0x1100 + i);
	}
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x420, 0x490, "arp_selected_source",
		[this, selected](offs_t at, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(!m_pipeline_operand_reads && at == 0x420 + selected * 0x10 && word == 0x1100 + selected && m_cpu->pipeline_cycles() == 6,
				"Immediate ARP has zero latency to the following compatibility consumer; encoded registers still select themselves"); ++m_pipeline_operand_reads;
		});
	m_phase = 252350; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_read_a(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const ram = variant >= 192;
	unsigned const page = std::array<unsigned, 3>{0, 2, 0x7f}[variant % 3];
	bool const upper = (variant / 3) % 2, ovly = (variant / 6) % 2;
	unsigned const gap = (variant / 12) % 4;
	u32 const logical = (page << 16) | (upper ? 0x8700 : 0x700);
	u32 const physical = ovly && !upper ? logical & 0xffff : logical;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 48; ++i) program.write_word(0x19000 + i, 0xf495);
	std::array<u16, 4> const code{0xf000, 1, 0x7ef8, 0x11};
	for (unsigned i = 0; i < code.size(); ++i) program.write_word(0x19000 + i, code[i]);
	program.write_word(0x19004 + gap, 0x1081);
	if (ram)
	{
		program.write_word(0x19002, 0x7e92); program.write_word(0x19003, 0xf495);
		for (unsigned i = 4; i < 8; ++i) program.write_word(0x19000 + i, i == 5 ? 0x1083 : 0xf495);
	}
	program.write_word(physical, 0x440); program.write_word(physical + 1, 0x450);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, ovly ? 0x20 : 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_SP, 0x1000},
		{cpu_regs::STATE_AR1, 0x410}, {cpu_regs::STATE_AR2, 0x420}, {cpu_regs::STATE_AR3, 0x420},
		{cpu_regs::STATE_A, logical - 1}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	m_cpu->space(AS_DATA).write_word(0x410, 0x1111); m_cpu->space(AS_DATA).write_word(0x440, 0x4444);
	m_cpu->space(AS_DATA).write_word(0x420, 0xffff);
	m_cpu->set_state_int(STATE_GENPC, 0x19000); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_store_writes = m_pipeline_operand_reads = m_pipeline_drained_fetches = 0;
	m_pipeline_operand_tap = program.install_read_tap(physical, physical + 1, "read_a_accumulator_source",
		[this, physical, ram](offs_t at, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(at == physical && word == 0x440 && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == (ram ? 11 : 12),
				"READA samples the preceding ADD result and retains the full accumulator-selected program address through its held bus clocks"); ++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(ram ? 0x420 : 0x11, ram ? 0x420 : 0x11, "read_a_publication",
		[this, ram](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == 0x440 && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == (ram ? 12 : 13), "READA publishes its sampled word once, not an early DAGEN write"); ++m_pipeline_store_writes;
		});
	m_vector_prefetch_tap = m_cpu->space(AS_DATA).install_read_tap(0x410, 0x440, "read_a_ar1_consumer",
		[this, gap, ram](offs_t at, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(at == (ram ? 0x420 : gap >= 2 ? 0x440 : 0x410) && word == (ram ? 0x440 : gap >= 2 ? 0x4444 : 0x1111) && !m_pipeline_drained_fetches &&
				m_cpu->pipeline_cycles() == (ram ? 14 : 13 + gap), "READA's publication respects the younger indirect address and data Read ownership"); ++m_pipeline_drained_fetches;
		});
	m_phase = 252370; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_add_sp_context(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const copy = profile_is("mvkd-sp");
	unsigned const producer = variant % 4, gap = std::array<unsigned, 3>{0, 1, 3}[(variant / 4) % 3];
	u16 const initial_sp = BIT(variant / 12, 0) ? 0x9000 : 0x1000;
	bool const compiler = BIT(variant / 24, 0), b = BIT(variant / 96, 0);
	u32 const base = BIT(variant / 48, 0) ? 0x20000 : 0;
	u16 const operand = initial_sp + std::array<int, 4>{-4, -1, 1, -8}[producer];
	unsigned const read_tick = 6 + (producer == 3 ? 2 : 1) + gap;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 48; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, std::array<u16, 4>{0xeefc, 0x4a11, 0x8a11, 0x7718}[producer]);
	if (producer == 3) program.write_word(base + 0x601, operand);
	unsigned const add = 0x600 + (producer == 3 ? 2 : 1) + gap;
	program.write_word(base + add, copy ? 0x7092 : b ? 0x01f8 : 0x00f8); program.write_word(base + add + 1, 0x18);
	// A younger FRAME must not change the older instruction's sampled SP.
	program.write_word(base + add + 2, 0xee02);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0xa008}, {cpu_regs::STATE_ST1, 0x900 | (compiler ? 0x4000 : 0)},
		{cpu_regs::STATE_SP, initial_sp}, {cpu_regs::STATE_AR1, 0x1357},
	});
	if (copy) m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x1400);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_A, b ? 0x2233445566ULL : 0}, {cpu_regs::STATE_B, b ? 0 : 0x2233445566ULL},
		{cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	m_cpu->space(AS_DATA).write_word(initial_sp, 0xbeef);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x18, 0x18, "add_sp_context_operand",
		[this, operand, read_tick](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == operand && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == read_tick,
				"Mapped SP ADD reads the older stack update once before the younger FRAME"); ++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x60, 0xffff, "mapped_sp_context_writes",
		[this, copy, initial_sp, operand](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			if (copy && at == 0x1400) { expect(word == operand && !m_pipeline_extensions_reads, "MVKD retains one SP publication after an older stack update"); ++m_pipeline_extensions_reads; }
			else { expect(at == u16(initial_sp - 1) && word == 0x1357 && !m_pipeline_store_writes, "PSHM retains one ordered stack write"); ++m_pipeline_store_writes; } });
	m_pipeline_extensions_reads = 0;
	m_phase = 291000; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_add_sp_exclusion(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const copy = profile_is("mvkd-sp");
	bool const destination_edge = copy && variant >= 20;
	u16 const source = destination_edge ? 0x18 : std::array<u16, 10>{0, 1, 6, 7, 0x1a, 0x1b, 0x1c, 0x1d, 0x24, 0x58}[variant % 10];
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(0x600 + i, 0xf495);
	program.write_word(0x600, copy ? destination_edge ? 0x70f8 : variant >= 10 ? 0x7212 : 0x7092 : variant >= 10 ? 0x01f8 : 0x00f8);
	program.write_word(0x601, destination_edge ? std::array<u16, 5>{0, 0x18, 0x19, 0x1a, 0x1f}[variant - 20] : source);
	if (destination_edge) program.write_word(0x602, source);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0xa008}, {cpu_regs::STATE_ST1, 0x900}, {cpu_regs::STATE_SP, 0x1000},
	});
	if (copy) { m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x1400); m_cpu->space(AS_DATA).write_word(0x1400, 0xaaaa); }
	set_registers(*m_cpu, {
		{cpu_regs::STATE_A, 0x123456789aULL}, {cpu_regs::STATE_B, 0x2233445566ULL}, {cpu_regs::STATE_IFR, 0},
		{cpu_regs::STATE_IMR, 0},
	});
	m_pipeline_operand_reads = m_idle_fixture_tick = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(source, source, "add_sp_excluded_source",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_operand_reads; });
	m_cpu->set_state_int(STATE_GENPC, 0x600); m_cpu->set_pipeline_frontend(true);
	m_phase = 291020; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_direct_rmw(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const operation = variant % 3, offset = (variant / 3) % 2 ? 0x20 : 0;
	u16 const initial = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 6) % 4];
	u16 const immediate = (variant / 24) % 2 ? 0x00ff : 0xffff;
	bool const compiler = (variant / 48) % 2;
	u32 const base = (variant / 96) % 2 ? 0x20000 : 0;
	u16 const target = (compiler ? 0x1000 : 0x400) + offset;
	u16 const opcode = std::array<u16, 3>{0x6800, 0x6900, 0x6b00}[operation] | offset;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, opcode); program.write_word(base + 0x601, immediate);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, 0x900 | (compiler ? 0x4000 : 0)},
		{cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	m_cpu->space(AS_DATA).write_word(target, initial);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x400, 0x1040, "direct_rmw_source",
		[this, target, initial](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == target && word == initial && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == 6,
			"Direct RMW retains Decode CPL/DP and Access SP instead of live context at Read"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x400, 0x1040, "direct_rmw_destination",
		[this, target, initial, immediate, operation](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			u16 const expected = operation == 0 ? initial & immediate : operation == 1 ? initial | immediate : u16(initial + immediate);
			expect(at == target && word == expected && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == 7,
				"Direct RMW writes its owned address and sampled operand once after context/RAM changes"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601, "direct_rmw_immediate",
		[this, immediate](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(word == immediate && !m_pipeline_extensions_reads,
			"Direct RMW retains its immediate without refetch"); ++m_pipeline_extensions_reads; });
	m_phase = 252450; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_mmr_move(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	unsigned const source = variant % 9, destination = (variant / 9) % 9;
	u16 const value = std::array<u16, 4>{0, 1, 0x8000, 0xf495}[(variant / 81) % 4];
	u32 const base = (variant / 324) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0xe700 | (source << 4) | destination);
	if (destination < 8) program.write_word(base + 0x601, 0x1080 | destination);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0x608}, {cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_A, 0x1111},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	for (unsigned i = 0; i < 9; ++i) m_cpu->space(AS_DATA).write_word(0x10 + i, 0x1000 + i);
	m_cpu->space(AS_DATA).write_word(0x10 + source, value);
	if (value >= 0x60) m_cpu->space(AS_DATA).write_word(value, 0x55aa);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "mvmm_decode_read",
		[this, source, destination, value](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			if (m_cpu->pipeline_cycles() == 3)
			{
				expect(at == 0x10 + source && word == value && !m_pipeline_operand_reads, "MVMM reads its selected MMR once in Decode"); ++m_pipeline_operand_reads;
			}
			else
			{
				expect(destination < 8 && at == value && word == (value < 0x60 ? 0 : 0x55aa) && !m_pipeline_extensions_reads && m_cpu->pipeline_cycles() == 6,
					"An adjacent indirect consumer sees the Access-published AR with no gap"); ++m_pipeline_extensions_reads;
			}
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "mvmm_access_write",
		[this, destination, value](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == 0x10 + destination && word == value && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == 4,
				"MVMM writes its retained Decode sample once in Access"); ++m_pipeline_store_writes; });
	m_phase = 252690; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_long_move(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const compatibility = (variant / 4800) % 2;
	auto const f = long_accumulator_store_fixture((variant % 100) + (compatibility ? 6400 : 0));
	unsigned const destination_kind = (variant / 100) % 12;
	u16 const value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 1200) % 4];
	u16 const destination = destination_kind < 10 ? 0x10 + destination_kind : destination_kind == 10 ? 0xf495 : f.address;
	u32 const base = f.reg == 6 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0x7100 | u8(f.opcode));
	program.write_word(base + 0x601, f.literal); program.write_word(base + 0x602, destination);
	if (destination_kind < 8) program.write_word(base + 0x603, 0xf4a8 | destination_kind);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x1007);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.selected, f.initial);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_SP, 0x1200}, {cpu_regs::STATE_BK, f.bk}, {cpu_regs::STATE_A, 0x123456789aULL},
		{cpu_regs::STATE_B, 0x2233445566ULL}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	m_cpu->space(AS_DATA).write_word(f.address, value);
	if (destination_kind == 10) m_cpu->space(AS_DATA).write_word(destination, 0xdead);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "long_mvdk_source",
		[this, f, value](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == f.address && word == value && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == 6,
				"Long MVDK samples its addressed source once before aliasing destination publication"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "long_mvdk_destination",
		[this, destination, value](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == destination && word == value && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == (destination < 0x60 ? 6 : 7),
				"Long MVDK writes once at its DAGEN/RAM publication clock"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x602, "long_mvdk_literals",
		[this, f, destination](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(m_pipeline_extensions_reads < 2 && (at & 0xffff) == 0x601 + m_pipeline_extensions_reads &&
				word == (m_pipeline_extensions_reads ? destination : f.literal), "Long MVDK retains source offset before destination literal"); ++m_pipeline_extensions_reads; });
	m_phase = 252830; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_original_long_move(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u16 const value = variant % 2 ? 0x1000 : 0x1234;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(0x600 + i, 0xf495);
	std::array<u16, 6> const words{0x80e6, 0x0010, 0x71e6, 0x0011, 0x0012, 0xf4aa};
	for (unsigned i = 0; i < words.size(); ++i) program.write_word(0x600 + i, words[i]);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_ST0, 0x608}, {cpu_regs::STATE_ST1, 0x800},
		{cpu_regs::STATE_AR0, 0x1000}, {cpu_regs::STATE_AR2, 0x1000}, {cpu_regs::STATE_AR6, 0x1000},
		{cpu_regs::STATE_A, 0x33fa}, {cpu_regs::STATE_B, 0x1122}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	m_cpu->space(AS_DATA).write_word(0x1010, 0xdead); m_cpu->space(AS_DATA).write_word(0x1011, value);
	m_cpu->set_state_int(STATE_GENPC, 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "original_long_move_source",
		[this, value](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == 0x1011 && word == value && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == 8,
				"Original MVDK offset 0011 reads after the preceding STL offset 0010 write"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "original_long_move_writes",
		[this, value](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(m_pipeline_store_writes < 2 && (at == 0x1010 ? word == 0x33fa && m_cpu->pipeline_cycles() == 7 && !m_pipeline_store_writes :
				at == 0x12 && word == value && m_cpu->pipeline_cycles() == 8 && m_pipeline_store_writes == 1),
				"Original pair retires the STL and AR2 publication once at their owned clocks"); ++m_pipeline_store_writes; });
	m_phase = 252850; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_tc_branch(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const carry = profile_is("carry-branch");
	if (carry) variant = variant % 8 + 16 + ((variant / 8) % 3) * 32 + (variant / 24) * 96;
	u16 const flag = carry ? 0x0800 : 0x1000;
	bool const tc = variant % 2, delayed = (variant / 2) % 2, value = (variant / 4) % 2;
	unsigned const producer = (variant / 8) % 4, gap = (variant / 32) % 3;
	u32 const base = (variant / 96) % 2 ? 0x20000 : 0;
	bool const taken = value == tc;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); }
	program.write_word(base + 0x600, std::array<u16, 4>{0x9634, 0xf4aa, u16(value ? 0xf5bc : 0xf4bc), 0xf5aa}[producer]);
	if (carry) program.write_word(base + 0x600, value ? 0xf5bb : 0xf4bb);
	program.write_word(base + 0x601 + gap, (carry ? 0xf808 | (tc << 2) : 0xf820 | (tc << 4)) | (delayed << 9));
	program.write_word(base + 0x602 + gap, 0x700);
	program.write_word(base + 0x603 + gap, carry ? value ? 0xf4bb : 0xf5bb : value ? 0xf4bc : 0xf5bc); // Opposite flag after evaluation.
	program.write_word(base + 0x604 + gap, 0xe922);
	program.write_word(base + 0x605 + gap, 0xe811); program.write_word(base + 0x606 + gap, 0xf4e1);
	program.write_word(base + 0x700, 0xe855); program.write_word(base + 0x701, 0xf4e1);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0},
	});
	set_registers(*m_cpu, {
		{cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_ST0, 0xa608 | (value ? 0 : flag)}, {cpu_regs::STATE_ST1, 0x800},
		{cpu_regs::STATE_AR0, 0x1200},
		{cpu_regs::STATE_AR2, producer == 3 ? (value ? 0x11ff : 0x1200) : value ? 0x1200 : 0x1201},
		{cpu_regs::STATE_AR5, 0x1000},
	});
	set_registers(*m_cpu, {
		{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	m_cpu->space(AS_DATA).write_word(0x1000, value ? 0x0800 : 0);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_register_writes = 0;
	m_pipeline_operand_tap = program.install_read_tap(base + 0x700, base + 0x700, "tc_branch_target",
		[this, taken, delayed, gap](offs_t, u16 &, u16) { if (machine().side_effects_disabled()) return;
			expect(taken && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == (delayed ? 9 : 8) + gap,
				"TC branch target is fetched once through its retained decision and conditional bubble"); ++m_pipeline_operand_reads; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x602 + gap, base + 0x602 + gap, "tc_branch_literal",
		[this](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(word == 0x700 && !m_pipeline_extensions_reads, "TC branch retains its target literal once"); ++m_pipeline_extensions_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "tc_branch_bit_source",
		[this, producer, value](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(!producer && at == 0x1000 && word == (value ? 0x0800 : 0) && !m_pipeline_register_writes && m_cpu->pipeline_cycles() == 5,
				"Only the BIT producer has one memory sample before TC publication"); ++m_pipeline_register_writes; });
	m_operand_data_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "tc_branch_no_data_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "TC branch and its register producers do not write data memory"); });
	m_phase = 252870; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
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




bool tms320c54x_test_state::check_pipeline_sp_rmw_results()
{
	if (m_phase == 252330)
	{
		unsigned const variant = m_pipeline_peripheral_case;
		u16 const initial = std::array<u16, 4>{0x1200, 0xfffe, 0, 0x8000}[variant % 4];
		u16 const delta = std::array<u16, 4>{0x3ff, 1, 0xffff, 0x8000}[(variant / 4) % 4];
		u16 const aligned = u16(initial + delta) & 0xfffe;
		unsigned const tick = ++m_idle_fixture_tick;
		unsigned const checkpoint = std::array<unsigned, 4>{7, 9, 10, 12}[variant / 64];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Absolute SP ADDM and ANDM have retained RMW ownership");
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save SP setup before/after ADDM Read/write or ANDM Read");
		}
		if (tick == 22)
		{
			expect(m_pipeline_operand_reads == 3 && m_pipeline_store_writes == 3 && m_cpu->state_int(tms320c54x_device::STATE_SP) == aligned &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == aligned && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (BIT(variant, 4) ? 0x100 : 0)),
				"Original SP adjustment/alignment preserves status and publishes the wrapped aligned result without duplicate operations");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1];
				return restore_pipeline_at(252340, "Restore SP RMW state without repeating a read or publication");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 255) { start_pipeline_sp_rmw(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged SP RMW: PASS cases=256 initial_values=4 deltas=4 sxm_states=2 pages=2 restore=256 ordered_reads=1 ordered_writes=1\n");
			start_pipeline_arp(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252340)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252330;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252350)
	{
		unsigned const variant = m_pipeline_peripheral_case, arp = variant % 8, encoded = BIT(variant, 3) ? 2 : 0;
		bool const compatibility = BIT(variant, 4), advance = BIT(variant, 5);
		unsigned const selected = compatibility && !encoded ? arp : encoded;
		unsigned const tick = ++m_idle_fixture_tick;
		unsigned const checkpoint = 3 + variant / 128;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Staged immediate ARP is distinct from compare and status-bit operations");
		if (tick == 6) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x420 + selected * 0x10, 0x9999); }
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_pipeline_branch_case = m_pipeline_operand_reads;
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save ARP load, selected address, auxiliary update or retained Read");
		}
		if (tick == 10)
		{
			expect(m_pipeline_operand_reads == 1 && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1100 + selected &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0x608 | ((compatibility && encoded ? encoded : arp) << 13)) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (compatibility ? 0x20 : 0)),
				"ARP load preserves unrelated status and does not re-publish over a younger encoded indirect ARP update");
			for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == 0x420 + i * 0x10 + (advance && i == selected ? 1 : 0),
				"ARP selection modifies only its owned auxiliary register once");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_pipeline_branch_case;
				return restore_pipeline_at(252360, "Restore ARP consumer without retargeting or repeating its auxiliary update");
			}
			m_pipeline_operand_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 511) { start_pipeline_arp(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged immediate ARP: PASS cases=512 arp_values=8 encoded_modes=2 cmpt_states=2 postincrement_states=2 pages=2 restore=512 zero_latency=1 duplicate_reads=0 duplicate_auxiliary_updates=0\n");
			start_pipeline_read_a(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252360)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252350;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252370)
	{
		unsigned const variant = m_pipeline_peripheral_case;
		bool const ram = variant >= 192;
		unsigned const page = std::array<unsigned, 3>{0, 2, 0x7f}[variant % 3];
		bool const upper = (variant / 3) % 2, ovly = (variant / 6) % 2;
		unsigned const gap = (variant / 12) % 4;
		u32 const logical = (page << 16) | (upper ? 0x8700 : 0x700), physical = ovly && !upper ? logical & 0xffff : logical;
		unsigned const tick = ++m_idle_fixture_tick;
		unsigned const checkpoint = ram ? std::array<unsigned, 4>{5, 7, 9, 11}[(variant - 192) / 12] : std::array<unsigned, 4>{6, 8, 10, 12}[variant / 48];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "READA has retained accumulator-source and AR1-destination ownership");
		if (tick == (ram ? 8 : 9)) m_cpu->set_state_int(tms320c54x_device::STATE_A, logical + 1);
		if (tick == (ram ? 11 : 12)) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(physical, 0x450); }
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_drained_fetches};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save READA literal, held clock, captured A or sampled program word");
		}
		if (tick == 22)
		{
			expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_drained_fetches == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == (ram ? 0x410 : 0x440) && m_cpu->state_int(tms320c54x_device::STATE_A) == (ram ? 0x440 : gap >= 2 ? 0x4444 : 0x1111) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == (ram ? 0x421 : 0x420) &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800,
				"READA's sampled program word and exact younger address selection reproduce with no repeated bus access or register write");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_drained_fetches = m_recurring_irq_snapshot[2];
				return restore_pipeline_at(252380, "Restore READA source/wait/operand without retargeting the transfer");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_vector_prefetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant == 191) osd_printf_info("TMS320C54x staged absolute READA AR1: PASS cases=192 source_pages=3 source_regions=2 ovly_states=2 consumer_gaps=4 restore=192 retained_accumulator_source=1 duplicate_program_reads=0 duplicate_register_writes=0\n");
			if (variant < 239) { start_pipeline_read_a(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged short READA RAM: PASS cases=48 source_pages=3 source_regions=2 ovly_states=2 restore=48 retained_accumulator_source=1 postincrement_once=1 duplicate_program_reads=0 duplicate_data_writes=0\n");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252380)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252370;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_add_sp_context_results()
{
	if (m_phase == 291000)
	{
		unsigned const variant = m_pipeline_peripheral_case, producer = variant % 4;
		bool const copy = profile_is("mvkd-sp");
		unsigned const gap = std::array<unsigned, 3>{0, 1, 3}[(variant / 4) % 3];
		u16 const initial_sp = BIT(variant / 12, 0) ? 0x9000 : 0x1000;
		u16 const operand = initial_sp + std::array<int, 4>{-4, -1, 1, -8}[producer];
		bool const compiler = BIT(variant / 24, 0), b = BIT(variant / 96, 0);
		unsigned const read_tick = 6 + (producer == 3 ? 2 : 1) + gap;
		unsigned const tick = ++m_idle_fixture_tick, checkpoint = std::array<unsigned, 3>{4, read_tick, read_tick + 2}[variant / 192];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Absolute ADD accepts SP after an evidenced stack update");
		if (tick == read_tick + 2)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == u16(operand + 2), "Younger FRAME updates SP after the ADD samples its older value");
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x2468);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save SP producer, retained ADD input or younger FRAME state");
		}
		if (tick == 24)
		{
			u64 const expected = u64(s64(s16(operand))) & 0xffffffffffULL;
			expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == unsigned(producer == 1) &&
				m_cpu->state_int(b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == (copy ? 0 : expected) &&
				m_cpu->state_int(b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == 0x2233445566ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x2468 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == (producer == 2 ? 0xbeef : 0x1357) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa008 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x900 | (compiler ? 0x4000 : 0)),
				"Mapped SP ADD retains the producer value independently of CPL, younger FRAME and later SP replacement");
			if (copy) { auto const disable = machine().disable_side_effects(); expect(m_pipeline_extensions_reads == 1 && m_cpu->space(AS_DATA).read_word(0x1400) == operand && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1401, "Mapped SP copy retains the older stack value and modifies its destination pointer once"); }
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				return restore_pipeline_at(291010, "Restore mapped-SP operand without duplicate stack effects or reads");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 575) { start_pipeline_add_sp_context(variant + 1); return true; }
			if (copy) osd_printf_info("TMS320C54x staged MVKD SP contexts: PASS cases=576 restore=576 producers=FRAME,PSHM,POPM,STM gaps=0,1,3 cpl_states=2 pages=2 accumulators=2 younger_frame=1 retained_source=1 silicon_phase_claim=0\n");
			else osd_printf_info("TMS320C54x staged absolute ADD SP contexts: PASS cases=576 restore=576 producers=FRAME,PSHM,POPM,STM gaps=0,1,3 cpl_states=2 pages=2 accumulators=2 younger_frame=1 retained_source=1 silicon_phase_claim=0\n");
			start_pipeline_add_sp_exclusion(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 291010) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 291000; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 291020)
	{
		bool const copy = profile_is("mvkd-sp");
		if (++m_idle_fixture_tick == 12)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && !m_pipeline_operand_reads &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2233445566ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa008,
				"SP admission does not admit control, repeat, mapping or peripheral sources");
			if (copy) { auto const disable = machine().disable_side_effects(); expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1400 && m_cpu->space(AS_DATA).read_word(0x1400) == 0xaaaa, "Excluded MVKD and MVDM do not update the destination pointer or publish data"); }
			m_pipeline_operand_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < (copy ? 24 : 19)) { start_pipeline_add_sp_exclusion(m_pipeline_peripheral_case + 1); return true; }
			if (copy)
			{
				osd_printf_info("TMS320C54x staged MVKD SP exclusions: PASS cases=25 sources=10 source_cases=20 destination_cases=5 forms=MVKD,MVDM operand_reads=0 architectural_results=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated MVKD SP complete");
			}
			osd_printf_info("TMS320C54x staged absolute ADD SP exclusions: PASS cases=20 sources=10 accumulators=2 operand_reads=0 architectural_results=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated absolute ADD SP complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252450)
	{
		unsigned const variant = m_pipeline_peripheral_case, operation = variant % 3, offset = (variant / 3) % 2 ? 0x20 : 0;
		u16 const initial = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 6) % 4], immediate = (variant / 24) % 2 ? 0x00ff : 0xffff;
		bool const compiler = (variant / 48) % 2;
		u32 const base = (variant / 96) % 2 ? 0x20000 : 0;
		u16 const target = (compiler ? 0x1000 : 0x400) + offset;
		unsigned const tick = ++m_idle_fixture_tick, checkpoint = 4 + variant / 192;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Compiler and noncompiler direct RMW have explicit retained addressing");
		if (tick == 4)
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, m_cpu->state_int(tms320c54x_device::STATE_ST1) ^ 0x4000);
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0xdead);
		}
		if (tick == 5)
		{
			if (compiler) m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1020);
			else m_cpu->set_state_int(tms320c54x_device::STATE_ST0, (m_cpu->state_int(tms320c54x_device::STATE_ST0) & ~0x1ff) | 9);
		}
		if (tick == 6) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(target, 0x5555); }
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save direct RMW captured context, address, operand or completed write");
		}
		if (tick == 20)
		{
			{
				auto const disable = machine().disable_side_effects();
				u16 const expected = operation == 0 ? initial & immediate : operation == 1 ? initial | immediate : u16(initial + immediate);
				expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_extensions_reads == 1 && m_cpu->space(AS_DATA).read_word(target) == expected &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == (compiler ? 0x1020 : 0x1000) && (m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1ff) == (compiler ? 8 : 9) &&
					m_cpu->state_int(tms320c54x_device::STATE_ST1) == ((0x900 | (compiler ? 0x4000 : 0)) ^ 0x4000),
					"Direct RMW preserves subsequent context changes while using its original owned address and operands");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				return restore_pipeline_at(252460, "Restore direct RMW without resampling modified context, literal or memory");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 767) { start_pipeline_direct_rmw(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged direct RMW: PASS cases=768 operations=ANDM,ORM,ADDM offsets=2 values=4 immediates=2 cpl_states=2 pages=2 restore=768 retained_context=1 duplicate_reads=0 duplicate_writes=0\n");
			start_pipeline_peripheral_logic(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252460)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252450;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_long_move_results()
{
	if (m_phase == 252830)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, destination_kind = (variant / 100) % 12;
		bool const compatibility = (variant / 4800) % 2;
		auto const f = long_accumulator_store_fixture((variant % 100) + (compatibility ? 6400 : 0));
		u16 const value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 1200) % 4];
		u16 const destination = destination_kind < 10 ? 0x10 + destination_kind : destination_kind == 10 ? 0xf495 : f.address;
		unsigned const checkpoint = std::array<unsigned, 4>{3, 5, 6, 7}[variant / 9600];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Long-offset MVDK has staged ownership");
		if (tick == 5)
		{
			auto const disable = machine().disable_side_effects();
			u32 const base = f.reg == 6 ? 0x20000 : 0;
			m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0xdead); m_cpu->space(AS_PROGRAM).write_word(base + 0x602, 0xdead);
		}
		if (tick == 6)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, 0x7777);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save long MVDK during Decode or before/after publication");
		}
		if (tick == 16)
		{
			u16 st0 = compatibility && f.reg && f.kind != 3 ? (f.st0 & 0x1fff) | (f.reg << 13) : f.st0;
			if (destination_kind < 8)
			{
				u16 const ar0 = destination_kind == 0 ? value : f.selected == 0 ? f.next_ar : 0x1007;
				st0 = (st0 & ~0x1000) | (value == ar0 ? 0x1000 : 0);
			}
			for (unsigned ar = 0; ar < 8; ++ar)
				expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == (destination == 0x10 + ar ? value : ar == f.selected ? f.next_ar : 0x1007),
					"MVDK address modification precedes aliasing destination publication and is not replayed twice");
			{
				auto const disable = machine().disable_side_effects();
				expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_extensions_reads == 2 &&
					m_cpu->space(AS_DATA).read_word(destination) == value && m_cpu->state_int(tms320c54x_device::STATE_ST0) == st0 &&
					m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_SP) == (destination_kind == 8 ? value : 0x1200) &&
					m_cpu->state_int(tms320c54x_device::STATE_BK) == (destination_kind == 9 ? value : f.bk) &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2233445566ULL,
					"MVDK preserves unsigned values and exposes early AR publication to the immediate CMPR consumer");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				return restore_pipeline_at(252840, "Restore long MVDK without source resampling or duplicate writes");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 38399) { start_pipeline_long_move(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged long MVDK: PASS cases=38400 forms=25 offsets=4 destinations=12 values=4 compatibility=2 restore=38400 source_alias=1 zero_gap_cmpr=1 duplicate_reads=0 duplicate_writes=0\n");
			start_pipeline_original_long_move(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252840)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252830;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252850)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		u16 const value = variant % 2 ? 0x1000 : 0x1234;
		unsigned const checkpoint = std::array<unsigned, 4>{4, 7, 8, 9}[variant / 2];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Original STL/MVDK pair has coherent staged ownership");
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save original STL/MVDK pair across its two retirement boundaries");
		}
		if (tick == 16)
		{
			expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 2 && m_cpu->state_int(tms320c54x_device::STATE_AR2) == value &&
				m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0x608 | (variant % 2 ? 0x1000 : 0)) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x33fa && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1122,
				"Original pair retires both writes and the immediate AR2 consumer without changing the pointer or accumulator");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1];
				return restore_pipeline_at(252860, "Restore original pair without repeating either write");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 7) { start_pipeline_original_long_move(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged original long MVDK pair: PASS cases=8 restore=8 stl_write=7 source_read=8 ar2_write=8 zero_gap_cmpr=1 duplicate_writes=0\n");
			if (profile_is("long-mvkd")) { start_pipeline_long_rmw(); return true; }
			start_pipeline_tc_branch(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252860)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252850;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252870)
	{
		unsigned const raw_variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const carry = profile_is("carry-branch");
		unsigned const variant = carry ? raw_variant % 8 + 16 + ((raw_variant / 8) % 3) * 32 + (raw_variant / 24) * 96 : raw_variant;
		u16 const flag = carry ? 0x0800 : 0x1000;
		bool const tc = variant % 2, delayed = (variant / 2) % 2, value = (variant / 4) % 2, poison = variant / 768;
		unsigned const producer = (variant / 8) % 4, gap = (variant / 32) % 3;
		bool const taken = value == tc;
		unsigned const checkpoint = std::array<unsigned, 4>{4, 6, 7, 10}[(variant / 192) % 4] + gap;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "TC/NTC conditional branches have staged ownership");
		if (poison && tick == 7 + gap)
			// CPU conformance mutation after Evaluate, never a native-boot hook.
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, m_cpu->state_int(tms320c54x_device::STATE_ST0) ^ flag);
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads, m_pipeline_register_writes};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save TC branch before/after evaluation and delay execution");
		}
		if (tick == 32)
		{
			bool const final_tc = delayed || !taken || poison ? !value : value;
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == unsigned(taken) &&
				m_pipeline_extensions_reads == 1 && m_pipeline_register_writes == unsigned(!producer) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == (taken ? 0x55 : 0x11) &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == (delayed || !taken ? 0x22 : 0) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xa608 | (final_tc ? flag : 0)) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800 && m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x1000,
				"TC branch consumes the published condition once; delay writes and later TC mutation cannot change the retained redirect");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2];
				return restore_pipeline_at(252880, "Restore TC decision/held word without duplicate target or source fetches");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_operand_data_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (raw_variant < (carry ? 383 : 1535)) { start_pipeline_tc_branch(raw_variant + 1); return true; }
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			if (carry)
			{
				osd_printf_info("TMS320C54x staged carry branches: PASS cases=384 forms=BC,BCD conditions=C,NC producer=status_bit gaps=0,1,2 pages=2 restore=384 post_evaluate_change=0,1 duplicate_fetches=0 arithmetic_producers=unvalidated\n");
				throw emu_fatalerror(0, "TMS320C54x isolated carry branches complete");
			}
			osd_printf_info("TMS320C54x staged TC branches: PASS cases=1536 forms=BC,BCD conditions=TC,NTC producers=BIT,CMPR_EQ,status_bit,CMPR_LT gaps=0,1,2 pages=2 restore=1536 post_evaluate_change=0,1 duplicate_fetches=0\n");
			if (profile_is("tc-branch")) throw emu_fatalerror(0, "TMS320C54x isolated TC branches complete");
			start_pipeline_long_rmw(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252880)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252870;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
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
