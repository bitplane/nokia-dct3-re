// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

void tms320c54x_test_state::start_idle_nmi_case(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	u16 const idle[] = {0xf4e1, 0xf6e1, 0xf5e1};
	program.write_word(0x010980, idle[index / 4]);
	program.write_word(0x010981, 0xf4e1);
	program.write_word(0x010004, 0xf4e1); // NMI ISR stops without changing masks.
	m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, index & 1 ? 0xffff : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, BIT(index, 1) ? 0x0800 : 0);
	m_cpu->set_state_int(STATE_GENPC, 0x010980);
	m_cpu->space(AS_DATA).write_word(0x0fff, 0x5a5a);
	m_phase = 7100 + index * 5;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::check_idle_nmi_result(unsigned index)
{
	expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
		m_cpu->state_int(STATE_GENPC) == 0x010005 &&
		m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff &&
		m_cpu->space(AS_DATA).read_word(0x0fff) == 0x0981 &&
		m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x8008 &&
		m_cpu->state_int(tms320c54x_device::STATE_IMR) == (index & 1 ? 0xffff : 0) &&
		m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x0800 &&
		!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),
		"NMI wakes IDLE1/2/3 despite INTM and IMR, uses vector 1, preserves IFR and stacks continuation once");
}

void tms320c54x_test_state::start_idle_dma_case(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	u16 const idle[] = {0xf4e1, 0xf6e1, 0xf5e1};
	program.write_word(0x010960, idle[std::min(index / 2, 2U)]);
	program.write_word(0x010961, 0x76f8);
	program.write_word(0x010962, 0x0501);
	program.write_word(0x010963, 1);
	program.write_word(0x010964, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_idle_mcbsp_external = false;
	m_idle_mcbsp->reset();
	m_idle_dma->reset();
	m_idle_dma->set_input_clock(index >= 6 ? 0 : 3'250'000);
	m_idle_dma_rate = index / 2 == 2 ? 3'250'000 : m_idle_dma->clock();
	m_idle_dma_first_ticks = index >= 8 ? 2 : 4;
	m_idle_dma_completions = 0;
	u16 const registers[] = {0x0520, 0x0530, 3, 0, 0x4145}; // Data/data, increment both, block IRQ.
	m_idle_dma->write(1, 0);
	for (u16 value : registers) m_idle_dma->write(2, value);
	for (unsigned i = 0; i < 4; ++i)
	{
		data.write_word(0x0520 + i, 0x1111 * (i + 1));
		data.write_word(0x0530 + i, 0);
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, index & 1 ? 0x40 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0800);
	m_cpu->set_state_int(STATE_GENPC, 0x010960);
	data.write_word(0x0501, 0);
	m_phase = 6280 + index * 2;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_idle_external_serial_case(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x010940, index < 2 ? 0xf6e1 : 0xf5e1);
	program.write_word(0x010941, 0x76f8);
	program.write_word(0x010942, 0x0501);
	program.write_word(0x010943, 1);
	program.write_word(0x010944, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_idle_mcbsp_external = false;
	m_idle_mcbsp->reset();
	m_idle_mcbsp_words = 0;
	for (auto const &[reg, value] : std::array<std::pair<u16, u16>, 3>{{{4, 0x0040}, {5, 1}, {1, 1}}})
	{
		m_idle_mcbsp->control_w(0, reg);
		m_idle_mcbsp->control_w(1, value);
	}
	m_idle_mcbsp->data_w(3, 0xa55a);
	m_idle_mcbsp_external = true;
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, index & 1 ? 0x20 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0800);
	m_cpu->set_state_int(STATE_GENPC, 0x010940);
	m_cpu->space(AS_DATA).write_word(0x0501, 0);
	m_phase = 6260 + index * 2;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_idle_timer_case(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x010920, index < 2 ? 0xf6e1 : 0xf5e1); // IDLE2 / IDLE3 (NN=10/01).
	program.write_word(0x010921, 0x76f8);
	program.write_word(0x010922, 0x0501);
	program.write_word(0x010923, 1);
	program.write_word(0x010924, 0xf4e1); // Timer runs again in IDLE1.
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PRD, 99);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x23);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, index & 1 ? 1 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0800);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(STATE_GENPC, 0x010920);
	m_cpu->space(AS_DATA).write_word(0x0501, 0);
	m_idle_mcbsp->reset();
	m_idle_mcbsp_words = 0;
	for (auto const &[reg, value] : std::array<std::pair<u16, u16>, 5>{{
		{4, 0x0040}, {6, 12}, {7, 0x2000}, {14, 0x0200}, {1, 0x0041}}})
	{
		m_idle_mcbsp->control_w(0, reg);
		m_idle_mcbsp->control_w(1, value);
	}
	m_idle_mcbsp->data_w(3, 0xa55a);
	m_phase = 6220 + index * 3;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_idle_wake_case(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x010900, 0xf4e1); // IDLE1
	program.write_word(0x010901, 0x76f8); // Mark foreground continuation.
	program.write_word(0x010902, 0x0501);
	program.write_word(0x010903, 1);
	program.write_word(0x010904, 0x7726); // Stop the periodic source after continuation.
	program.write_word(0x010905, 0x0010);
	program.write_word(0x010906, 0xf4e1);
	program.write_word(0x010040, 0xf4e1); // External IRQ0 vector, without return.
	m_cpu->space(AS_DATA).write_word(0x0501, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, index >= 2 ? 0x0800 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, (index & 1) ? (index >= 4 ? 8 : 1) : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(STATE_GENPC, 0x010900);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 6200 + index * 2;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_stopped_timer_reload_case(unsigned index)
{
	unsigned const divider = index % 16;
	unsigned const variant = index / 16;
	auto &program = m_cpu->space(AS_PROGRAM);
	u16 const code[] = {0x7725, 0x1234, 0x7726, u16(0x0030 | divider),
		u16(variant ? 0x6926 : 0xf495), u16(variant ? (variant == 1 ? 0 : 0x03c0) : 0xf495),
		0x4826, 0xf4e1}; // Reload only, identity ORM, or attempted write to read-only PSC.
	for (unsigned i = 0; i < std::size(code); ++i)
		program.write_word(0x010600 + i, code[i]);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0800);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(STATE_GENPC, 0x010600);
	m_phase = 7300 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_timer_prescaler_divider_case(unsigned divider)
{
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PRD, 2);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x30 | divider);
	// TDDR changes the next reload, not the current read-only PSC.
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 1);
	m_phase = 7350 + divider;
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_timer_counter_write_case(unsigned divider)
{
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PRD, 2);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x20 | divider);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 1);
	m_phase = 7700 + divider;
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::check_timer_reserved_fields()
{
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PRD, 9);
	for (unsigned control = 0; control < 4; ++control)
		for (unsigned reserved = 0; reserved < 16; ++reserved)
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x33);
			m_cpu->set_state_int(tms320c54x_device::STATE_TCR,
					(reserved << 12) | (control << 10) | 0x13);
			u16 const actual = m_cpu->state_int(tms320c54x_device::STATE_TCR);
			osd_printf_info("TMS320C54x timer reserved fields: reserved=%u control=%u tcr=%04x\n",
				reserved, control, actual);
			expect(actual == ((control << 10) | 0xd3) &&
				m_cpu->state_int(tms320c54x_device::STATE_TIM) == 9,
				"SPRU131G TCR reserved bits read zero while Free/Soft storage and stopped PSC are preserved");
		}
	osd_printf_info("TMS320C54x timer reserved fields: PASS cases=64 debugger_free_soft_behavior_claim=0\n");
}

void tms320c54x_test_state::start_clkmd_write_case(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	u16 const initial = u16((index << 12) | 0x0494);
	u16 const replacement = u16((~initial & 0xfff8) | 4);
	u16 pc = 0x0b00;
	auto write = [&](u16 value) { program.write_word(pc++, 0x7758); program.write_word(pc++, value); };
	auto capture = [&](u16 address) {
		program.write_word(pc++, 0x4858); // LDM CLKMD,A
		program.write_word(pc++, 0x80f8); program.write_word(pc++, address); // STL A,*(address)
	};
	write(0); // Leave PLL before programming any protected fields.
	write(initial | 2); capture(0x0501);
	write(replacement | 3); capture(0x0502); // PLLSTATUS is not writable.
	write(replacement); // Only PLLNDIV may change while still in PLL.
	write(replacement | 2); capture(0x0503); // Now in DIV: replacement is legal.
	program.write_word(pc, 0xf4e1);
	m_cpu->set_state_int(STATE_GENPC, 0x0b00);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 79000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_peripheral_mmr_timing_audit(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_mmr_timing_case = variant;
	u16 const target = std::array<u16, 4>{0x12, 0x25, 0x5f, 0x60}[variant % 4];
	unsigned const family = variant / 4;
	bool const read = family == 4;
	u16 const opcode = std::array<u16, 5>{0x7700, 0x6800, 0x6900, 0x7600, 0x1000}[family] | target;
	unsigned const words = read ? 1 : 2;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(0x600 + i, 0xf495);
	for (unsigned i = 0; i < 2; ++i)
	{
		program.write_word(0x600 + i * words, opcode);
		if (!read) program.write_word(0x601 + i * words, 0xff00);
	}
	program.write_word(0x600 + 2 * words, 0xf073); program.write_word(0x601 + 2 * words, 0x600 + 2 * words);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->space(AS_DATA).write_word(target, 0xff00);
	m_cpu->set_state_int(STATE_GENPC, 0x600);
	m_cpu->set_pipeline_frontend(false);
	m_pipeline_store_writes = 0;
	m_first_operand_cycle = m_last_operand_cycle = 0;
	auto const observe = [this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == 0xff00, "Timing audit reaches the actual register bus");
			if (!m_pipeline_store_writes++) m_first_operand_cycle = m_cpu->total_cycles();
			else m_last_operand_cycle = m_cpu->total_cycles();
		};
	m_pipeline_write_tap = read ? m_cpu->space(AS_DATA).install_read_tap(target, target, "peripheral_mmr_read_timing", observe) :
		m_cpu->space(AS_DATA).install_write_tap(target, target, "peripheral_mmr_write_timing", observe);
	m_phase = 251210;
	m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(24, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_peripheral_case(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	unsigned const family = variant % 4;
	u32 const base = variant & 8 ? 0x10000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, std::array<u16, 4>{0x1025, 0x8025, 0x6825, 0x68f8}[family]);
	if (family >= 2) program.write_word(base + 0x601, family == 3 ? 0x25 : 0xfffe);
	if (family == 3) program.write_word(base + 0x602, 0xfffe);
	m_cpu->set_input_line(0, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_PRD, family == 0 ? 0x1234 : family == 1 ? 0xaaaa : 0xffff);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x5678);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false;
	m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x25, 0x25, "staged_peripheral_read",
		[this, family](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(family != 1 && value == (family == 0 ? 0x1234 : 0xffff) &&
				m_cpu->pipeline_cycles() == (family == 0 ? 6 : family == 2 ? 7 : 8), "Peripheral operand is read once after its retained wait");
			++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x25, 0x25, "staged_peripheral_write",
		[this, family](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(family != 0 && value == (family == 1 ? 0x5678 : 0xfffe) &&
				m_cpu->pipeline_cycles() == (family == 1 ? 7 : family == 2 ? 8 : 9), "Peripheral write commits once without charging the RMW twice");
			++m_pipeline_store_writes;
		});
	m_phase = 251220;
	m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_mu4_timer_context(unsigned variant)
{
	bool const sxm = BIT(variant, 0), matched = BIT(variant, 1);
	unsigned const page = BIT(variant, 2) ? 3 : 0;
	start_pipeline_original_context_call(1 | (sxm ? 4 : 0) | (matched ? 8 : 0), true);
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove();
	m_vector_stack_tap.remove(); m_vector_prefetch_tap.remove();
	auto &program = m_cpu->space(AS_PROGRAM);
	// Original uploaded lower-32K code, with the observed warm OVLY mapping.
	for (unsigned i = 0; i < 144; ++i) program.write_word(0x3d73 + i, program.read_word(0x23d73 + i));
	std::array<u16, 4> const vector{0x4a1e, 0xf495, 0xf882, 0x3d73};
	for (unsigned i = 0; i < vector.size(); ++i) program.write_word(0x204c + i, vector[i]);
	program.write_word(0x4044, 0xf4e5); // Original FRETE, not a fixture continuation.
	u32 const foreground = (page << 16) | 0x9000;
	for (unsigned i = 0; i < 512; ++i) program.write_word(foreground + i, 0xf495);
	program.write_word(foreground + 0x80, 0xe966);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0x2020);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4200 | (sxm ? 0x100 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x77);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 8);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(STATE_GENPC, foreground); m_cpu->set_pipeline_frontend(true);
	m_cpu->set_state_int(tms320c54x_device::STATE_PRD, 0xffff);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x20);
	m_cpu->set_state_int(tms320c54x_device::STATE_TIM, 31);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_vector_acks = m_pipeline_store_writes = m_pipeline_extensions_reads = m_pipeline_drained_fetches = m_vector_reads = m_pipeline_operand_reads = m_fast_stack_reads = 0;
	m_timer_probe_return_pc = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0xfe0, 0xfff, "mu4_timer_original_stack",
		[this, page, sxm](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			std::array<u16, 18> const values{m_timer_probe_return_pc, u16(page), 0xa61b, u16(0x4a00 | (sxm ? 0x100 : 0)),
				0x77, 0, 0, 0x5566, 0x3344, 0x22, 0x1357, 0x2468, 0x369c, 0x8888,
				0x3d96, u16(0x800 | (sxm ? 0x100 : 0)), 0xff11, 0x4044};
			unsigned const index = m_pipeline_store_writes;
			unsigned const destination = index < 2 ? 0xfff - index : index < 17 ? 0xffe - index : 0xffd;
			if (index >= values.size() || address != destination || value != values[index])
				osd_printf_info("MU4 timer stack mismatch: index=%u cycle=%llu address=%04x value=%04x return=%04x\n",
					index, (unsigned long long)m_cpu->pipeline_cycles(), unsigned(address), value, m_timer_probe_return_pc);
			expect(index < values.size() && address == destination && value == values[index],
				"Hardware timer, original XPC vector and handler own a balanced ordered context stack");
			++m_pipeline_store_writes;
		});
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0xffe, 0xfff, "mu4_timer_frete_reads",
		[this, page](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_fast_stack_reads < 2 && address == 0xffe + m_fast_stack_reads && value == (m_fast_stack_reads ? m_timer_probe_return_pc : page),
				"Original FRETE consumes saved XPC then the hardware return PC once");
			++m_fast_stack_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(0x204c, 0x4044, "mu4_timer_original_route",
		[this](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			if (address == 0x204c) { expect(value == 0x4a1e, "Timer vector uses original PSHM XPC"); ++m_pipeline_extensions_reads; }
			if (address == 0x3d73) { expect(value == 0xeeff, "Timer enters the original context handler via FB"); ++m_pipeline_drained_fetches; }
			if (address == 0x4044) { expect(value == 0xf4e5, "Original handler returns through FRETE"); ++m_vector_reads; }
		});
	m_vector_prefetch_tap = program.install_read_tap(foreground + 0x80, foreground + 0x80, "mu4_timer_resumed_foreground",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == 0xe966 && m_fast_stack_reads == 2 && m_vector_reads == 1, "Foreground marker is fetched after original far return");
			++m_pipeline_operand_reads;
		});
	m_phase = 252110; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_mu4_recurring_timer(unsigned variant)
{
	bool const short_period = BIT(variant, 3);
	start_pipeline_mu4_timer_context(variant & 7);
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_vector_prefetch_tap.remove();
	unsigned const page = BIT(variant, 2) ? 3 : 0;
	u32 const foreground = (page << 16) | 0x9000;
	auto &program = m_cpu->space(AS_PROGRAM);
	std::array<u16, 5> const loop{0x6bf8, 0x0700, 1, 0xf073, 0x9000};
	for (unsigned i = 0; i < loop.size(); ++i) program.write_word(foreground + i, loop[i]);
	program.write_word(foreground + 0x80, 0xf495);
	m_cpu->space(AS_DATA).write_word(0x700, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PRD, short_period ? 1 : 0xff);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x20);
	m_cpu->set_state_int(tms320c54x_device::STATE_TIM, 31);
	m_pipeline_replay = false; m_irq_raised = false; m_recurring_irq_end_cycle = 0;
	m_recurring_irq_first_return_writes = ~0U;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0xfe0, 0xfff, "recurring_mu4_stack",
		[this, page](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			if (address == 0xfff) expect(value == m_timer_probe_return_pc, "Each timer entry stacks its actual foreground continuation");
			if (address == 0xffe) expect(value == page, "Each original vector saves the actual foreground page");
			if (address == 0xffd) expect(value == 0x4044, "Each original handler publishes its FRETE return into the reserved slot");
			++m_pipeline_store_writes;
		});
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0xffe, 0xfff, "recurring_mu4_return",
		[this, page](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			bool const pc = BIT(m_fast_stack_reads, 0);
			expect(address == (pc ? 0xfff : 0xffe) && value == (pc ? m_timer_probe_return_pc : page),
				"Every FRETE consumes its current saved page and PC in order");
			++m_fast_stack_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(0x204c, 0x4044, "recurring_mu4_route",
		[this](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			if (address == 0x204c) { expect(value == 0x4a1e, "Recurring timer uses original vector"); ++m_pipeline_extensions_reads; }
			if (address == 0x3d73) { expect(value == 0xeeff, "Recurring timer uses original context handler"); ++m_pipeline_drained_fetches; }
			if (address == 0x4044) { expect(value == 0xf4e5, "Recurring timer uses original far return"); ++m_vector_reads; }
		});
	m_vector_prefetch_tap = m_cpu->space(AS_DATA).install_write_tap(0x700, 0x700, "recurring_mu4_foreground_retirement",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == m_pipeline_operand_reads + 1, "Foreground counter changes only by an actually retired ADDM");
			++m_pipeline_operand_reads;
		});
	m_phase = 252130; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_idle_sp_tail(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(0x23d3a + i, 0xf495); program.write_word(0x20700 + i, 0xf495); }
	std::array<u16, 4> const words{0x8a18, 0xf495, 0xf495, 0xfc00};
	for (unsigned i = 0; i < words.size(); ++i) program.write_word(0x23d3a + i, words[i]);
	program.write_word(0x23d3e, 0xffff); program.write_word(0x20700, 0xe855);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0xffe);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x77);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(0xffe, 0x1000); m_cpu->space(AS_DATA).write_word(0x1000, 0x700);
	m_cpu->set_state_int(STATE_GENPC, 0x23d3a); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1; m_pipeline_operand_reads = m_pipeline_register_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0xffe, 0x1000, "idle_original_sp_return_sources",
		[this](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(address == (m_pipeline_operand_reads ? 0x1000 : 0xffe) && value == (m_pipeline_operand_reads ? 0x700 : 0x1000) &&
				m_cpu->pipeline_cycles() == (m_pipeline_operand_reads ? 8 : 5), "Original suffix pops restored SP then reads the caller return at that restored address");
			++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x18, 0x18, "idle_original_sp_publication",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == 0x1000 && m_cpu->pipeline_cycles() == 5, "POPM SP publishes its popped value without incrementing it afterward");
			++m_pipeline_register_writes;
		});
	m_pipeline_fetch_tap = program.install_read_tap(0x20700, 0x20700, "idle_original_sp_caller",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 10, "Original idle suffix returns to its same-page caller"); ++m_pipeline_extensions_reads;
		});
	m_phase = 252210; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

unsigned tms320c54x_test_state::pipeline_idle_kind(unsigned variant) const
{
	return !strcmp(machine().options().bios(), "idle-modes") ?
		std::array<unsigned,4>{0,1,2,5}[variant % 4] : variant % 6;
}

void tms320c54x_test_state::start_pipeline_idle1(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const modes = !strcmp(machine().options().bios(), "idle-modes");
	unsigned const kind = pipeline_idle_kind(variant);
	u16 const idle = modes ? std::array<u16,3>{0xf4e1,0xf6e1,0xf5e1}[variant / 32] : 0xf4e1;
	bool const blocked = kind == 1 || kind == 4;
	unsigned const source = kind == 3 || kind == 4 ? 3 : 0;
	u32 const base = BIT(variant / (modes ? 4 : 6), 0) ? 0x20000 : 0;
	u16 const vector = kind == 5 ? 4 : 0x40 + source * 4;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 256; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, idle); program.write_word(base + 0x601, 0xe811);
	program.write_word(base + vector, 0x4a1e); program.write_word(base + vector + 1, 0xe955); program.write_word(base + vector + 2, 0xf4e5);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, blocked || kind == 5 ? 0x800 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, kind == 5 ? 0 : 1 << source);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	if (modes)
	{
		m_cpu->set_state_int(tms320c54x_device::STATE_PRD, 0xffff);
		m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x20);
		m_cpu->set_state_int(tms320c54x_device::STATE_TIM, 0x7fff);
	}
	if (source == 3)
	{
		m_cpu->set_state_int(tms320c54x_device::STATE_PRD, 0xffff);
		m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x20); m_cpu->set_state_int(tms320c54x_device::STATE_TIM, 31);
	}
	m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_vector_acks = m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_timer_probe_return_pc = 0;
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601, "idle1_continuation_refetch",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_pipeline_extensions_reads < 2 && value == (m_pipeline_extensions_reads ? 0xe844 : 0xe811),
				"IDLE1 discards the speculative continuation and fetches current program RAM only after wake");
			++m_pipeline_extensions_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0xffe, 0xfff, "idle1_irq_stack",
		[this, base, blocked](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(!blocked && m_pipeline_store_writes < 2 && address == 0xfff - m_pipeline_store_writes &&
				value == (m_pipeline_store_writes ? base >> 16 : 0x601), "IDLE1 ISR stacks the actual continuation PC and page");
			++m_pipeline_store_writes;
		});
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0xffe, 0xfff, "idle1_irq_return",
		[this, base, blocked](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(!blocked && m_pipeline_operand_reads < 2 && address == 0xffe + m_pipeline_operand_reads &&
				value == (m_pipeline_operand_reads ? 0x601 : base >> 16), "IDLE1 wake returns to the unexecuted continuation");
			++m_pipeline_operand_reads;
		});
	m_phase = 252230; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_original_idle(unsigned variant)
{
	bool const service = variant >= 8, initialize = variant >= 16;
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	if (service)
	{
		// Reuse the pinned original timer vector/handler/worker image and
		// typed worker data. This is CPU composition, not native flag setup.
		start_pipeline_mu4_timer_context();
		m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove();
		m_vector_stack_tap.remove(); m_vector_prefetch_tap.remove();
	}
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const skip = !initialize && (service || BIT(variant, 0));
	u16 const sp = 0x1000 + (service ? BIT(variant, 0) : BIT(variant, 1));
	std::array<u16, 29> const idle{0x4a11, 0xeeff, 0x10f8, 0xa531, 0xf844, 0x98e9,
		0x7711, 0x0025, 0x7681, 0x0001, 0x7711, 0x0026, 0x7681, 0x0020,
		0x7711, 0x0000, 0x6881, 0xfff7, 0x7711, 0x0000, 0x6981, 0x08c0,
		0x76f8, 0xa531, 0x0001, 0xf4e1, 0xee01, 0x8a11, 0xf4e4};
	std::array<u16, 15> const wrapper{0x4918, 0x68f8, 0x0018, 0xfffe, 0xf7be,
		0xf495, 0x4a0b, 0x4a0b, 0xf982, 0x98d0, 0xf6be, 0x8a18, 0xf495, 0xf495, 0xfc00};
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 64; ++i) { program.write_word(0x23d2f + i, 0xf495); program.write_word(0x298d0 + i, 0xf495); program.write_word(0x20700 + i, 0xf495); }
	for (unsigned i = 0; i < idle.size(); ++i) program.write_word(0x298d0 + i, idle[i]);
	for (unsigned i = 0; i < wrapper.size(); ++i) program.write_word(0x23d2f + i, wrapper[i]);
	program.write_word(0x20700, 0xe855);
	if (service)
	{
		for (unsigned i = 0; i < wrapper.size(); ++i) program.write_word(0x3d2f + i, wrapper[i]);
		for (unsigned i = 0; i < 512; ++i) program.write_word(0x700 + i, i ? 0xf495 : 0xe855);
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, service ? 0x2020 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, service ? 0 : 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, sp);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x1357);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x77);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, service ? 8 : 0x40);
	auto &data = m_cpu->space(AS_DATA);
	data.write_word(sp, 0x700); data.write_word(0xa531, skip ? 1 : 0);
	m_cpu->set_state_int(STATE_GENPC, 0x23d2f); m_cpu->set_pipeline_frontend(true);
	if (service && !initialize)
	{
		m_cpu->set_state_int(tms320c54x_device::STATE_PRD, 0xffff);
		m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x20);
		m_cpu->set_state_int(tms320c54x_device::STATE_TIM, 119);
	}
	m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_store_writes = m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_vector_acks = m_pipeline_drained_fetches = m_vector_reads = 0;
	m_pipeline_write_tap = data.install_write_tap(0, 0xa531, "original_idle_setup",
		[this, skip, initialize](offs_t at, u16 &value, u16) {
			if (machine().side_effects_disabled() || (at != 0 && at != 0x25 && at != 0x26 && at != 0xa531)) return;
			std::array<u16, 5> const addresses{0x25, 0x26, 0, 0, 0xa531};
			std::array<u16, 5> const values{1, 0x20, initialize ? u16(0) : u16(0x40), 0x8c0, 1};
			expect(!skip && m_pipeline_store_writes < addresses.size() && at == addresses[m_pipeline_store_writes] && value == values[m_pipeline_store_writes],
				"Original zero flag owns ordered timer, interrupt mask and local flag writes; nonzero skips them");
			++m_pipeline_store_writes;
		});
	m_pipeline_fetch_tap = program.install_read_tap(service ? 0x700 : 0x20700, service ? 0x700 : 0x20700, "original_idle_caller",
		[this, service](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == 0xe855 && m_idle_fixture_tick >= 120 && (!service || (m_vector_acks == 1 && m_vector_reads == 1)), "Original wrapper reaches its caller only after physical wake and any required ISR return");
			++m_pipeline_extensions_reads;
		});
	if (service)
		m_vector_prefetch_tap = program.install_read_tap(0x204c, 0x4044, "original_idle_timer_route",
			[this](offs_t at, u16 &value, u16) {
				if (machine().side_effects_disabled()) return;
				if (at == 0x204c) { expect(value == 0x4a1e, "Original timer vector saves XPC before far branching"); ++m_pipeline_operand_reads; }
				if (at == 0x3d73) { expect(value == 0xeeff, "Original idle timer wake enters the uploaded context handler"); ++m_pipeline_drained_fetches; }
				if (at == 0x4044) { expect(value == 0xf4e5, "Original idle timer wake returns through uploaded FRETE"); ++m_vector_reads; }
			});
	m_phase = 252250; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_peripheral_logic(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const operation = variant % 3, mode = (variant / 3) % 3;
	u16 const operand = std::array<u16, 3>{0xf0, 0x8000, 0xffff}[(variant / 9) % 3];
	bool const b = (variant / 27) % 2, sxm = (variant / 54) % 2;
	u32 const base = (variant / 108) % 2 ? 0x20000 : 0;
	u16 const opcode = std::array<u16, 3>{0x1800, 0x1a00, 0x1c00}[operation] | (b ? 0x100 : 0) | (mode == 0 ? 0x25 : mode == 1 ? 0x81 : 0x91);
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	std::array<u16, 4> const code{0x7711, 0x25, u16(b ? 0xe955 : 0xe855), opcode};
	for (unsigned i = 0; i < code.size(); ++i) program.write_word(base + 0x600 + i, code[i]);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PRD, operand);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x600);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | (sxm ? 0x100 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x410);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, b ? 0x2233445566ULL : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, b ? 0 : 0x2233445566ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x25, 0x26, "peripheral_logic_read",
		[this, operand](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == 0x25 && word == operand && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == 9,
			"Peripheral logical operand is read once after exactly one paid access clock"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x25, 0x26, "peripheral_logic_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Accumulator logic must not write its peripheral operand"); });
	m_phase = 252470; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_port_write(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const mode = variant % 5;
	u16 const value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 5) % 4];
	bool const sxm = (variant / 20) % 2;
	u32 const base = (variant / 40) % 2 ? 0x20000 : 0;
	bool const absolute = mode == 0 || mode == 1 || mode == 4;
	u16 const source = mode == 0 ? 8 : mode == 1 || mode == 2 ? 0x500 : mode == 3 ? 0x1020 : 0x25;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, absolute ? 0x75f8 : mode == 2 ? 0x7591 : 0x7520);
	program.write_word(base + 0x601, absolute ? source : 0x124);
	if (absolute) program.write_word(base + 0x602, 0x124);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PRD, value);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | (sxm ? 0x100 : 0) | (mode == 3 ? 0x4000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x500);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, mode == 0 ? 0x1234560000ULL | value : 0x2233445566ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	if (mode == 1 || mode == 2 || mode == 3) m_cpu->space(AS_DATA).write_word(source, value);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_port_writes = 0;
	unsigned const read_cycle = absolute ? mode == 4 ? 8 : 7 : 6;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(source, source, "staged_portw_source",
		[this, value, read_cycle](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(word == value && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == read_cycle,
			"PORTW reads its owned MMR/RAM/peripheral word exactly once before I/O publication"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_IO).install_write_tap(0x124, 0x125, "staged_portw_destination",
		[this, value, read_cycle](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == 0x124 && word == value && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == read_cycle + 1,
			"PORTW publishes its sampled word once through the retained I/O address, not a data-memory destination"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + (absolute ? 0x602 : 0x601), "staged_portw_literals",
		[this, absolute, source](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(m_pipeline_extensions_reads < (absolute ? 2 : 1) && word == (absolute && !m_pipeline_extensions_reads ? source : 0x124) &&
			unsigned(at & 0xffff) == 0x601 + m_pipeline_extensions_reads, "PORTW retains source/port literals in their distinct original order"); ++m_pipeline_extensions_reads; });
	m_phase = 252490; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::long_store_fixture tms320c54x_test_state::peripheral_long_store_fixture(unsigned variant)
{
	unsigned const reg = variant % 8, kind = (variant / 8) % 2;
	bool const compatibility = (variant / 64) % 2, ram = (variant / 384) % 2;
	u16 const offset = std::array<u16, 3>{0x3b, 0x38, 0x49}[(variant / 128) % 3], pointer = ram ? 0x1400 : 0;
	return {reg, compatibility && !reg ? 3U : reg, kind, offset,
		std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 16) % 4], u16(pointer + offset),
		u16(kind ? pointer + offset : pointer), u16(compatibility ? 0x6608 : 0x608), compatibility};
}

void tms320c54x_test_state::start_pipeline_bio_branch(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	bool const low = variant % 2, delayed = (variant / 2) % 2, initial = (variant / 4) % 2;
	unsigned const transition = (variant / 8) % 4;
	bool const sampled = transition == 1 || transition == 2 ? !initial : initial, taken = low ? !sampled : sampled;
	u32 const base = (variant / 32) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); }
	program.write_word(base + 0x600, 0xf802 | low | (delayed << 9)); program.write_word(base + 0x601, 0x700);
	program.write_word(base + 0x602, 0xe811); program.write_word(base + 0x603, 0xe922); program.write_word(base + 0x604, 0xf4e1);
	program.write_word(base + 0x700, 0xe855); program.write_word(base + 0x701, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_bio_level = initial;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_register_writes = m_pipeline_extensions_reads = m_pipeline_operand_reads = 0;
	m_pipeline_operand_tap = program.install_read_tap(base + 0x700, base + 0x700, "bio_branch_target",
		[this, taken](offs_t, u16 &, u16) { if (machine().side_effects_disabled()) return;
			expect(taken && !m_pipeline_operand_reads, "Only the BIO-selected target is fetched, once"); ++m_pipeline_operand_reads; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601, "bio_branch_address",
		[this](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(word == 0x700 && !m_pipeline_extensions_reads, "BIO branch retains its target literal across input changes"); ++m_pipeline_extensions_reads; });
	m_phase = 252670; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::indexed_store_fixture tms320c54x_test_state::circular_portr_case(unsigned v)
{
	unsigned const encoded = v % 8192 + ((v / 8192) % 2 ? 7 : 0) * 8192 + ((v / 16384) % 2) * 65536 + (v / 32768) * 131072;
	return circular_load_case(encoded);
}

void tms320c54x_test_state::start_circular_portr(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = circular_portr_case(variant);
	u16 const port = f.selector ? 0xfffe : 0x125;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0x7400 | f.mode); program.write_word(f.base + 0x601, port); program.write_word(f.base + 0x602, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.pmst); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x8608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.bk); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x9876543210ULL);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, f.initial[i]);
	m_shifted_bus_word = f.word; m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_IO).install_read_tap(0, 0xffff, "circular_portr_sample", [this, f, port](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!f.excluded && at == port && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == 6, "Circular PORTR samples one retained port in Read"); word = m_shifted_bus_word; expect(word == f.word, "Circular PORTR captures the original volatile sample"); m_shifted_bus_word ^= 0xffff; ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "circular_portr_write", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!f.excluded && at == f.address && word == f.word && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == 7, "Circular PORTR writes its retained old address and sampled word once at Execute"); ++m_pipeline_store_writes; });
	m_operand_data_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "circular_portr_no_read", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Circular PORTR never reads its data destination"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x601, "circular_portr_literal", [this, port](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_extensions_reads && word == port, "Circular PORTR fetches its port literal once"); ++m_pipeline_extensions_reads; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62138;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::portr_bypass_fixture tms320c54x_test_state::portr_bypass_case(unsigned variant)
{
	portr_bypass_fixture f{};
	f.chain = variant >= 6144;
	unsigned n = f.chain ? variant - 6144 : variant;
	static constexpr std::array<u16, 8> words{0, 1, 0xffff, 0x8000, 0x7fff, 0xfde5, 0x0002, 0x000a};
	f.sample = words[n % 8]; n /= 8;
	f.a = 0x123456789aULL; f.b = 0xfeabcd9876ULL;
	if (f.chain)
	{
		f.request = words[n % 8]; n /= 8;
		f.a = u16(~f.sample) & f.request; f.b = f.a & 2;
	}
	else
	{
		f.gap = n % 3; n /= 3; f.other = n % 2; n /= 2;
		f.destination = n % 2; n /= 2; f.sxm = n % 2; n /= 2;
		f.operation = n % 4; n /= 4;
		u16 const value = f.other ? 0x55aa : f.sample;
		u64 &result = f.destination ? f.b : f.a;
		if (!f.operation) result = u64(f.sxm ? s64(s16(value)) : s64(value)) & 0xffffffffffULL;
		else if (f.operation == 1) result &= value;
		else if (f.operation == 2) result |= value;
		else result ^= value;
	}
	f.base = n % 2 ? 0x20000 : 0; n /= 2; f.snapshot = n;
	return f;
}

void tms320c54x_test_state::start_portr_bypass(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = portr_bypass_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 24; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0x7482); program.write_word(f.base + 0x601, 0x125);
	if (f.chain)
	{
		program.write_word(f.base + 0x602, 0x1082); program.write_word(f.base + 0x603, 0xf493);
		program.write_word(f.base + 0x604, 0x7482); program.write_word(f.base + 0x605, 0x126);
		program.write_word(f.base + 0x606, 0x1882); program.write_word(f.base + 0x607, 0xf130); program.write_word(f.base + 0x608, 2); program.write_word(f.base + 0x609, 0xf4e1);
	}
	else
	{
		program.write_word(f.base + 0x602 + f.gap, std::array<u16, 4>{0x1000, 0x1800, 0x1a00, 0x1c00}[f.operation] | (f.destination << 8) | (f.other ? 0x83 : 0x82));
		program.write_word(f.base + 0x603 + f.gap, 0xf4e1);
	}
	data.write_word(0x1000, 0xa55a); data.write_word(0x1100, 0x55aa);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | (f.sxm ? 0x100 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfeabcd9876ULL); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x1100);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_port_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_IO).install_read_tap(0, 0xffff, "portr_bypass_sample", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_operand_reads++; expect(i < (f.chain ? 2U : 1U) && at == 0x125 + i && m_cpu->pipeline_cycles() == 6 + 4 * i, "PORTR retains each volatile I/O sample once in Read"); word = i ? f.request : f.sample; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "portr_bypass_write", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_store_writes++; expect(i < (f.chain ? 2U : 1U) && at == 0x1000 && word == (i ? f.request : f.sample) && m_cpu->pipeline_cycles() == 7 + 4 * i, "PORTR retains the real RAM write after bypass"); });
	m_operand_data_tap = data.install_read_tap(0, 0xffff, "portr_bypass_ram_read", [this, f](offs_t at, u16 &, u16) { if (machine().side_effects_disabled()) return; expect(at == 0x1000 || (f.other && at == 0x1100), "PORTR consumers read only their selected RAM"); ++m_port_reads; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x600, f.base + 0x609, "portr_bypass_literals", [this, f](offs_t at, u16 &, u16) { if (machine().side_effects_disabled()) return; if (at == f.base + 0x601 || (f.chain && at == f.base + 0x605)) ++m_pipeline_extensions_reads; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62144;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::circular_portr_block_fixture tms320c54x_test_state::circular_portr_block_case(unsigned v)
{
	circular_portr_block_fixture f{};
	f.count = std::array<unsigned, 3>{2, 7, 17}[v % 3]; f.size = std::array<unsigned, 3>{1, 7, 16}[(v / 3) % 3];
	f.reg = (v / 9) % 8; f.compatibility = (v / 72) % 2; f.selected = f.compatibility && !f.reg ? 4 : f.reg;
	f.index = (v / 144) % 2 ? f.size - 1 : 0; f.buffer = 0x1200 + f.selected * 0x100;
	f.base = (v / 288) % 2 ? 0x20000 : 0; f.snapshot = v / 576;
	f.st0 = f.compatibility && f.reg ? 0x608 | (f.reg << 13) : 0x8608;
	return f;
}

void tms320c54x_test_state::start_circular_portr_block(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = circular_portr_block_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 20; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0x771a); program.write_word(f.base + 0x601, f.count - 1);
	program.write_word(f.base + 0x602, 0xf272); program.write_word(f.base + 0x603, 0x607);
	program.write_word(f.base + 0x606, 0x74d0 | f.reg); program.write_word(f.base + 0x607, 0x27); program.write_word(f.base + 0x608, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x8608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900 | (f.compatibility ? 0x20 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.size); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, i == f.selected ? f.buffer + f.index : 0x3300 + i);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x9876543210ULL);
	m_shifted_bus_word = 0x2468; m_first_port_cycle = 0; m_pipeline_expected_cycle = 0;
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_IO).install_read_tap(0, 0xffff, "circular_portr_block_sample", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const n = m_pipeline_operand_reads++; if (!n) m_first_port_cycle = m_cpu->pipeline_cycles(); expect(n < f.count && at == 0x27 && m_cpu->pipeline_cycles() == m_first_port_cycle + n * 2, "Block PORTR samples each retained port at two-word issue cadence"); expect(m_cpu->state_int(tms320c54x_device::STATE_READPC) == f.base + 0x606, "Read-only pipeline PC identifies PORTR's full-page bus owner"); word = m_shifted_bus_word++; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "circular_portr_block_write", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x1a) { expect(!m_pipeline_register_writes && word == f.count - 1, "Firmware publishes BRC once before RPTBD"); ++m_pipeline_register_writes; return; } unsigned const n = m_pipeline_store_writes++; expect(n < f.count && at == f.buffer + (f.index + n) % f.size && word == 0x2468 + n && m_cpu->pipeline_cycles() == m_first_port_cycle + n * 2 + 1, "Block PORTR wraps the circular destination and publishes each sampled word once"); expect(m_cpu->state_int(tms320c54x_device::STATE_EXECPC) == f.base + 0x606, "Read-only pipeline PC identifies the retained destination publisher"); });
	m_operand_data_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "circular_portr_block_no_read", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Block PORTR never reads its data destination"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62140;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true);
	expect(m_cpu->state_int(tms320c54x_device::STATE_READPC) == 0xffffffff && m_cpu->state_int(tms320c54x_device::STATE_EXECPC) == 0xffffffff, "Empty pipeline slots have no observational instruction owner");
	m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_port_read(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const mode = variant % 13;
	u16 const value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 13) % 4];
	bool const absolute = mode <= 8, compiler = mode == 10;
	u16 const target = mode < 8 ? 8 + mode : compiler ? 0x1020 : mode == 9 ? 0x4a0 : 0x500;
	u32 const base = (variant / 52) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, absolute ? 0x74f8 : mode == 11 ? 0x7491 : mode == 12 ? 0x7489 : 0x7420);
	program.write_word(base + 0x601, absolute ? target : 0x125);
	if (absolute) program.write_word(base + 0x602, 0x125);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x609);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900 | (compiler ? 0x4000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x500);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1212345678ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x349abcdef0ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x1234); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0x5678);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	if (mode >= 8) m_cpu->space(AS_DATA).write_word(target, 0xdead);
	m_cpu->space(AS_IO).write_word(0x125, value);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	unsigned const read_cycle = absolute ? 7 : 6;
	m_pipeline_operand_tap = m_cpu->space(AS_IO).install_read_tap(0, 0xffff, "portr_sample",
		[this, value, read_cycle](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == 0x125 && word == value && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == read_cycle,
				"PORTR samples the retained external port once in Read"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "portr_publication",
		[this, target, value, read_cycle](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == target && word == value && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == read_cycle + 1,
				"PORTR writes its captured data destination once in Execute"); ++m_pipeline_store_writes; });
	m_operand_data_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "portr_no_destination_read",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "PORTR does not read its data destination"); });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + (absolute ? 0x602 : 0x601), "portr_literals",
		[this, absolute, target](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(m_pipeline_extensions_reads < (absolute ? 2 : 1) && word == (absolute && !m_pipeline_extensions_reads ? target : 0x125) &&
				(at & 0xffff) == 0x601 + m_pipeline_extensions_reads, "PORTR retains absolute destination before port literal"); ++m_pipeline_extensions_reads; });
	m_phase = 252790; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::accumulator_store_fixture tms320c54x_test_state::peripheral_long_rmw_fixture(unsigned variant)
{
	unsigned const reg = (variant / 3) % 8, kind = (variant / 24) % 2;
	bool const compatibility = (variant / 288) % 2, ram = (variant / 144) % 2;
	auto f = long_accumulator_store_fixture(reg + (compatibility ? 6400 : 0));
	f.kind = kind; f.opcode = 0x80e0 | (kind << 3) | reg;
	f.literal = std::array<u16, 3>{0x3b, 0x39, 0x49}[(variant / 48) % 3];
	f.address = (ram ? 0x1400 : 0) + f.literal; f.next_ar = kind ? f.address : ram ? 0x1400 : 0;
	return f;
}

void tms320c54x_test_state::start_stlm_bsp(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto f = stlm_t_case(variant % 256);
	f.st1 = (f.st1 & ~u16(0x4000)) | (((variant / 256) % 2) ? 0x4000 : 0);
	f.base = (variant / 512) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 24; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, f.source ? 0x8921 : 0x8821);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.pmst);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.source ? 0xdead : 0x1111); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.source ? 0x1111 : 0xdead);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	data.write_word(0x21, 0xa55a);
	m_pipeline_operand_reads = m_pipeline_store_writes = 0; m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "stlm_bsp_no_reads",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { ++m_pipeline_operand_reads; expect(false, "STLM BSP has no data-source or destination read"); } });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "stlm_bsp_write",
		[this, f](offs_t address, u16 &value, u16) { if (machine().side_effects_disabled()) return;
			expect(!m_pipeline_store_writes && address == 0x21 && value == f.stored && m_cpu->pipeline_cycles() == 7,
				"STLM BSP publishes its Execute-live saturated accumulator once after the peripheral clock"); ++m_pipeline_store_writes; });
	m_phase = 62060; m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_stlm_bsp_edge(unsigned variant)
{
	start_stlm_bsp((variant / 4) % 2 + ((variant / 8) % 2) * 512);
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
	u32 const base = (variant / 8) % 2 ? 0x20000 : 0;
	u16 const word = (variant / 4) % 2 ? 0x8921 : 0x8821;
	auto &program = m_cpu->space(AS_PROGRAM);
	switch (variant % 4)
	{
	case 0: program.write_word(base + 0x600, 0xf273); program.write_word(base + 0x601, 0x700); program.write_word(base + 0x602, word); break;
	case 1: program.write_word(base + 0x600, 0xec01); program.write_word(base + 0x601, word); break;
	case 2: program.write_word(base + 0x600, 0xf072); program.write_word(base + 0x601, 0x603); program.write_word(base + 0x602, word); break;
	case 3: program.write_word(base + 0x600, 0xfd00); program.write_word(base + 0x601, word); break;
	}
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "stlm_bsp_edge_reads", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "stlm_bsp_edge_writes", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_store_writes; });
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_phase = 62062; m_cpu->resume(SUSPEND_REASON_DISABLE);
}

void tms320c54x_test_state::start_pll_transition_case()
{
	// MMIO conformance, not handset provisioning. Suspend instruction
	// execution so this fixture isolates the independently clocked PLL.
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	auto &data = m_cpu->space(AS_DATA);
	data.write_word(0x58, 0);
	m_cpu->set_pll_reference_clock(11'995'200);
	data.write_word(0x58, 0x6004);
	data.write_word(0x58, 0x77ff); // x8/count255 with PLLON/OFF set for retained-lock test.
	expect(data.read_word(0x58) == 0x77fe, "PLLSTATUS remains DIV while lock count runs");
	m_phase = 79500;
	m_check_timer->adjust(attotime::from_usec(200));
}

void tms320c54x_test_state::start_rate_dma(bool input_clock)
{
	m_idle_dma->reset(); m_idle_dma_completions = 0;
	if (input_clock)
	{
		m_idle_dma->set_input_clock(3'250'000);
		m_idle_dma->pll_clock_stop_w(1);
	}
	u16 const registers[] = {0x0520, 0x0530, 3, 0, 0x4145};
	m_idle_dma->write(1, 0);
	for (u16 value : registers) m_idle_dma->write(2, value);
	auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 4; ++i)
	{
		data.write_word(0x0520 + i, 0x1111 * (i + 1));
		data.write_word(0x0530 + i, 0);
	}
	m_idle_dma->write(0, 1);
}

void tms320c54x_test_state::start_clkmd_rate_case()
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_cpu->set_pll_reference_clock(11'995'200);
	m_cpu->set_follow_clkmd(true);
	auto &data = m_cpu->space(AS_DATA);
	data.write_word(0x58, 0x6004);
	expect(m_cpu->clock() == 5'997'600 && m_idle_dma->clock() == 5'997'600 &&
		m_idle_mcbsp->clock() == 5'997'600, "DIV/2 rate propagates from core to system-clocked peripherals");
	m_idle_dma->reset(); m_idle_dma_completions = 0;
	m_idle_mcbsp->reset(); m_idle_mcbsp_words = 0;
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_TIM, 9'999);
	m_cpu->set_state_int(tms320c54x_device::STATE_PRD, 9'999);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0);
	data.write_word(0x58, 0x97fb);
	m_phase = 79600;
	m_check_timer->adjust(attotime::from_ticks(81'727, 239'904'000) - attotime::from_ticks(1, 11'995'200));
}

void tms320c54x_test_state::start_idle3_pll_relock_case()
{
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->space(AS_DATA).write_word(0x58, 0x77ff);
	m_phase = 79700;
	m_check_timer->adjust(attotime::from_usec(341));
}

void tms320c54x_test_state::check_mmr_bus()
{
	auto &data = m_cpu->space(AS_DATA);
	u16 const ar = m_cpu->state_int(tms320c54x_device::STATE_AR3);
	u16 const flags = m_cpu->state_int(tms320c54x_device::STATE_IFR);
	u16 const gap = data.read_word(3);
	unsigned observed = 0;
	auto tap = data.install_write_tap(0x13, 0x13, "mmr_bus_check",
		[&observed](offs_t, u16 &, u16) { ++observed; });
	data.write_word(0x13, 0x1234);
	expect(m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x1234 && data.read_word(0x13) == 0x1234,
		"Mapped AR3 and CPU state share one register");
	data.write_word(0x13, 0xab00, 0xff00); data.write_word(0x13, 0x00cd, 0x00ff);
	expect(data.read_word(0x13) == 0xabcd && observed == 3, "MMR partial writes preserve unselected bits and reach passive taps");
	tap.remove();
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0xbeef);
	data.write_word(1, 0x00ef, 0x00ff);
	expect(data.read_word(1) == 0xbe00, "IFR byte write clears only selected low flags");
	data.write_word(1, 0x0e00, 0xff00);
	expect(data.read_word(1) == 0xb000, "IFR byte write clears only selected high flags");
	data.write_word(3, 0x5a5a);
	expect(data.read_word(3) == 0x5a5a, "Reserved CPU register gap stays in the board map");
	data.write_word(3, gap);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR3, ar);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, flags);
	osd_printf_info("TMS320C54x MMR bus: PASS unified_state=1 partial_write=1 ifr_clear=1 passive_tap=1 reserved_gap=1\n");
}

void tms320c54x_test_state::start_timer_boundary_probe(unsigned index, bool staged)
{
	bool const short_period = !(index & 1);
	bool const direct_mask = (index & 3) >= 2;
	unsigned const gap = staged && index >= 4 ? 4 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	// Synthetic timer reload followed by IMR masking. No handset image.
	u16 const code[] = {0x7711, 0x0025, 0x7681, u16(short_period ? 1 : 0x0100),
		0x7711, 0x0026, 0x7681, 0x0020, 0x7711, 0,
		0x6881, 0xfff7, 0x76f8, 0x0501, 1, 0xf073, 0x060f};
	for (unsigned i = 0; i < std::size(code); ++i)
		program.write_word(0x010600 + i + (i >= 8 ? gap : 0), i == 16 ? code[i] + gap : code[i]);
	for (unsigned i = 0; i < gap; ++i) program.write_word(0x010608 + i, 0xf495);
	// Direct IMR addressing removes the post-reload pointer setup only.
	if (direct_mask)
	{
		program.write_word(0x010608 + gap, 0x68f8); // ANDM #fff7,*(0000)
		program.write_word(0x010609 + gap, 0x0000);
		program.write_word(0x01060a + gap, 0xfff7);
		program.write_word(0x01060b + gap, 0xf495); // NOP; keep the marker location unchanged.
	}
	program.write_word(0x01004c, 0x4a1e); // Save XPC, as a far-return ISR requires.
	program.write_word(0x01004d, 0xf4e5); // FRETE
	data.write_word(0x0501, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 8);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(STATE_GENPC, 0x010600);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_pipeline_frontend(staged);
	m_phase = (staged ? 7020 : 7000) + index;
	m_timer_probe_fetch.remove();
	m_timer_probe_reload.remove();
	m_timer_probe_stack.remove();
	m_timer_probe_write.remove(); m_timer_probe_mask.remove();
	m_timer_probe_foreground.remove();
	m_timer_probe_foreground_fetches = 0;
	m_timer_probe_foreground_first_cycle = 0;
	m_timer_probe_entries = 0;
	m_timer_probe_reload_cycle = 0;
	m_timer_probe_first_entry_cycle = 0;
	m_timer_probe_return_pc = 0;
	m_timer_probe_write_cycle = m_timer_probe_mask_cycle = 0;
	m_timer_probe_writes = m_timer_probe_masks = 0;
	m_timer_probe_write = data.install_write_tap(0x26, 0x26, "timer_probe_reload_write",
		[this, staged](offs_t, u16 &value, u16) {
			if (!machine().side_effects_disabled() && value == 0x20)
			{
				++m_timer_probe_writes; m_timer_probe_write_cycle = m_cpu->total_cycles();
				if (staged) osd_printf_info("Staged timer reload: cycle=%llu prd=%04x imr=%04x ifr=%04x st1=%04x\n",
					(unsigned long long)m_cpu->pipeline_cycles(), unsigned(m_cpu->state_int(tms320c54x_device::STATE_PRD)),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_IMR)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)));
			}
		});
	m_timer_probe_mask = data.install_write_tap(0, 0, "timer_probe_mask_write",
		[this, staged](offs_t, u16 &value, u16) {
			if (!machine().side_effects_disabled() && !(value & 8))
			{
				++m_timer_probe_masks; m_timer_probe_mask_cycle = m_cpu->total_cycles();
				if (staged) osd_printf_info("Staged timer mask: cycle=%llu tim=%04x tcr=%04x ifr=%04x st1=%04x\n",
					(unsigned long long)m_cpu->pipeline_cycles(), unsigned(m_cpu->state_int(tms320c54x_device::STATE_TIM)),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_TCR)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)));
			}
		});
	m_timer_probe_stack = data.install_write_tap(0x0fff, 0x0fff, "timer_probe_stack",
		[this, gap](offs_t, u16 &value, u16) {
			if (!machine().side_effects_disabled() && m_timer_probe_entries == 1 && !m_timer_probe_return_pc)
				m_timer_probe_return_pc = value;
			if (!machine().side_effects_disabled() && gap && m_timer_probe_entries <= 4)
				osd_printf_info("Staged timer continuation push: cycle=%llu pc=%04x entry=%u\n", (unsigned long long)m_cpu->pipeline_cycles(), value, m_timer_probe_entries);
		});
	// Observe scheduler-cycle accounting, not physical pipeline latency.
	// Keep opcode fetch distinct from the observable internal-register bus write.
	m_timer_probe_reload = program.install_read_tap(0x010606, 0x010606, "timer_probe_reload",
		[this, staged](offs_t address, u16 &, u16)
		{
			if (!machine().side_effects_disabled() && (staged || m_cpu->pc() == address + 1))
				m_timer_probe_reload_cycle = m_cpu->total_cycles();
		});
	m_timer_probe_fetch = program.install_read_tap(0x01004c, 0x01004c, "timer_probe_vector",
		[this, staged](offs_t address, u16 &, u16)
		{
			if (machine().side_effects_disabled() || (!staged && m_cpu->pc() != address + 1)) return;
			if (!m_timer_probe_entries++)
			{
				m_timer_probe_first_entry_cycle = m_cpu->total_cycles();
			}
		});
	m_timer_probe_foreground = program.install_read_tap(0x010608 + gap, 0x010608 + gap,
		"timer_probe_foreground_fetch", [this, staged](offs_t address, u16 &, u16) {
			if (machine().side_effects_disabled() || (!staged && m_cpu->pc() != address + 1)) return;
			if (!m_timer_probe_foreground_fetches++)
				m_timer_probe_foreground_first_cycle = m_cpu->total_cycles();
		});
	m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove();
	m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	if (gap)
	{
		m_pipeline_operand_tap = data.install_read_tap(0xffe, 0xfff, "staged_timer_return_samples",
			[this](offs_t address, u16 &value, u16) {
				if (machine().side_effects_disabled()) return;
				if (m_pipeline_operand_reads++ < 8) osd_printf_info("Staged timer return sample: cycle=%llu address=%04x value=%04x\n", (unsigned long long)m_cpu->pipeline_cycles(), unsigned(address), value);
			});
		m_pipeline_fetch_tap = program.install_read_tap(0x10608, 0x10613, "staged_timer_continuation_fetch",
			[this](offs_t address, u16 &value, u16) {
				if (machine().side_effects_disabled()) return;
				if (m_pipeline_extensions_reads++ < 16) osd_printf_info("Staged timer continuation bus: cycle=%llu address=%06x word=%04x\n", (unsigned long long)m_cpu->pipeline_cycles(), unsigned(address), value);
			});
	}
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_bio_case(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	u16 const op = bio_opcodes[index / 2];
	m_bio_level = index & 1;
	unsigned next = 0x010900;
	program.write_word(next++, op);
	bool const branch_or_call = (op & 0xfc00) == 0xf800;
	bool const conditional_execute = (op & 0xfd00) == 0xfd00;
	if (branch_or_call) program.write_word(next++, 0x0910);
	if (conditional_execute)
	{
		program.write_word(next++, 0xe802);
		if (BIT(op, 9)) program.write_word(next++, 0xe903);
	}
	else
	{
		if (BIT(op, 9))
		{
			program.write_word(next++, 0xe905);
			program.write_word(next++, 0xe906);
		}
		program.write_word(next++, 0xe801);
	}
	program.write_word(next, 0xf4e1);
	program.write_word(0x010910, 0xe802);
	program.write_word(0x010911, 0xf4e1);
	data.write_word(0x0400, 0x0910);
	m_cpu->set_state_int(STATE_GENPC, 0x010900);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0400);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 789 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

bool tms320c54x_test_state::standard_bus_io() const
{ return !strcmp(machine().options().bios(), "std-io") || !strcmp(machine().options().bios(), "std-io-atomic"); }

bool tms320c54x_test_state::atomic_bus_io() const
{ return !strcmp(machine().options().bios(), "bus-io-atomic") || !strcmp(machine().options().bios(), "std-io-atomic"); }

unsigned tms320c54x_test_state::bus_io_form(unsigned variant) const
{ return atomic_bus_io() ? variant / 576 : 0; }

u16 tms320c54x_test_state::bus_io_address(unsigned variant) const
{ unsigned const form = bus_io_form(variant); return form == 3 ? 0x30 : form == 4 ? 0x11 : 0x1400; }

u64 tms320c54x_test_state::bus_io_clock() const
{ return atomic_bus_io() ? m_cpu->total_cycles() - m_bus_start_cycles + 1 : m_cpu->pipeline_cycles(); }

tms320c54x_test_state::bus_io_fixture tms320c54x_test_state::bus_io_case(unsigned variant) const
{
	unsigned const v = variant % 192, waits = (v % 8) * (1 + (v / 8) % 2);
	bool const hold = (v / 16) % 2, write = (v / 32) % 2;
	unsigned const form = bus_io_form(variant);
	unsigned const first = atomic_bus_io() ? 2 + unsigned(form >= 1 && form <= 3) + unsigned(form == 3) : write ? 7 : 6, complete = first + waits + (hold && waits >= 2 ? 3 : 0);
	// Input pulses are scheduler-synchronized; save after their delivery settles.
	return {waits, first, complete, std::array<unsigned, 3>{first - 1, first + 1, complete + 1}[(variant % 576) / 192], v / 64, write, hold,
		u16(write && form == 4 ? 0x11 : write || !waits ? 0x1234 : 0xabcd)};
}

void tms320c54x_test_state::start_bus_io_test(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = bus_io_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 64; ++i) { program.write_word(0x600 + i, 0xf495); program.write_word(i, 0xf495); }
	unsigned const form = bus_io_form(variant);
	bool const extended = form >= 1 && form <= 3;
	program.write_word(0x600, (f.write ? 0x7500 : 0x7400) | (form == 1 ? 0xe1 : form == 2 || form == 3 ? 0xf8 : form == 4 ? 0x91 : 0x81));
	program.write_word(0x601, form == 2 || form == 3 ? bus_io_address(variant) : 0x125);
	if (extended) program.write_word(0x602, form == 1 ? 1 : 0x125);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1804); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, form == 1 ? 0x13ff : form == 4 ? 0x11 : 0x1400);
	data.write_word(0x1400, 0x1234); m_cpu->space(AS_IO).write_word(0x125, 0x1234);
	if (form == 3) data.write_word(0x30, 0x1234);
	data.write_word(0x28, (variant % 8) << 12); data.write_word(0x2b, (variant / 8) % 2);
	m_cpu->set_state_int(tms320c54x_device::STATE_PRD, 0xffff); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x20);
	m_bus_ready = !f.hold; m_cpu->set_state_int(STATE_GENPC, 0x600); m_cpu->set_pipeline_frontend(!atomic_bus_io());
	m_bus_start_cycles = m_cpu->total_cycles();
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_vector_acks = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_IO).install_read_tap(0x125, 0x125, "bus_io_read",
		[this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!f.write && !m_pipeline_operand_reads && word == f.value && bus_io_clock() == f.complete, "External PORTR samples once after software/READY waits"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_IO).install_write_tap(0x125, 0x125, "bus_io_write",
		[this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(f.write && !m_pipeline_store_writes && word == f.value && bus_io_clock() == f.complete, "External PORTW preserves one retained source through waits"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(0x601, extended ? 0x602 : 0x601, "bus_io_literal",
		[this, extended](offs_t address, u16 &, u16) { if (!machine().side_effects_disabled()) { expect(m_pipeline_extensions_reads < 1 + unsigned(extended) && address == 0x601 + m_pipeline_extensions_reads, "External I/O extensions retain their order across stalls"); ++m_pipeline_extensions_reads; } });
	m_phase = 253550; m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_standard_swwsr_test()
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	auto &data = m_cpu->space(AS_DATA);
	expect(data.read_word(0x28) == 0x7fff, "Original-family SWWSR resets to seven waits in each field");
	for (unsigned value = 0; value < 0x10000; ++value)
	{
		data.write_word(0x29, value); data.write_word(0x2b, value);
		data.write_word(0x28, value);
		expect(data.read_word(0x28) == (value & 0x7fff) && m_cpu->external_io_wait_states() == ((value >> 12) & 7), "Original SWWSR masks reserved bit 15 and has no multiplier");
		expect(data.read_word(0x29) == value && data.read_word(0x2b) == value && m_cpu->state_int(tms320c54x_device::STATE_BSCR) == 0 && m_cpu->state_int(tms320c54x_device::STATE_SWCR) == 0, "Original SWWSR option does not acquire C5416 register ownership");
	}
	data.write_word(0x28, 0x7fff); data.write_word(0x28, 0, 0x8000);
	expect(data.read_word(0x28) == 0x7fff, "Partial reserved-bit write cannot clear wait fields");
	data.write_word(0x28, 0xabcd, 0x00ff);
	expect(data.read_word(0x28) == 0x7fcd, "Partial original SWWSR write preserves other fields");
	data.write_word(0x28, 0x2000);
	m_saved_pipeline.str({}); m_saved_pipeline.clear();
	expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save original-family SWWSR");
	data.write_word(0x28, 0);
	m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
	expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE && data.read_word(0x28) == 0x2000 && m_cpu->external_io_wait_states() == 2, "Restore the firmware's two-I/O-wait setting");
	m_cpu->reset(); m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	expect(data.read_word(0x28) == 0x7fff, "Reset restores the original SWWSR contract");
	osd_printf_info("TMS320C54x standard SWWSR: PASS encodings=65536 reserved_bit15=0 multiplier=0 separate_BSCR_SWCR=1 partial_writes=1 restore=1 reset=1 program_data_wait_claim=0 physical_mapping=unvalidated\n");
	throw emu_fatalerror(0, "TMS320C54x isolated standard SWWSR complete");
}

void tms320c54x_test_state::start_bus_register_test()
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	auto &data = m_cpu->space(AS_DATA);
	expect(data.read_word(0x28) == 0x7fff && data.read_word(0x29) == 0xf000 && data.read_word(0x2b) == 0 &&
		m_bus_divisor == 4 && !m_bus_iack_enable, "C5416-compatible bus controls reset from SPRS095P tables 3-3/3-4/3-5");
	u32 const system_clock = m_cpu->clock();
	for (unsigned value = 0; value < 0x10000; ++value)
	{
		data.write_word(0x28, value); data.write_word(0x29, value); data.write_word(0x2b, value);
		expect(data.read_word(0x28) == value && data.read_word(0x29) == (value & 0xf006) && data.read_word(0x2b) == (value & 1), "Bus-control reserved fields ignore writes");
		expect(m_cpu->external_io_wait_states() == ((value >> 12) & 7) * (1 + (value & 1)) &&
			m_cpu->clkout_divisor() == 1 + ((value >> 13) & 3) && m_bus_divisor == m_cpu->clkout_divisor() &&
			m_bus_iack_enable == !(value & 0x1000) && m_cpu->iack_output_enabled() == m_bus_iack_enable && m_cpu->clock() == system_clock,
			"Register decode publishes independent CLKOUT/IACK configuration without changing the system clock");
	}
	data.write_word(0x28, 0x7fff); data.write_word(0x29, 0x8006); data.write_word(0x2b, 0);
	m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save CPU-owned bus controls");
	data.write_word(0x28, 0); data.write_word(0x29, 0xffff); data.write_word(0x2b, 1);
	m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore bus controls and derived outputs");
	expect(data.read_word(0x28) == 0x7fff && data.read_word(0x29) == 0x8006 && !data.read_word(0x2b) && m_bus_divisor == 1 && m_bus_iack_enable && m_cpu->external_io_wait_states() == 7,
		"Saved bus state re-drives exact configuration outputs");
	data.write_word(0x29, 0, 0x8000); data.write_word(0x29, 0xffff, 0x0ff9);
	data.write_word(0x2b, 0xffff, 0xfffe); data.write_word(0x28, 0xabcd, 0x00ff);
	expect(data.read_word(0x29) == 6 && data.read_word(0x2b) == 0 && data.read_word(0x28) == 0x7fcd, "Partial writes merge only writable selected fields");
	m_cpu->reset(); m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	expect(data.read_word(0x28) == 0x7fff && data.read_word(0x29) == 0xf000 && !data.read_word(0x2b) && m_bus_divisor == 4 && !m_bus_iack_enable,
		"Device reset restores control state and outputs, independently of saved values");
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(0x600 + i, 0xf495); program.write_word(0x40 + i, 0xf495); }
	program.write_word(0x40, 0xe955);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(STATE_GENPC, 0x600); m_cpu->set_pipeline_frontend(false); m_cpu->set_input_line(0, ASSERT_LINE);
	m_vector_acks = 0; m_phase = 253530; m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_usec(5));
}

void tms320c54x_test_state::start_atomic_io_composition(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	unsigned const context = variant / 8, count = (variant / 2) % 4;
	bool const write = variant % 2;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 64; ++i) { program.write_word(0x600 + i, 0xf495); program.write_word(0x700 + i, 0xf495); program.write_word(i, 0xf495); }
	unsigned body = 0x601;
	if (!context) program.write_word(0x600, 0xec00 | count);
	else if (context == 1) { program.write_word(0x600, 0xfa45); program.write_word(0x601, 0x700); body = 0x602; }
	else program.write_word(0x600, 0xf6bb);
	program.write_word(body, write ? 0x7591 : 0x7491); program.write_word(body + 1, 0x125);
	m_cpu->set_pipeline_frontend(false); m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1804); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, context == 2 ? 0x800 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x1400); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	for (unsigned i = 0; i < 4; ++i) data.write_word(0x1400 + i, 0x1234);
	m_cpu->space(AS_IO).write_word(0x125, 0x1234); data.write_word(0x28, 0x7000); data.write_word(0x2b, 0);
	m_bus_ready = true; m_pipeline_operand_reads = m_pipeline_store_writes = m_vector_acks = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_IO).install_read_tap(0x125, 0x125, "atomic_composition_read",
		[this, write](offs_t, u16 &value, u16) { if (!machine().side_effects_disabled()) { expect(!write && value == 0x1234, "Repeated waited read preserves bus value"); ++m_pipeline_operand_reads; } });
	m_pipeline_write_tap = m_cpu->space(AS_IO).install_write_tap(0x125, 0x125, "atomic_composition_write",
		[this, write](offs_t, u16 &value, u16) { if (!machine().side_effects_disabled()) { expect(write && value == 0x1234, "Repeated waited write preserves source value"); ++m_pipeline_store_writes; } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 253570;
	m_cpu->set_state_int(STATE_GENPC, 0x600); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::parallel_load_fixture tms320c54x_test_state::bsp_parallel_load_case(unsigned variant)
{
	auto c = mapped_parallel_load_case(variant % 6 + (variant / 6) * 192);
	auto &f = c.memory;
	f.xaddress = 0x20;
	f.initial[f.x & 3] += 0x20;
	f.final[f.x & 3] += 0x20;
	f.xword = (variant / 48) % 2 ? 0x8001 : 0x7ffe;
	finish_parallel_load_case(c);
	return c;
}

tms320c54x_test_state::parallel_load_fixture tms320c54x_test_state::peripheral_parallel_load_case(unsigned variant)
{
	auto c = bsp_parallel_load_case(variant / 64);
	auto &f = c.memory;
	unsigned const offset = variant % 64;
	f.xaddress += offset;
	f.initial[f.x & 3] += offset;
	f.final[f.x & 3] += offset;
	return c;
}

void tms320c54x_test_state::start_staged_bus_stm(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const timer = !strcmp(machine().options().bios(), "timer-stm");
	bool const standard = !strcmp(machine().options().bios(), "std-stm");
	bool const clock = !strcmp(machine().options().bios(), "clock-stm");
	bool const serial = !strcmp(machine().options().bios(), "serial-stm");
	u16 const address = serial ? 0x22 : standard ? 0x28 : clock ? 0x58 : (timer ? std::array<u16, 3>{0x24, 0x25, 0x26} : std::array<u16, 3>{0x28, 0x29, 0x2b})[variant % 3];
	u16 const value = (serial ? std::array<u16, 4>{0xc008, 0xc0c8, 0, 0xffff} : clock ? std::array<u16, 4>{0, 0x3146, 0x8006, 0xffff} : standard ? std::array<u16, 4>{0x2000, 0x7fff, 0x8006, 0xffff} : address == 0x26 ? std::array<u16, 4>{0x10, 0x13, 0x30, 0xffff} : std::array<u16, 4>{0, 0x7fff, 0x8006, 0xffff})[(variant / 3) % 4];
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(0x600 + i, 0xf495);
	program.write_word(0x600, 0x7700 | address); program.write_word(0x601, value);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	if (timer)
	{
		// Seed stopped PSC=3 via TRB; writes to PSC itself must not change it.
		data.write_word(0x25, 0x4567); data.write_word(0x26, 0x33); data.write_word(0x24, 0x2345);
	}
	else { data.write_word(0x28, 0); data.write_word(0x29, 0); data.write_word(0x2b, 0); }
	if (clock) { data.write_word(0x58, 0); data.write_word(0x58, 0); }
	m_pipeline_store_writes = m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_write_tap = data.install_write_tap(address, address, "staged_bus_stm_write",
		[this, value](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_store_writes && word == value && m_cpu->pipeline_cycles() == 8, "Control STM publishes once in Execute after its peripheral clock"); ++m_pipeline_store_writes; });
	m_pipeline_operand_tap = data.install_read_tap(address, address, "staged_bus_stm_no_read",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { ++m_pipeline_operand_reads; expect(false, "Bus STM must not read its destination"); } });
	m_pipeline_fetch_tap = program.install_read_tap(0x601, 0x601, "staged_bus_stm_literal",
		[this, value](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_extensions_reads && word == value, "Bus STM owns one literal fetch"); ++m_pipeline_extensions_reads; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 253590;
	m_cpu->set_state_int(STATE_GENPC, 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::mvkd_peripheral_fixture tms320c54x_test_state::mvkd_peripheral_case(unsigned variant)
{
	unsigned const form = variant % 3, read = form == 2 ? 7 : 6;
	return {std::array<u16, 3>{0x3b, 0x48, 0x49}[(variant / 3) % 3],
		std::array<u16, 3>{0x10, 0x19, 0x1400}[(variant / 36) % 3],
		std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 9) % 4],
		u32((variant / 108) % 2 ? 0x20000 : 0), form, read, read + 2,
		std::array<unsigned, 3>{read - 1, read, read + 2}[variant / 216]};
}

void tms320c54x_test_state::start_mvkd_peripheral(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = mvkd_peripheral_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, f.form == 2 ? 0x70f8 : f.form ? 0x7092 : 0x7082);
	program.write_word(f.base + 0x601, f.form == 2 ? f.destination : f.source);
	if (f.form == 2) program.write_word(f.base + 0x602, f.source);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfedcba9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, f.destination);
	data.write_word(f.source, f.value); data.write_word(f.destination, 0xaaaa);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "mvkd_peripheral_source",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == f.source && word == f.value && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == f.read, "MVKD samples one source before its peripheral wait"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "mvkd_peripheral_destination",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == f.destination && word == f.value && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == f.write, "MVKD pays one peripheral clock before publishing its retained source once"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + (f.form == 2 ? 0x602 : 0x601), "mvkd_peripheral_literals",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const part = m_pipeline_extensions_reads++; expect(part < (f.form == 2 ? 2 : 1) && at == f.base + 0x601 + part && word == (f.form == 2 && !part ? f.destination : f.source), "MVKD captures its source/address extensions once in order"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 253750;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::t_peripheral_fixture tms320c54x_test_state::t_peripheral_case(unsigned variant)
{
	unsigned const gap = (variant / 48) % 3, write = 8 + gap;
	return {u16((variant / 2) % 2 ? 0x0f : 0x0e),
		std::array<u16, 3>{0x3b, 0x48, 0x49}[(variant / 16) % 3],
		std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 4) % 4],
		u32((variant / 144) % 2 ? 0x20000 : 0), gap, write,
		std::array<unsigned, 3>{5, write - 1, write}[variant / 288], bool(variant & 1)};
}

void tms320c54x_test_state::start_t_peripheral(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = t_peripheral_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0x8800 | f.source);
	program.write_word(f.base + 0x601 + f.gap, (f.source == 0x0f ? 0x8d00 : 0x8c00) | (f.postincrement ? 0x92 : 0x82));
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.value); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfedcba9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, f.destination);
	data.write_word(f.source, 0xdead); data.write_word(f.destination, 0xaaaa);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "t_peripheral_no_reads",
		[this](offs_t, u16 &, u16) { if (machine().side_effects_disabled()) return; ++m_pipeline_operand_reads; expect(false, "STLM/ST must not read the peripheral destination"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "t_peripheral_producer_and_store",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(word == f.value, "ST consumes the instruction-driven T/TRN producer value"); if (at == f.source) { expect(!m_pipeline_register_writes && m_cpu->pipeline_cycles() == 6, "STLM publishes its T/TRN producer once in Execute"); ++m_pipeline_register_writes; } else { expect(at == f.destination && !m_pipeline_store_writes && m_pipeline_register_writes == 1 && m_cpu->pipeline_cycles() == f.write, "ST publishes once after the peripheral access clock"); ++m_pipeline_store_writes; } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 253770;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_mvdd_peripheral(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u16 const destination = std::array<u16, 3>{0x20, 0x33, 0x5f}[variant % 3], value = std::array<u16, 4>{0, 0x7f, 0x8000, 0xffff}[(variant / 3) % 4];
	unsigned const x = ((variant / 12) % 4) << 2, y = (((variant / 48) % 4) << 2) | 1;
	u32 const base = (variant / 192) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0xe500 | (x << 4) | y); program.write_word(base + 0x601, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfedcba9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 3); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x14b2); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, destination); m_cpu->set_state_int(tms320c54x_device::STATE_BK, 0);
	data.write_word(0x14b2, value); data.write_word(destination, 0xaaaa);
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "mvdd_peripheral_read", [this, value, base, x, y](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		u32 instruction = 0; u16 opcode = 0;
		expect(m_cpu->observe_staged_data_read(at,instruction,opcode) && instruction == base+0x600 && opcode == (0xe500 | (x<<4) | y), "Staged operand bus read retains logical instruction and opcode before Execute");
		expect(!m_cpu->observe_staged_data_read(at^1,instruction,opcode), "Read identity excludes other addresses");
		{ auto const disable = machine().disable_side_effects(); expect(!m_cpu->observe_staged_data_read(at,instruction,opcode), "Read identity excludes debugger peeks"); }
		expect(at == 0x14b2 && word == value && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == 5, "Peripheral MVDD retains one RAM source before its write wait"); ++m_pipeline_operand_reads;
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "mvdd_peripheral_write", [this, destination, value, base, x, y](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		u32 instruction = 0; u16 opcode = 0;
		expect(!m_cpu->observe_staged_data_read(0x14b2,instruction,opcode), "Read identity expires before the destination write");
		u16 source = 0;
		expect(m_cpu->observe_staged_mvdd_write(at,instruction,opcode,source) && instruction == base+0x600 && opcode == (0xe500 | (x<<4) | y) && source == 0x14b2, "MVDD write identity retains instruction, opcode and original source address");
		expect(!m_cpu->observe_staged_mvdd_write(at^1,instruction,opcode,source), "MVDD write identity excludes other destinations");
		{ auto const disable = machine().disable_side_effects(); expect(!m_cpu->observe_staged_mvdd_write(at,instruction,opcode,source), "MVDD write identity excludes peeks"); }
		expect(at == destination && word == value && !m_pipeline_store_writes && m_pipeline_operand_reads == 1 && m_cpu->pipeline_cycles() == 7, "Peripheral MVDD publishes once after paying one extra cycle"); ++m_pipeline_store_writes;
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254320;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_clkmd_move_edge(unsigned variant)
{
	unsigned const form = variant % 3, context = (variant / 3) % 2;
	u32 const base = (variant / 6) % 2 ? 0x20000 : 0;
	start_mvdk_mapped(form + 3 + (base ? 12 : 0));
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(base + 0x600 + i, 0xf495);
	unsigned pc = base + 0x600;
	program.write_word(pc++, context ? 0xf273 : 0xec01);
	if (context) program.write_word(pc++, 0x680);
	program.write_word(pc++, form == 0 ? 0x7120 : form == 1 ? 0x7196 : 0x71f8);
	program.write_word(pc++, form == 2 ? 0x1420 : 0x58);
	if (form == 2) program.write_word(pc++, 0x58);
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove();
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0,0xffff,"clock_move_edge_reads",[this](offs_t,u16 &,u16) { if (!machine().side_effects_disabled()) ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0,0xffff,"clock_move_edge_writes",[this](offs_t,u16 &,u16) { if (!machine().side_effects_disabled()) ++m_pipeline_store_writes; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62070;
	m_cpu->set_state_int(STATE_GENPC,base+0x600); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock()));
}

void tms320c54x_test_state::start_mcbsp_control_sequence(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u32 const base = variant & 1 ? 0x20000 : 0; bool const sxm = variant & 2;
	m_idle_mcbsp_external = false; m_idle_mcbsp->reset(); m_idle_mcbsp_words = 0;
	m_idle_mcbsp->control_w(0, 1); m_idle_mcbsp->control_w(1, 1); m_idle_mcbsp->control_w(0, 5);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	static constexpr u16 words[] = {0x76e1, 0x38, 1, 0x68e1, 0x39, 0xfffe, 0xf4e1};
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, i < std::size(words) ? words[i] : 0xf495);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1600); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, sxm ? 0x900 : 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0); m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xabcdef1234ULL);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_drained_fetches = 0;
	m_pipeline_operand_tap = data.install_read_tap(0x38, 0x39, "mcbsp_control_read", [this](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == 0x39 && word == 3 && !m_pipeline_operand_reads && m_pipeline_store_writes == 1, "ANDM reads actual enabled/ready McBSP status after selector publication"); ++m_pipeline_operand_reads; m_idle_mcbsp->data_w(3, 0xa55a); expect(m_idle_mcbsp->control_r(1) == 1, "A real DXR write changes XRDY after the captured CPU read"); ++m_pipeline_drained_fetches; });
	m_pipeline_write_tap = data.install_write_tap(0x38, 0x39, "mcbsp_control_write", [this](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(m_pipeline_store_writes < 2 && at == (m_pipeline_store_writes ? 0x39 : 0x38) && word == (m_pipeline_store_writes ? 2 : 1) && (!m_pipeline_store_writes || m_pipeline_operand_reads == 1), "Actual McBSP selector/read/reset order retains the volatile status word"); ++m_pipeline_store_writes; });
	m_pipeline_replay = false; m_idle_fixture_tick = m_idle_fixture_saved_tick = 0; m_phase = 253950;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::shifted_peripheral_fixture tms320c54x_test_state::shifted_peripheral_case(unsigned variant, bool bsp, bool unowned)
{
	shifted_peripheral_fixture f{}; f.bsp = bsp; f.unowned = unowned;
	f.address = bsp ? 0x20 : unowned ? 2 + variant % 4 : 0x20 + variant % 64; if (!bsp) variant /= unowned ? 4 : 64;
	unsigned const shift_code = bsp || unowned ? variant % 32 : std::array<unsigned, 4>{16, 31, 4, 15}[variant % 4]; variant /= bsp || unowned ? 32 : 4;
	f.form = variant % 5; variant /= 5;
	unsigned const flags = variant % 4; variant /= 4;
	f.b = variant % 2; variant /= 2;
	f.value = std::array<u16, 4>{0, 0xffff, 0x8000, 0x1234}[variant % 4]; variant /= 4;
	f.base = variant % 2 ? 0x20000 : 0; variant /= 2;
	f.read = (f.form == 2 || f.form == 4 ? 8 : 7) - unsigned(unowned);
	f.checkpoint = f.read - 2 + variant;
	f.literals = f.form == 2 || f.form == 4 ? 2 : 1;
	f.st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0) | (f.form == 1 ? 0x4000 : 0);
	f.opcode = 0x6f00 | (f.form < 2 ? f.address : f.form == 2 ? 0xf8 : f.form == 3 ? 0x91 : 0xe1);
	f.operation = 0x0c40 | (f.b ? 0x100 : 0) | shift_code;
	int const shift = shift_code >= 16 ? int(shift_code) - 32 : int(shift_code);
	s64 const value = flags & 1 ? s16(f.value) : f.value;
	f.result = shift >= 0 ? u64(value * (s64(1) << shift)) & 0xffffffffffULL : flags & 1 ? u64(value >> -shift) & 0xffffffffffULL : u64(value) >> -shift;
	return f;
}

bool tms320c54x_test_state::shifted_bsp_fixture() const
{ return !strcmp(machine().options().bios(), "shift-bsp"); }

void tms320c54x_test_state::start_shifted_peripheral(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = shifted_peripheral_case(variant, shifted_bsp_fixture(), shifted_unowned_fixture()); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, f.opcode);
	if (f.literals == 2) program.write_word(f.base + 0x601, f.form == 2 ? f.address : 0x10);
	program.write_word(f.base + 0x600 + f.literals, f.operation);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xb800); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0); m_cpu->set_state_int(tms320c54x_device::STATE_AR1, f.form == 4 ? u16(f.address - 0x10) : f.address);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfedcba9876ULL);
	m_shifted_bus_word = f.value; m_shifted_bsp.reset(); m_shifted_bsp.control_w(0x80);
	if (f.bsp) { expect(m_shifted_bsp.frame_receive(f.value), "Seed one completed BSP receive frame"); expect(!m_shifted_bsp.frame_receive(f.value ^ 0xffff) && m_shifted_bsp.receive_overrun, "Unread BSP frame establishes an overrun for the CPU to acknowledge"); }
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0; m_pipeline_replay = false; m_idle_fixture_tick = 0;
	// The generic cohort supplies a volatile bus endpoint, not a recovered part identity.
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "shifted_peripheral_read", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) { if (at == f.address) word = f.bsp ? m_shifted_bsp.receive : m_shifted_bus_word; return; }
		expect(at == f.address && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == f.read, "Shifted LD consumes one owned bus Read with its address-class timing");
		word = f.bsp ? m_shifted_bsp.receive_r() : m_shifted_bus_word; expect(word == f.value, "Shifted load captures the original volatile word"); ++m_pipeline_operand_reads;
		m_shifted_bus_word = f.value ^ 0xffff;
		if (f.bsp) { expect(!m_shifted_bsp.receive_ready && !m_shifted_bsp.receive_overrun, "The CPU read acknowledges BSP ready and overrun once"); expect(m_shifted_bsp.frame_receive(m_shifted_bus_word), "A second frame can arrive after acknowledgement and must remain unread"); }
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "shifted_peripheral_no_writes", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { ++m_pipeline_store_writes; expect(false, "Shifted peripheral LD never writes a data endpoint"); } });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x600 + f.literals, "shifted_peripheral_literals", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		unsigned const index = m_pipeline_extensions_reads++;
		expect(index < f.literals && at == f.base + 0x601 + index && word == (f.literals == 2 && !index ? f.form == 2 ? f.address : 0x10 : f.operation), "Shifted load fetches its address/operation words once in order");
	});
	m_phase = 62120; m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_shifted_peripheral_edge(unsigned variant)
{
	start_shifted_peripheral(); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove();
	u32 const base = (variant / 16) % 2 ? 0x20000 : 0;
	u16 const operation = 0x0c44 | ((variant / 8) % 2 ? 0x100 : 0);
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	switch (variant % 8)
	{
	case 0: program.write_word(base + 0x600, 0xec01); program.write_word(base + 0x601, 0x6f20); program.write_word(base + 0x602, operation); break;
	case 1: program.write_word(base + 0x600, 0xf072); program.write_word(base + 0x601, 0x604); program.write_word(base + 0x602, 0x6f20); program.write_word(base + 0x603, operation); break;
	case 2: program.write_word(base + 0x600, 0xfd00); program.write_word(base + 0x601, 0x6f20); program.write_word(base + 0x602, operation); break;
	case 3: program.write_word(base + 0x600, 0xf273); program.write_word(base + 0x601, 0x700); program.write_word(base + 0x602, 0x6f20); program.write_word(base + 0x603, operation); break;
	case 4: case 5: program.write_word(base + 0x600, variant % 8 == 4 ? 0x6f07 : 0x6f1a); program.write_word(base + 0x601, operation); break;
	case 6: case 7: program.write_word(base + 0x600, 0x6f20); program.write_word(base + 0x601, (operation & ~u16(0x0040)) | (variant % 8 == 7 ? 0x20 : 0)); break;
	}
	if (!strcmp(machine().options().bios(), "shift-bk-edge"))
	{
		unsigned const prefix = variant % 8 == 0 || variant % 8 == 2 ? 1 : variant % 8 == 1 || variant % 8 == 3 ? 2 : 0;
		program.write_word(base + 0x600 + prefix, 0x6ff8);
		program.write_word(base + 0x601 + prefix, variant % 8 == 4 ? 7 : variant % 8 == 5 ? 0x1a : 0x19);
		program.write_word(base + 0x602 + prefix, (operation & ~u16(0x0040)) | (variant % 8 == 6 ? 0x40 : variant % 8 == 7 ? 0x60 : 0));
	}
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "shifted_peripheral_edge_reads", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "shifted_peripheral_edge_writes", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_store_writes; });
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_phase = 62122; m_cpu->resume(SUSPEND_REASON_DISABLE);
}

void tms320c54x_test_state::start_ldu_peripheral(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const address = 0x20 + variant % 64, form = (variant / 256) % 3, read = form == 1 ? 7 : 6;
	u16 const value = std::array<u16, 4>{0, 0xffff, 0x8000, 0x1234}[(variant / 64) % 4];
	bool const b = (variant / 768) % 2;
	u32 const base = (variant / 1536) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0x1200 | (b ? 0x100 : 0) | (form == 1 ? 0xf8 : form == 2 ? 0x81 : address));
	if (form == 1) program.write_word(base + 0x601, address);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1600); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xabcdef1234ULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x123456789aULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, address);
	m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	// Synthetic bus endpoint: proves CPU transaction semantics, not any part's register map.
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "ldu_peripheral_read", [this, address, value, read](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == address && !m_pipeline_operand_reads++ && m_cpu->pipeline_cycles() == read, "Peripheral LDU pays one extra clock and reads its endpoint exactly once"); word = value; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "ldu_peripheral_no_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Peripheral LDU never writes a bus endpoint"); });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601, "ldu_peripheral_literal", [this, form, address](offs_t, u16 &word, u16) { if (machine().side_effects_disabled() || form != 1) return; expect(word == address && !m_pipeline_extensions_reads++, "Peripheral LDU fetches one retained address literal"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254420;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

u16 tms320c54x_test_state::test_port_r()
{
	const u64 cycle = m_cpu->total_cycles();
	if (m_port_reads++ == 0)
		m_first_port_cycle = cycle;
	else
		m_last_port_cycle = cycle;
	return 0xabcd;
}

void tms320c54x_test_state::test_port_w(u16 value)
{
	const u64 cycle = m_cpu->total_cycles();
	if (m_port_writes++ == 0)
	{
		m_first_port_cycle = cycle;
		m_first_port_value = value;
	}
	else
	{
		if (m_port_writes == 2)
		{
			m_middle_port_value = value;
			m_middle_port_cycle = cycle;
		}
		m_last_port_cycle = cycle;
		m_last_port_value = value;
	}
}

void tms320c54x_test_state::start_bsp_interrupt_case(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	// Architectural source fixture, not a handset codec clock or ROM patch.
	program.write_word(0x010a00, 0xf4e1);
	program.write_word(0x010a01, 0x76f8);
	program.write_word(0x010a02, 0x0501);
	program.write_word(0x010a03, 1);
	program.write_word(0x010a04, 0xf4e1);
	program.write_word(0x010050, 0xf4e1); // BRINT0, vector 20.
	program.write_word(0x010054, 0xf4e1); // BXINT0, vector 21.
	data.write_word(0x0501, 0);
	m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, index < 8 && (index & 2) ? 0x0800 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	unsigned const source = index < 4 ? 4 : 5;
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR,
		index == 8 ? 0x30 : (index & 1) ? (1U << source) : 0);
	m_cpu->set_state_int(STATE_GENPC, 0x010a00);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 78000 + index * 4;
	m_check_timer->adjust(attotime::from_usec(100));
}

bool tms320c54x_test_state::check_idle_nmi_result_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 7100 && m_phase < 7160)
	{
		unsigned const index = (m_phase - 7100) / 5;
		unsigned const step = (m_phase - 7100) % 5;
		if (step == 0)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(STATE_GENPC) == 0x010981,
				"each idle mode remains suspended before NMI");
			// Freeze instruction execution, not the input synchronizer, to
			// capture an already-latched request before vector entry.
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0x8008);
			m_cpu->set_input_line(INPUT_LINE_NMI, ASSERT_LINE);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		if (step == 1)
		{
			expect(!m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(STATE_GENPC) == 0x010981 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000,
				"NMI request wakes idle but remains unserviced while fixture execution is suspended");
			m_saved_repeat.str(std::string());
			m_saved_repeat.clear();
			expect(machine().save().write_stream(m_saved_repeat) == STATERR_NONE,
				"save latched NMI and held input before vector entry");
			m_cpu->resume(SUSPEND_REASON_DISABLE);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		check_idle_nmi_result(index);
		if (step == 2)
		{
			m_saved_repeat.clear();
			m_saved_repeat.seekg(0);
			expect(machine().save().read_stream(m_saved_repeat) == STATERR_NONE &&
				m_cpu->state_int(STATE_GENPC) == 0x010981 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 &&
				m_cpu->space(AS_DATA).read_word(0x0fff) == 0x5a5a &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x8008 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == (BIT(index, 1) ? 0x0800 : 0),
				"restore pending NMI before its stack write, preserving masks and original INTM");
			m_cpu->resume(SUSPEND_REASON_DISABLE);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		if (step == 3)
		{
			m_cpu->set_input_line(INPUT_LINE_NMI, ASSERT_LINE);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		if (index < 11) { start_idle_nmi_case(index + 1); return true; }
		osd_printf_info("TMS320C54x NMI idle wake: PASS cases=12 cycle_accuracy_claim=0\n");
		osd_printf_info("TMS320C54x NMI pending restore: PASS cases=12 held_line_cases=12 timing_claim=0\n");
		check_control_disassembler();
		start_shifted_high_store_case(0);
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_idle_wake_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 6200 && m_phase < 6212)
	{
		unsigned const index = (m_phase - 6200) / 2;
		if (!(m_phase & 1))
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(STATE_GENPC) == 0x010901,
				"IDLE1 reaches its continuation before the wake source is supplied");
			if (index < 4)
			{
				m_cpu->set_input_line(0, ASSERT_LINE);
				m_cpu->set_input_line(0, CLEAR_LINE);
			}
			else
			{
				m_cpu->set_state_int(tms320c54x_device::STATE_PRD, 9);
				m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x20);
			}
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		bool const enabled = index & 1;
		bool const serviced = enabled && index < 2;
		bool const continued = enabled && !serviced;
		expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->space(AS_DATA).read_word(0x0501) == (continued ? 1 : 0) &&
			m_cpu->state_int(STATE_GENPC) == (serviced ? 0x010041 : continued ? 0x010907 : 0x010901) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (serviced ? 0x0fff : 0x1000) &&
			(serviced || (m_cpu->state_int(tms320c54x_device::STATE_IFR) & (index >= 4 ? 8 : 1))),
			"SPRU131G IDLE1 wake requires IMR enable, independently of INTM; masked requests remain pending");
		if (index < 5) { start_idle_wake_case(index + 1); return true; }
		osd_printf_info("TMS320C54x IDLE1 wake: PASS external_cases=4 timer_cases=2\n");
		start_idle_timer_case(0);
		return true;
	}
	if ((m_phase >= 6220 && m_phase < 6232) || (m_phase >= 6240 && m_phase < 6244))
	{
		unsigned const index = m_phase >= 6240 ? m_phase - 6240 : (m_phase - 6220) / 3;
		unsigned const step = m_phase >= 6240 ? 3 : (m_phase - 6220) % 3;
		if (step < 2)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(STATE_GENPC) == 0x010921 &&
				!(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 8) && m_idle_mcbsp_words == 0,
				"IDLE2/3 stop the timer without changing its running control bit");
			u16 const counter = m_cpu->state_int(tms320c54x_device::STATE_TIM);
			if (!step)
			{
				m_idle_timer_counter = counter;
				m_idle_timer_control = m_cpu->state_int(tms320c54x_device::STATE_TCR);
			}
			else
			{
				expect(counter == m_idle_timer_counter && counter < 100 &&
					m_cpu->state_int(tms320c54x_device::STATE_TCR) == m_idle_timer_control &&
					!(m_idle_timer_control & 0x10),
					"deep-idle timer counter is frozen, not reloaded or stopped through TSS");
				m_saved_repeat.str(std::string());
				m_saved_repeat.clear();
				expect(machine().save().write_stream(m_saved_repeat) == STATERR_NONE,
					"save frozen deep-idle timer state");
				m_cpu->set_input_line(0, ASSERT_LINE);
				m_cpu->set_input_line(0, CLEAR_LINE);
			}
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		bool const enabled = index & 1;
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->space(AS_DATA).read_word(0x0501) == (enabled ? 1 : 0) &&
			m_cpu->state_int(STATE_GENPC) == (enabled ? 0x010925 : 0x010921) &&
			bool(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 8) == enabled &&
			m_idle_mcbsp_words == (enabled ? 1 : 0),
			"enabled external wake resumes timer operation; masked wake leaves deep idle intact");
		if (step == 2)
		{
			m_saved_repeat.clear();
			m_saved_repeat.seekg(0);
			expect(machine().save().read_stream(m_saved_repeat) == STATERR_NONE &&
				m_cpu->state_int(tms320c54x_device::STATE_TIM) == m_idle_timer_counter &&
				m_cpu->state_int(tms320c54x_device::STATE_TCR) == m_idle_timer_control &&
				m_cpu->space(AS_DATA).read_word(0x0501) == 0,
				"restore frozen timer count, prescaler and foreground continuation");
			m_idle_mcbsp_words = 0;
			m_cpu->set_input_line(0, ASSERT_LINE);
			m_cpu->set_input_line(0, CLEAR_LINE);
			m_phase = 6240 + index;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		if (index < 3) { start_idle_timer_case(index + 1); return true; }
		osd_printf_info("TMS320C54x IDLE2/3 timer clock: PASS cases=4 save_replay=4\n");
		osd_printf_info("TMS320C54x IDLE2/3 McBSP internal clock: PASS cases=4 save_replay=4\n");
		start_idle_external_serial_case(0);
		return true;
	}
	if (m_phase >= 6260 && m_phase < 6268)
	{
		unsigned const index = (m_phase - 6260) / 2;
		if (!(m_phase & 1))
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(STATE_GENPC) == 0x010941 && m_idle_mcbsp_words == 0,
				"external McBSP awaits frame and clock in deep idle");
			m_idle_mcbsp->tx_frame_w(1);
			for (unsigned bit = 0; bit < 16; ++bit)
			{
				m_idle_mcbsp->tx_clock_w(1);
				m_idle_mcbsp->tx_clock_w(0);
			}
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		bool const enabled = index & 1;
		expect(m_idle_mcbsp_words == 1 &&
			(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 0x20) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(STATE_GENPC) == (enabled ? 0x010945 : 0x010941) &&
			m_cpu->space(AS_DATA).read_word(0x0501) == (enabled ? 1 : 0),
			"external McBSP completes while clocks stop and wakes only with its IMR bit enabled");
		if (index < 3) { start_idle_external_serial_case(index + 1); return true; }
		osd_printf_info("TMS320C54x IDLE2/3 McBSP external clock: PASS cases=4\n");
		start_idle_dma_case(0);
		return true;
	}
	if (m_phase >= 6348 && m_phase < 6350)
	{
		unsigned const index = m_phase - 6340;
		expect(machine().time() - m_idle_dma_start == attotime::from_ticks(2, m_idle_dma_rate) &&
			m_idle_dma_completions == 0 && m_cpu->space(AS_DATA).read_word(0x0530) == 0,
			"DMA clock loss occurs halfway through its first four-clock word");
		m_idle_dma->pll_clock_stop_w(1);
		m_saved_repeat.str(std::string());
		m_saved_repeat.clear();
		expect(machine().save().write_stream(m_saved_repeat) == STATERR_NONE,
			"save clockless DMA with two clocks remaining on its first word");
		m_phase = 6281 + index * 2;
		m_check_timer->adjust(attotime::from_usec(100));
		return true;
	}
	if ((m_phase >= 6280 && m_phase < 6300) || (m_phase >= 6300 && m_phase < 6310) ||
		(m_phase >= 6326 && m_phase < 6330))
	{
		unsigned const index = m_phase >= 6326 ? m_phase - 6320 : m_phase >= 6300 ? m_phase - 6300 : (m_phase - 6280) / 2;
		unsigned const step = m_phase >= 6326 ? 1 : m_phase >= 6300 ? 2 : (m_phase - 6280) % 2;
		if (!step)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(STATE_GENPC) == 0x010961 && m_idle_dma_completions == 0,
				"DMA fixture starts with CPU idle and channel disabled");
			m_idle_dma_start = machine().time();
			if (index >= 8) m_idle_dma->pll_clock_stop_w(0);
			m_idle_dma->write(0, 1);
			if (index >= 8)
			{
				m_phase = 6340 + index;
				m_check_timer->adjust(attotime::from_ticks(2, m_idle_dma_rate));
				return true;
			}
			m_saved_repeat.str(std::string());
			m_saved_repeat.clear();
			expect(machine().save().write_stream(m_saved_repeat) == STATERR_NONE,
				"save pending DMA transfer with CPU idle");
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		if (index >= 6 && m_phase < 6300)
		{
			expect(m_idle_dma_completions == 0 && (m_idle_dma->read(0) & 1) &&
				m_cpu->space(AS_DATA).read_word(0x0530) == 0,
				"IDLE3 DMA stays pending without X2/CLKIN");
			// Controller fixture restores its system clock; no handset PLL delay is inferred.
			m_idle_dma_start = machine().time();
			m_idle_dma->pll_clock_stop_w(0);
			m_phase = 6320 + index;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		bool const enabled = index & 1;
		for (unsigned i = 0; i < 4; ++i)
			expect(m_cpu->space(AS_DATA).read_word(0x0530 + i) == 0x1111 * (i + 1),
				"DMA copies its complete block while CPU execution is suspended");
		expect(m_idle_dma_completions == 1 && !(m_idle_dma->read(0) & 1) &&
			(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 0x40) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(STATE_GENPC) == (enabled ? 0x010965 : 0x010961) &&
			m_cpu->space(AS_DATA).read_word(0x0501) == (enabled ? 1 : 0),
			"DMA completion wakes each idle mode only when enabled in IMR, independently of INTM");
		if (step == 1)
		{
			m_saved_repeat.clear();
			m_saved_repeat.seekg(0);
			expect(machine().save().read_stream(m_saved_repeat) == STATERR_NONE &&
				(m_idle_dma->read(0) & 1) && m_cpu->state_int(STATE_GENPC) == 0x010961 &&
				m_cpu->space(AS_DATA).read_word(0x0530) == 0,
				"restore pending DMA and sleeping CPU before the first transfer");
			m_idle_dma_completions = 0;
			if (index >= 6)
			{
				m_idle_dma_start = machine().time();
				m_idle_dma->pll_clock_stop_w(0);
			}
			m_phase = 6300 + index;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		if (index < 9) { start_idle_dma_case(index + 1); return true; }
		osd_printf_info("TMS320C54x IDLE DMA transfer: PASS cases=10 save_replay=10 clock_rate_claim=0\n");
		osd_printf_info("TMS320C54x internal DMA cadence: PASS cases=10 replay=10 words=4 clocks=16\n");
		osd_printf_info("TMS320C54x IDLE3 DMA clock: PASS cases=2 replay=2 system_hz=13000000 input_hz=3250000\n");
		osd_printf_info("TMS320C54x IDLE3 DMA absent clock: PASS cases=2 replay=2 pending_preserved=1\n");
		osd_printf_info("TMS320C54x DMA partial clock loss: PASS cases=2 replay=2 first_word_remaining_clocks=2\n");
		start_rounded_multiply_case(0);
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_stopped_timer_reload_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 7300 && m_phase <= 7347)
	{
		unsigned const index = m_phase - 7300;
		unsigned const divider = index % 16;
		osd_printf_info("TMS320C54x stopped reload case: divider=%u variant=%u a=%llx tim=%04x idle=%u illegal=%u pc=%06x\n",
			divider, index / 16, static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_TIM)),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_IDLE)),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)),
			unsigned(m_cpu->state_int(STATE_GENPC)));
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (0x10 | divider | (divider << 6)) &&
			m_cpu->state_int(tms320c54x_device::STATE_TIM) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),
			"SPRU131G TRB reloads stopped TIM and read-only PSC; TCR writes preserve PSC and TRB reads zero");
		if (index < 47) { start_stopped_timer_reload_case(index + 1); return true; }
		osd_printf_info("TMS320C54x stopped timer reload: PASS divider_cases=16 write_variants=3\n");
		start_timer_prescaler_divider_case(1);
		return true;
	}
	if (m_phase >= 7351 && m_phase <= 7365)
	{
		unsigned const divider = m_phase - 7350;
		u16 const tim = m_cpu->state_int(tms320c54x_device::STATE_TIM);
		u16 const tcr = m_cpu->state_int(tms320c54x_device::STATE_TCR);
		osd_printf_info("TMS320C54x timer divider change: loaded_psc=%u tim=%04x tcr=%04x\n",
			divider, tim, tcr);
		expect(tim == 2 && tcr == ((divider - 1) << 6) &&
			!m_cpu->state_int(tms320c54x_device::STATE_IFR),
			"SPRU131G changing TDDR preserves current PSC until borrow or TRB");
		m_saved_repeat.str(std::string());
		m_saved_repeat.clear();
		expect(machine().save().write_stream(m_saved_repeat) == STATERR_NONE,
			"save current PSC larger than new TDDR");
		m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
		m_cpu->set_state_int(tms320c54x_device::STATE_PRD, 9);
		m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x20);
		m_saved_repeat.clear();
		m_saved_repeat.seekg(0);
		expect(machine().save().read_stream(m_saved_repeat) == STATERR_NONE &&
			m_cpu->state_int(tms320c54x_device::STATE_TIM) == tim &&
			m_cpu->state_int(tms320c54x_device::STATE_TCR) == tcr,
			"restore current PSC interval independently of overwritten timer reload");
		m_phase = 7450 + divider;
		// Observe after the first borrow, halfway to the following edge.
		m_check_timer->adjust(attotime::from_ticks(2 * divider + 1, m_cpu->clock()) / 2);
		return true;
	}
	if ((m_phase >= 7451 && m_phase <= 7465) ||
		(m_phase >= 7551 && m_phase <= 7565) ||
		(m_phase >= 7651 && m_phase <= 7665))
	{
		unsigned const stage = (m_phase - 7350) / 100;
		unsigned const divider = (m_phase - 7350) % 100;
		u16 const expected_tim = stage == 1 ? 1 : stage == 2 ? 0 : 2;
		expect(m_cpu->state_int(tms320c54x_device::STATE_TIM) == expected_tim &&
			m_cpu->state_int(tms320c54x_device::STATE_TCR) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_IFR) == (stage == 3 ? 8 : 0),
			"SPRU131G PSC borrows once, then reloads new TDDR; timer expiry reloads PRD and latches TINT");
		if (stage < 3)
		{
			m_phase += 100;
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
			return true;
		}
		if (divider < 15) { start_timer_prescaler_divider_case(divider + 1); return true; }
		osd_printf_info("TMS320C54x timer divider change: PASS cases=15 save_replay=15 borrow_expiry_checks=45\n");
		start_timer_counter_write_case(2);
		return true;
	}
	if (m_phase >= 7702 && m_phase <= 7715)
	{
		// Do not read TIM/TCR here: a read would refresh cached PSC and
		// conceal a write path that otherwise restarts the prescaler.
		m_cpu->set_state_int(tms320c54x_device::STATE_TIM, 9);
		m_phase += 100;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase >= 7802 && m_phase <= 7815)
	{
		unsigned const divider = m_phase - 7800;
		u16 const tim = m_cpu->state_int(tms320c54x_device::STATE_TIM);
		u16 const tcr = m_cpu->state_int(tms320c54x_device::STATE_TCR);
		osd_printf_info("TMS320C54x timer counter write: divider=%u tim=%04x tcr=%04x\n",
			divider, tim, tcr);
		expect(tim == 9 && tcr == (divider | ((divider - 2) << 6)) &&
			!m_cpu->state_int(tms320c54x_device::STATE_IFR),
			"SPRU131G TIM writes replace only TIM, preserving live read-only PSC without an intervening read");
		if (divider < 15) { start_timer_counter_write_case(divider + 1); return true; }
		osd_printf_info("TMS320C54x timer counter write: PASS cases=14\n");
		check_timer_reserved_fields();
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		start_timer_boundary_probe(0);
		return true;
	}
	if (m_phase >= 7020 && m_phase <= 7027)
	{
		unsigned const index = m_phase - 7020;
		u16 const marker = m_cpu->space(AS_DATA).read_word(0x0501);
		osd_printf_info("Staged timer final: case=%u cycles=%llu tim=%04x prd=%04x tcr=%04x imr=%04x ifr=%04x st1=%04x\n",
			index, (unsigned long long)m_cpu->pipeline_cycles(), unsigned(m_cpu->state_int(tms320c54x_device::STATE_TIM)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_PRD)),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_TCR)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IMR)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)));
		osd_printf_info("TMS320C54x staged timer observation: case=%u period=%u addressing=%s marker=%u foreground_fetches=%u mask_writes=%u vector_entries=%u pc=%06x sp=%04x illegal=%u gap_nops=%u silicon_phase_claim=0\n",
			index, index & 1 ? 256 : 1, (index & 3) >= 2 ? "direct" : "indirect", marker,
			m_timer_probe_foreground_fetches, m_timer_probe_masks, m_timer_probe_entries,
			unsigned(m_cpu->state_int(STATE_GENPC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)), index >= 4 ? 4 : 0);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Original timer stream executes the staged subset without atomic fallback");
		if (index >= 4 && !(index & 1))
		{
			expect(m_timer_probe_entries > 0, "Short-period NOP-gap control actually enters the PSHM/FRETE ISR");
			if ((index & 3) >= 2)
				expect(marker == 1 && m_timer_probe_masks == 1 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000,
				"Direct mask drains across the accepted ISR and far return without losing stack ownership");
		}
		m_cpu->set_pipeline_frontend(false);
		if (index < 7) { start_timer_boundary_probe(index + 1, true); return true; }
		osd_printf_info("TMS320C54x staged timer observation: complete cases=8 fidelity_acceptance=0\n");
		osd_printf_info("TMS320C54x core conformance: PASS\n");
		throw emu_fatalerror(0, "TMS320C54x core tests complete");
	}
	if (m_phase >= 7000 && m_phase <= 7003)
	{
		unsigned const index = m_phase - 7000;
		bool const short_period = !(index & 1);
		u16 const marker = m_cpu->space(AS_DATA).read_word(0x0501);
		osd_printf_info("TMS320C54x timer boundary probe: period=%u mask_addressing=%s mask_reached=%u pc=%06x sp=%04x imr=%04x ifr=%04x correctness_claim=0\n",
			short_period ? 1 : 0x0100, index >= 2 ? "direct" : "indirect", marker, unsigned(m_cpu->state_int(STATE_GENPC)),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_IMR)),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)));
		osd_printf_info("TMS320C54x timer boundary accounting: case=%u reload_fetch_cycle=%llu first_vector_cycle=%llu vector_entries=%u first_return_pc=%04x physical_latency_claim=0\n",
			index, static_cast<unsigned long long>(m_timer_probe_reload_cycle),
			static_cast<unsigned long long>(m_timer_probe_first_entry_cycle),
			m_timer_probe_entries, m_timer_probe_return_pc);
		expect(m_timer_probe_reload_cycle != 0, "Timer observation captures the reload opcode fetch, not an internal MMR tap");
		osd_printf_info("TMS320C54x timer boundary bus: case=%u reload_write_cycle=%llu mask_write_cycle=%llu reload_writes=%u mask_writes=%u physical_latency_claim=0\n",
			index, static_cast<unsigned long long>(m_timer_probe_write_cycle), static_cast<unsigned long long>(m_timer_probe_mask_cycle),
			m_timer_probe_writes, m_timer_probe_masks);
		osd_printf_info("TMS320C54x timer boundary foreground: case=%u opcode_address=010608 fetches=%u first_fetch_cycle=%llu mask_writes=%u pipeline_drain_claim=0\n",
			index, m_timer_probe_foreground_fetches,
			static_cast<unsigned long long>(m_timer_probe_foreground_first_cycle), m_timer_probe_masks);
		if (!short_period)
			expect(m_timer_probe_foreground_fetches == 1 && m_timer_probe_foreground_first_cycle,
				"Long-period control observes the post-reload foreground opcode fetch exactly once");
		expect(m_timer_probe_writes == 1 && (short_period || m_timer_probe_masks == 1),
			"Passive data-space taps observe the real TCR reload and completing IMR mask");
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Timer probe executes supported synthetic instructions");
		if (!short_period)
			expect(marker == 1 && !(m_cpu->state_int(tms320c54x_device::STATE_IMR) & 8),
			"Long-period control reaches the IMR mask before first timer expiry");
		if (index < 3) { start_timer_boundary_probe(index + 1); return true; }
		osd_printf_info("TMS320C54x timer boundary probe: complete cases=4 fidelity_acceptance=0\n");
		machine().schedule_exit();
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_bio_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 789 && m_phase < 817)
	{
		unsigned const index = m_phase - 789;
		u16 const op = bio_opcodes[index / 2];
		bool const take = BIT(op, 0) ? !m_bio_level : bool(m_bio_level);
		bool const execute = (op & 0xfd00) == 0xfd00;
		bool const returning = (op & 0xfc00) == 0xfc00 && !execute;
		bool const call = (op & 0xff00) == 0xf900;
		u16 const sp = 0x0400 + (take && returning ? 1 : take && call ? -1 : 0);
		expect_opcode(op, m_cpu->state_int(tms320c54x_device::STATE_A) ==
				(execute ? (take ? 2 : 0) : take ? 2 : 1) &&
				m_cpu->state_int(tms320c54x_device::STATE_B) ==
				(execute ? (take && BIT(op, 9) ? 3 : 0) : BIT(op, 9) ? 6 : 0) &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == sp &&
				m_cpu->state_int(tms320c54x_device::STATE_IDLE),
				"live BIO/NBIO branch, call, return and conditional execution at both pin levels");
		if (index + 1 < std::size(bio_opcodes) * 2)
		{
			start_bio_case(index + 1);
			return true;
		}
		osd_printf_info("TMS320C54x live BIO conformance: PASS variants=28\n");
		program.write_word(0x010930, 0xf803);
		program.write_word(0x010931, 0x0930);
		program.write_word(0x010932, 0xe807);
		program.write_word(0x010933, 0xf4e1);
		m_bio_level = 0;
		m_cpu->set_state_int(STATE_GENPC, 0x010930);
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		m_phase = 817;
		m_check_timer->adjust(attotime::from_usec(100));
		return true;
	}
	if (m_phase == 817)
	{
		expect(m_cpu->state_int(STATE_GENPC) == 0x010930 &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
			!m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),
			"BIO-low busy loop remains live without illegal opcode or forced PC");
		m_bio_level = 1;
		m_phase = 818;
		m_check_timer->adjust(attotime::from_usec(100));
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_bsp_interrupt_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 78000 && m_phase < 78036)
	{
		unsigned const index = (m_phase - 78000) / 4;
		unsigned const stage = (m_phase - 78000) % 4;
		unsigned const source = index < 4 ? 4 : 5;
		if (!stage)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(STATE_GENPC) == 0x010a01,
				"BSP source fixture waits in IDLE before completed-frame edge");
			// Input-line changes synchronize asynchronously. Hold execution,
			// then snapshot only after the delivered edge is visible in IFR.
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
			tms320c54x_bsp_state port;
			port.control_w(index == 8 ? 0x00c0 : source == 4 ? 0x0080 : 0x0040);
			if (source == 5) port.transmit_w(0x1234);
			u16 output = 0;
			const auto frame = port.duplex_frame(0x5678, output);
			expect(frame.transmit_ready_edge == (source == 5) &&
				frame.receive_ready_edge == (source == 4 || index == 8) &&
				(!frame.transmitted || output == 0x1234),
				"completed BSP frame supplies only the enabled ready transitions");
			if (frame.transmit_ready_edge) m_cpu->set_input_line(5, HOLD_LINE);
			if (frame.receive_ready_edge) m_cpu->set_input_line(4, HOLD_LINE);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		if (stage == 1)
		{
			expect(m_cpu->state_int(STATE_GENPC) == 0x010a01 &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) ==
					(index == 8 ? 0x30 : u16(1U << source)),
				"BSP source edge reaches IFR before snapshot and dispatch");
			m_saved_repeat.str(std::string());
			m_saved_repeat.clear();
			expect(machine().save().write_stream(m_saved_repeat) == STATERR_NONE,
				"save pending BSP interrupt sources before CPU dispatch");
			m_cpu->resume(SUSPEND_REASON_DISABLE);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		bool const enabled = index == 8 || (index & 1);
		bool const serviced = enabled && (index == 8 || !(index & 2));
		u32 const pc = serviced ? (index == 8 || source == 4 ? 0x010051 : 0x010055) :
			enabled ? 0x010a05 : 0x010a01;
		u16 const pending = index == 8 ? 0x20 : serviced ? 0 : u16(1U << source);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(STATE_GENPC) == pc &&
			m_cpu->state_int(tms320c54x_device::STATE_IFR) == pending &&
			m_cpu->space(AS_DATA).read_word(0x0501) == (enabled && !serviced ? 1 : 0),
			"BSP source vectors, masked pending state, INTM wake and simultaneous priority");
		if (stage == 2)
		{
			m_saved_repeat.clear();
			m_saved_repeat.seekg(0);
			expect(machine().save().read_stream(m_saved_repeat) == STATERR_NONE &&
				m_cpu->state_int(STATE_GENPC) == 0x010a01 &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) ==
					(index == 8 ? 0x30 : u16(1U << source)),
				"restore sleeping CPU and complete pending BSP source set");
			m_cpu->resume(SUSPEND_REASON_DISABLE);
			m_phase = 78000 + index * 4 + 3;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		if (index < 8) { start_bsp_interrupt_case(index + 1); return true; }
		osd_printf_info("TMS320C54x BSP interrupt sources: PASS cases=9 save_replay=9 handset_audio_claim=0\n");
		start_clkmd_write_case(0);
		return true;
	}
	if (m_phase >= 79000 && m_phase < 79016)
	{
		unsigned const index = m_phase - 79000;
		u16 const initial = u16((index << 12) | 0x0494);
		u16 const replacement = u16((~initial & 0xfff8) | 4);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE), "CLKMD fixture completes original STM/LDM/STL instructions");
		expect(data.read_word(0x0501) == (initial | 3) &&
			data.read_word(0x0502) == (initial | 3) &&
			data.read_word(0x0503) == (replacement | 3),
			"SPRU131G CLKMD protected fields require DIV mode; PLLNDIV remains writable and PLLSTATUS read-only");
		if (index < 15) { start_clkmd_write_case(index + 1); return true; }
		osd_printf_info("TMS320C54x CLKMD write protection: PASS cases=16 physical_clock_claim=0\n");
		start_pll_transition_case(); return true;
	}
	if (m_phase >= 79500 && m_phase <= 79509)
	{
		if (m_phase == 79500)
		{
			expect(data.read_word(0x58) == 0x77fe, "PLL lock counter remains pending at 200 us");
			m_saved_repeat.str(std::string()); m_saved_repeat.clear();
			expect(machine().save().write_stream(m_saved_repeat) == STATERR_NONE, "save pending PLL lock timer");
			data.write_word(0x58, 0);
			expect(data.read_word(0x58) == 0, "cancel pending PLL before restoring checkpoint");
			// Restore at the same scheduler timestamp: read_stream inside a
			// later timer callback cannot rewind that callback's cached time.
			m_saved_repeat.clear(); m_saved_repeat.seekg(0);
			expect(machine().save().read_stream(m_saved_repeat) == STATERR_NONE && data.read_word(0x58) == 0x77fe,
				"restore pending PLL request and its original deadline after cancellation");
			data.write_word(0x58, 0x77ff); // Repeating a request must retain its original deadline.
			m_phase = 79501;
			m_check_timer->adjust(attotime::from_ticks(65'383, 191'923'200) -
				attotime::from_usec(200) - attotime::from_ticks(1, 11'995'200)); return true;
		}
		if (m_phase == 79501)
		{
			expect(data.read_word(0x58) == 0x77fe, "PLLSTATUS still DIV one reference tick before exact 340.672727 us deadline");
			++m_phase;
			m_check_timer->adjust(attotime::from_ticks(1, 11'995'200) + attotime::from_nsec(1)); return true;
		}
		if (m_phase == 79502)
		{
			expect(data.read_word(0x58) == 0x77ff, "restored PLL switches at the same relative deadline");
			data.write_word(0x58, 0x6004);
			expect(data.read_word(0x58) == 0x77fd, "PLL fields remain protected throughout pending DIV switch");
			data.write_word(0x58, 0x97fb);
			expect(data.read_word(0x58) == 0x77ff, "opposite request cancels pending exit without modifying PLL fields");
			m_phase = 79505; m_check_timer->adjust(attotime::from_usec(2)); return true;
		}
		if (m_phase == 79505)
		{
			expect(data.read_word(0x58) == 0x77ff, "cancelled source-switch timer cannot change PLLSTATUS later");
			data.write_word(0x58, 0x6004);
			m_phase = 79506; m_check_timer->adjust(attotime::from_usec(1)); return true;
		}
		if (m_phase == 79506)
		{
			expect(data.read_word(0x58) == 0x77fc, "DIV exit completes without 4080-clock lock delay");
			data.write_word(0x58, 0x77ff);
			m_phase = 79507; m_check_timer->adjust(attotime::from_usec(1)); return true;
		}
		if (m_phase == 79507)
		{
			expect(data.read_word(0x58) == 0x77ff, "still-powered locked PLL re-entry skips lock delay");
			data.write_word(0x58, 0x6004);
			m_phase = 79508; m_check_timer->adjust(attotime::from_usec(1)); return true;
		}
		if (m_phase == 79508)
		{
			data.write_word(0x58, 0x97fb);
			expect(data.read_word(0x58) == 0x97fa, "new multiplier invalidates previous PLL lock");
			m_phase = 79509; m_check_timer->adjust(attotime::from_usec(341)); return true;
		}
		expect(data.read_word(0x58) == 0x97fb, "x10 PLL obtains a new lock before switching");
		m_cpu->set_pll_reference_clock(0);
		data.write_word(0x58, 0);
		m_cpu->resume(SUSPEND_REASON_DISABLE);
		osd_printf_info("TMS320C54x PLL transition timing: PASS lock_delay=1 protected_exit=1 cancel=1 retained_lock=1 multiplier_relock=1 save_replay=1 execution_rate_claim=0\n");
		start_clkmd_rate_case(); return true;
	}
	if (m_phase >= 79600 && m_phase <= 79604)
	{
		if (m_phase == 79600)
		{
			expect(m_cpu->clock() == 5'997'600, "core remains DIV/2 until PLLSTATUS changes");
			start_rate_dma();
			for (auto const &[reg, value] : std::array<std::pair<u16, u16>, 5>{{
				{4, 0x0040}, {6, 12}, {7, 0x2000}, {14, 0x0200}, {1, 0x0041}}})
			{
				m_idle_mcbsp->control_w(0, reg); m_idle_mcbsp->control_w(1, value);
			}
			m_idle_mcbsp->data_w(3, 0xa55a);
			m_phase = 79601;
			m_check_timer->adjust(attotime::from_ticks(1, 11'995'200) + attotime::from_nsec(1)); return true;
		}
		if (m_phase == 79601)
		{
			expect(m_cpu->clock() == 119'952'000 && m_idle_dma->clock() == 119'952'000 &&
				m_idle_mcbsp->clock() == 119'952'000, "PLL x10 is published only after the input-clock transition");
			expect(!m_idle_dma_completions && !data.read_word(0x0530), "clock change does not complete pending DMA early");
			m_phase = 79602; m_check_timer->adjust(attotime::from_usec(5)); return true;
		}
		if (m_phase == 79602)
		{
			expect(m_idle_dma_completions == 1 && data.read_word(0x0533) == 0x4444,
				"pending DMA follows selected system clock instead of its former DIV rate");
			expect(m_idle_mcbsp_words == 1, "internal McBSP word follows retimed system-clock generator");
			u16 const tim = data.read_word(0x24);
			expect(tim > 7'300 && tim < 7'500, "core timer retains count and advances at selected PLL rate");
			m_saved_repeat.str(std::string()); m_saved_repeat.clear();
			expect(machine().save().write_stream(m_saved_repeat) == STATERR_NONE, "save selected CPU and peripheral rates");
			m_cpu->set_unscaled_clock(13'000'000);
			expect(m_idle_dma->clock() == 13'000'000 && m_idle_mcbsp->clock() == 13'000'000,
				"core runtime clock publication reaches both consumers");
			m_saved_repeat.clear(); m_saved_repeat.seekg(0);
			expect(machine().save().read_stream(m_saved_repeat) == STATERR_NONE &&
				m_cpu->clock() == 119'952'000 && m_idle_dma->clock() == 119'952'000 &&
				m_idle_mcbsp->clock() == 119'952'000 && data.read_word(0x24) == tim,
				"restore selected rates without restarting core timer");
			m_cpu->set_follow_clkmd(false); m_cpu->set_pll_reference_clock(0);
			m_cpu->set_unscaled_clock(13'000'000);
			m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
			m_idle_mcbsp->reset();
			start_rate_dma(true);
			// System-rate changes must not retime DMA already on CLKIN.
			m_cpu->set_unscaled_clock(119'952'000);
			m_phase = 79603;
			m_check_timer->adjust(attotime::from_ticks(15, 3'250'000)); return true;
		}
		if (m_phase == 79603)
		{
			expect(!m_idle_dma_completions && data.read_word(0x0532) == 0x3333 && !data.read_word(0x0533),
				"input-clock DMA is still pending one CLKIN tick before four-word completion");
			m_cpu->set_unscaled_clock(13'000'000);
			m_phase = 79604;
			m_check_timer->adjust(attotime::from_ticks(1, 3'250'000) + attotime::from_nsec(1)); return true;
		}
		if (m_phase == 79604)
		{
			expect(m_idle_dma_completions == 1 && data.read_word(0x0533) == 0x4444,
				"input-clock DMA completes on CLKIN despite both system-rate changes");
			m_idle_dma->reset();
			m_cpu->set_pll_reference_clock(11'995'200); m_cpu->set_follow_clkmd(true);
			m_cpu->set_reset_clkmd(0);
			m_cpu->reset();
			expect(data.read_word(0x58) == 0 && m_cpu->clock() == 5'997'600 &&
				m_idle_dma->clock() == 5'997'600 && m_idle_mcbsp->clock() == 5'997'600,
				"explicit compatibility reset restores CLKMD and divider-derived CPU/peripheral rates");
			m_cpu->set_reset_clkmd(0, false);
			osd_printf_info("TMS320C54x CLKMD rates: PASS div_hz=5997600 pll_hz=119952000 timer=1 dma=1 input_clock_dma=1 internal_mcbsp=1 save_restore=1 reset_profile=1 physical_rate_claim=0\n");
			start_idle3_pll_relock_case(); return true;
		}
	}
	return false;
}

bool tms320c54x_test_state::check_peripheral_mmr_timing_audit_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 251210)
	{
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_pipeline_store_writes == 2, "Timing audit executes both register operations without interrupt or peripheral activity");
		u64 const observed = m_last_operand_cycle - m_first_operand_cycle;
		unsigned const specified = (m_mmr_timing_case / 4 == 4 ? 1 : 2) + unsigned((m_mmr_timing_case % 4) == 1 || (m_mmr_timing_case % 4) == 2);
		osd_printf_info("TMS320C54x MMR timing conformance: operation=%s destination=%s observed_cycles=%llu ti_cycles=%u matches=%u physical_latency_claim=0\n",
			std::array<char const *, 5>{"STM", "ANDM", "ORM", "ST", "LD"}[m_mmr_timing_case / 4], std::array<char const *, 4>{"AR2", "PRD", "005f", "scratch0060"}[m_mmr_timing_case % 4],
			(unsigned long long)observed, specified, observed == specified);
		expect(observed == specified, "CPU, peripheral and scratch memory costs match TI's instruction classes");
		m_pipeline_write_tap.remove();
		if (m_mmr_timing_case < 19) { start_peripheral_mmr_timing_audit(m_mmr_timing_case + 1); return true; }
		osd_printf_info("TMS320C54x peripheral MMR timing: PASS cases=20 families=STM,ANDM,ORM,ST,LD extra_cycles=1 rmw_double_charge=0 scratch_wait=0\n");
		start_pipeline_peripheral_case();
		return true;
	}
	if (m_phase == 251220)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		unsigned const family = m_pipeline_peripheral_case % 4;
		u64 const wait_cycle = family == 0 ? 5 : family == 3 ? 7 : 6;
		u64 const checkpoint = wait_cycle + unsigned(bool(m_pipeline_peripheral_case & 4));
		u64 const read_cycle = family == 0 ? 6 : family == 2 ? 7 : 8;
		u64 const write_cycle = family == 1 ? 7 : family == 2 ? 8 : 9;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Peripheral wait retains stages without an atomic fallback");
		expect(m_pipeline_operand_reads == unsigned(family != 1 && cycle >= read_cycle) &&
			m_pipeline_store_writes == unsigned(family != 0 && cycle >= write_cycle), "Peripheral stall neither repeats nor speculates bus side effects");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (family == 0 && cycle >= 7 ? 0x1234 : 0x5678), "Peripheral load retires its retained operand after the access wait");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save held peripheral stages or resumed access");
		}
		if (cycle == 10)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true;
				m_pipeline_operand_reads = unsigned(family != 1 && checkpoint >= read_cycle);
				m_pipeline_store_writes = unsigned(family != 0 && checkpoint >= write_cycle);
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE && m_cpu->pipeline_cycles() == checkpoint, "Restore paid wait and operand ownership");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = checkpoint + 1;
				m_phase = 251230;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
				return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 15) { start_pipeline_peripheral_case(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged peripheral waits: PASS cases=16 extra_cycles=1 rmw_double_charge=0 held_stage_restore=8 resumed_access_restore=8 duplicate_bus_accesses=0 silicon_phase_claim=0\n");
			start_pipeline_peripheral_case(9);
			// Adjacent STL/LD contend at the prototype's two bus stages.
			m_cpu->space(AS_PROGRAM).write_word(0x10601, 0x1025);
			m_phase = 251240;
			m_check_timer->adjust(attotime::from_ticks(12, m_cpu->clock()));
			return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 251230)
	{
		unsigned const family = m_pipeline_peripheral_case % 4;
		u64 const checkpoint = (family == 0 ? 5 : family == 3 ? 7 : 6) + unsigned(bool(m_pipeline_peripheral_case & 4));
		expect(m_cpu->pipeline_cycles() == checkpoint, "Settlement does not consume a restored peripheral wait");
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251220;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_mu4_timer_context_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 252110)
	{
		unsigned const variant = m_pipeline_peripheral_case;
		bool const sxm = BIT(variant, 0), matched = BIT(variant, 1);
		unsigned const page = BIT(variant, 2) ? 3 : 0;
		unsigned const checkpoint = std::array<unsigned, 6>{28, 36, 55, 85, 130, 200}[variant / 8];
		u64 const cycle = m_cpu->pipeline_cycles();
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Timer-driven original context executes without a synthetic handler jump");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_pipeline_store_case = m_pipeline_store_writes; m_pipeline_long_case = m_pipeline_extensions_reads;
			m_pipeline_rmw_case = m_pipeline_drained_fetches; m_pipeline_return_case = m_vector_reads;
			m_pipeline_indirect_case = m_pipeline_operand_reads; m_pipeline_register_writes = m_vector_acks; m_pipeline_branch_case = m_fast_stack_reads;
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save genuine timer entry, original context and far-return foreground settlement");
		}
		if (cycle == 400)
		{
			{
				auto const disable = machine().disable_side_effects();
				expect(m_vector_acks == 1 && m_pipeline_extensions_reads == 1 && m_pipeline_drained_fetches == 1 && m_vector_reads == 1 &&
					m_pipeline_store_writes == 18 && m_fast_stack_reads == 2 && m_pipeline_operand_reads == 1 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_XPC) == page &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa61b && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x4200 | (sxm ? 0x100 : 0)) &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x77 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x66 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x1357 && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x2468 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x369c && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x8888 &&
					m_cpu->space(AS_DATA).read_word(0x7c) == 0x1357 && m_cpu->space(AS_DATA).read_word(0x7d) == 0x2469 &&
					m_cpu->space(AS_DATA).read_word(0x511) == (matched ? 0x470 : 0) && m_cpu->space(AS_DATA).read_word(0x513) == 0x4f4,
					"Original timer ISR restores hardware return page/PC and foreground retirement with one worker execution");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_store_writes = m_pipeline_store_case; m_pipeline_extensions_reads = m_pipeline_long_case;
				m_pipeline_drained_fetches = m_pipeline_rmw_case; m_vector_reads = m_pipeline_return_case; m_pipeline_operand_reads = m_pipeline_indirect_case;
				m_vector_acks = m_pipeline_register_writes; m_fast_stack_reads = m_pipeline_branch_case;
				if (!m_vector_acks) m_timer_probe_return_pc = 0;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore original timer ISR without duplicate vector, context or return ownership");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = checkpoint + 1; m_phase = 252120;
				m_check_timer->enable(true); return true;
			}
			m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_vector_prefetch_tap.remove();
			m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_pipeline_frontend(false);
			if (variant < 47) { start_pipeline_mu4_timer_context(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged original MU4 timer ISR: PASS cases=48 original_words=94 foreground_pages=2 sxm_states=2 cmpr_outcomes=2 restore=48 timer_vector=1 original_frete=1 foreground_retired=1\n");
			start_pipeline_mu4_recurring_timer(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252120)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252110;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252130)
	{
		unsigned const variant = m_pipeline_peripheral_case;
		bool const sxm = BIT(variant, 0), matched = BIT(variant, 1), short_period = BIT(variant, 3);
		unsigned const page = BIT(variant, 2) ? 3 : 0;
		u64 const cycle = m_cpu->pipeline_cycles(); unsigned const point = variant / 16;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && cycle < 6000,
			"Recurring original timer context remains bounded without reseeding worker state");
		if (m_vector_acks == 1 && m_fast_stack_reads == 2 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 &&
			!BIT(m_cpu->state_int(tms320c54x_device::STATE_ST1), 11) && m_recurring_irq_first_return_writes == ~0U)
			m_recurring_irq_first_return_writes = m_pipeline_operand_reads;
		bool const save_point = point == 0 ? cycle == 28 : point == 1 ? m_vector_acks == 1 :
			point == 2 ? m_fast_stack_reads == 2 : point == 3 ? m_vector_acks == 3 : point == 4 ? m_fast_stack_reads == 6 : m_vector_acks == 6;
		if (save_point && !m_irq_raised && !m_pipeline_replay)
		{
			m_irq_raised = true; m_recurring_irq_checkpoint_cycle = cycle;
			m_recurring_irq_snapshot = {m_vector_acks, m_pipeline_extensions_reads, m_pipeline_drained_fetches, m_vector_reads,
				m_fast_stack_reads, m_pipeline_store_writes, m_pipeline_operand_reads, m_timer_probe_return_pc, m_recurring_irq_first_return_writes};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save recurring timer and evolving original worker at a real interrupt boundary");
		}
		bool const returned = m_vector_acks == 8 && m_vector_reads == 8 && m_fast_stack_reads == 16 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && !BIT(m_cpu->state_int(tms320c54x_device::STATE_ST1), 11) &&
			m_cpu->state_int(tms320c54x_device::STATE_XPC) == page;
		if (returned)
		{
			{
				auto const disable = machine().disable_side_effects();
				unsigned const bodies = matched ? 1 : 8;
				u16 pointer = 0x470; for (unsigned i = 0; i < bodies; ++i) pointer = ((pointer + 4) & 0xff0f) | 0x00f0;
				expect(m_irq_raised && m_pipeline_extensions_reads == 8 && m_pipeline_drained_fetches == 8 &&
					m_pipeline_store_writes == 8 * 17 + bodies && m_recurring_irq_first_return_writes != ~0U &&
					(short_period || m_pipeline_operand_reads > m_recurring_irq_first_return_writes) &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa61b && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x4200 | (sxm ? 0x100 : 0)) &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x77 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2233445566ULL &&
					m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x1357 && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x2468 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x369c && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x8888 &&
					m_cpu->space(AS_DATA).read_word(0x7c) == 0x1357 && m_cpu->space(AS_DATA).read_word(0x7d) == 0x2470 &&
					m_cpu->space(AS_DATA).read_word(0x512) == 0xff0f + bodies * 2 && m_cpu->space(AS_DATA).read_word(0x513) == pointer &&
					m_cpu->space(AS_DATA).read_word(0x511) == (matched ? 0x470 : 0) && m_cpu->space(AS_DATA).read_word(0x700) == m_pipeline_operand_reads,
					"Recurring timer preserves caller context while original control/mask state evolves without forcing");
			}
			osd_printf_info("MU4 recurring timer observation: case=%u replay=%u prd=%u returns=8 cycle=%llu foreground_writes=%u first_return_writes=%u pending=%04x\n",
				variant, unsigned(m_pipeline_replay), short_period ? 1 : 255, (unsigned long long)cycle, m_pipeline_operand_reads, m_recurring_irq_first_return_writes,
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)));
			if (!m_pipeline_replay)
			{
				m_recurring_irq_end_cycle = cycle; m_recurring_irq_end_writes = m_pipeline_operand_reads; m_pipeline_replay = true;
				m_vector_acks = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				m_pipeline_drained_fetches = m_recurring_irq_snapshot[2]; m_vector_reads = m_recurring_irq_snapshot[3];
				m_fast_stack_reads = m_recurring_irq_snapshot[4]; m_pipeline_store_writes = m_recurring_irq_snapshot[5];
				m_pipeline_operand_reads = m_recurring_irq_snapshot[6]; m_timer_probe_return_pc = m_recurring_irq_snapshot[7];
				m_recurring_irq_first_return_writes = m_recurring_irq_snapshot[8];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				// Mark the executing check timer modified before restoring it.
				// Resume at its restored deadline, not stale callback time.
				m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore recurring interrupts without duplicate timer, stack or foreground effects");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = m_recurring_irq_checkpoint_cycle + 1; m_phase = 252140;
				m_check_timer->enable(true); return true;
			}
			expect(cycle == m_recurring_irq_end_cycle && m_pipeline_operand_reads == m_recurring_irq_end_writes,
				"Recurring timer replay reproduces the exact eighth-return cycle and foreground effects");
			m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_vector_prefetch_tap.remove();
			m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_pipeline_frontend(false);
			if (variant < 95) { start_pipeline_mu4_recurring_timer(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged recurring MU4 timer: PASS cases=96 returns_per_leg=8 periods=2 foreground_pages=2 sxm_states=2 control_outcomes=2 restore=96 exact_return_cycle=1 firmware_reseeding=0\n");
			start_pipeline_absolute_load(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252140)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252130;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_idle_sp_tail_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 252210)
	{
		u64 const cycle = m_cpu->pipeline_cycles(); unsigned const checkpoint = std::array<unsigned, 4>{5, 7, 8, 9}[m_pipeline_peripheral_case];
		auto const reads_at = [](u64 clock) { return unsigned(clock >= 5) + unsigned(clock >= 8); };
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == reads_at(cycle) && m_pipeline_register_writes == unsigned(cycle >= 5) && m_pipeline_extensions_reads == unsigned(cycle >= 10) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (cycle >= 7 ? 0x1001 : cycle >= 5 ? 0x1000 : 0xffe) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle >= 14 ? 0x55 : 0x77) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800,
			"Original POPM SP/NOP/NOP/RET suffix restores and consumes the intended caller frame");
		if (cycle == 5 || cycle == 8)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_DATA).write_word(cycle == 5 ? 0xffe : 0x1000, 0xdead);
		}
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save original idle SP restoration and captured near-return word");
		}
		if (cycle == 16)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = reads_at(checkpoint); m_pipeline_register_writes = unsigned(checkpoint >= 5); m_pipeline_extensions_reads = unsigned(checkpoint >= 10);
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore original idle suffix without a second SP publication or return-word read");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = checkpoint + 1; m_phase = 252220;
				m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 3) { start_pipeline_idle_sp_tail(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged original idle SP tail: PASS cases=4 original_words=4 restore=4 duplicate_reads=0 duplicate_sp_writes=0\n");
			start_pipeline_idle1(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252220)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252210;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252230)
	{
		bool const modes = !strcmp(machine().options().bios(), "idle-modes");
		unsigned const variant = m_pipeline_peripheral_case;
		unsigned const kind = pipeline_idle_kind(variant);
		bool const blocked = kind == 1 || kind == 4;
		unsigned const source = kind == 3 || kind == 4 ? 3 : 0;
		u32 const base = BIT(variant / (modes ? 4 : 6), 0) ? 0x20000 : 0;
		unsigned const tick = ++m_idle_fixture_tick;
		unsigned const checkpoint = std::array<unsigned, 4>{4, 10, 18, 80}[modes ? (variant / 8) % 4 : variant / 12];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Staged IDLE1 barriers and wake do not decode suspended younger words");
		if (tick >= 6 && tick < 20)
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->pipeline_cycles() == 6 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000, "IDLE1 halts CPU execution while pending masked sources remain inert");
		if (kind == 2 && tick == 12) { m_cpu->set_input_line(1, ASSERT_LINE); m_cpu->set_input_line(1, CLEAR_LINE); }
		if (modes && tick == 8) m_pipeline_expected_cycle = m_cpu->state_int(tms320c54x_device::STATE_TIM);
		if (modes && tick == 16)
			expect(variant / 32 ? m_cpu->state_int(tms320c54x_device::STATE_TIM) == m_pipeline_expected_cycle :
				m_cpu->state_int(tms320c54x_device::STATE_TIM) < m_pipeline_expected_cycle,
				"Staged IDLE2/3 stop the timer while IDLE1 retains its running clock");
		if (tick == 16)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0xe844);
			if (source == 3) expect(m_cpu->state_int(tms320c54x_device::STATE_TIM) < 31, "IDLE1 leaves its timer running while the CPU is stopped");
		}
		if (source != 3 && tick == 20)
		{
			int const line = kind == 5 ? INPUT_LINE_NMI : 0;
			m_cpu->set_input_line(line, ASSERT_LINE); m_cpu->set_input_line(line, CLEAR_LINE);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick;
			m_pipeline_register_writes = m_vector_acks; m_pipeline_store_case = m_pipeline_store_writes;
			m_pipeline_branch_case = m_pipeline_operand_reads; m_pipeline_stm_case = m_pipeline_extensions_reads;
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save IDLE1 decode barrier, asleep state, pending input and resumed execution");
		}
		if (tick == 100)
		{
			expect(!m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_extensions_reads == 2 &&
				m_vector_acks == unsigned(!blocked) && m_pipeline_store_writes == (blocked ? 0 : 2) && m_pipeline_operand_reads == (blocked ? 0 : 2) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x44 && m_cpu->state_int(tms320c54x_device::STATE_B) == (blocked ? 0 : 0x55) &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_XPC) == base >> 16 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == (blocked ? 0x800 : 0) &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == (kind == 2 ? 2 : blocked ? 1 << source : 0),
				"Enabled wake resumes directly under INTM or services a real ISR before fresh continuation retirement");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_vector_acks = m_pipeline_register_writes; m_pipeline_store_writes = m_pipeline_store_case;
				m_pipeline_operand_reads = m_pipeline_branch_case; m_pipeline_extensions_reads = m_pipeline_stm_case;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore IDLE1 continuation and timer phase without a synthetic wake");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252240; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove();
			m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_pipeline_frontend(false);
			if (variant < (modes ? 95U : 47U)) { start_pipeline_idle1(variant + 1); return true; }
			if (modes)
			{
				osd_printf_info("TMS320C54x staged idle modes: PASS cases=96 modes=3 wake_profiles=4 pages=2 restore=96 timer_stop=1 suspended_younger_words=1 real_irq_return=1 physical_clock_claim=0\n");
				fatalerror("TMS320C54x isolated staged idle modes complete");
			}
			osd_printf_info("TMS320C54x staged IDLE1: PASS cases=48 wake_profiles=6 source_pages=2 restore=48 suspended_younger_words=1 timer_runs=1 real_irq_return=1\n");
			start_pipeline_original_idle(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252240)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252230;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252250)
	{
		unsigned const variant = m_pipeline_peripheral_case;
		bool const service = variant >= 8, initialize = variant >= 16, skip = !initialize && (service || BIT(variant, 0));
		u16 const sp = 0x1000 + (service ? BIT(variant, 0) : BIT(variant, 1));
		unsigned const tick = ++m_idle_fixture_tick;
		unsigned const checkpoint = initialize ? (variant >= 18 ? 140 : 60) : service ? std::array<unsigned, 4>{80, 110, 140, 300}[(variant - 8) / 2] : BIT(variant, 2) ? 110 : 80;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Original 44-word idle call uses only validated staged forms");
		if (!initialize && tick >= 80 && tick < 120)
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_extensions_reads == 0 &&
				m_pipeline_store_writes == (skip ? 0 : 5) && m_cpu->state_int(tms320c54x_device::STATE_IMR) == (service ? 8 : skip ? 0x40 : 0x8c0),
				"Original setup masks its short-period timer before sleeping, without executing its continuation");
		if (!service && tick == 120) { m_cpu->set_input_line(6, ASSERT_LINE); m_cpu->set_input_line(6, CLEAR_LINE); }
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_pipeline_store_case = m_pipeline_store_writes;
			m_recurring_irq_snapshot = {m_vector_acks, m_pipeline_operand_reads, m_pipeline_drained_fetches, m_vector_reads, m_pipeline_extensions_reads, m_timer_probe_return_pc};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save original idle call after firmware-owned setup and before external wake");
		}
		if (tick == (initialize ? 1000 : service ? 400 : 200))
		{
			if (initialize)
			{
				osd_printf_info("Original idle zero/TINT observation: case=%u replay=%u writes=%u acks=%u vectors=%u handlers=%u returns=%u idle=%llu pc=%04llx rtn=%04llx sp=%04llx imr=%04llx ifr=%04llx interrupted_pc=%04x\n",
					variant, unsigned(m_pipeline_replay), m_pipeline_store_writes, m_vector_acks, m_pipeline_operand_reads, m_pipeline_drained_fetches, m_vector_reads,
					(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_IDLE), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_PC),
					(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_RTN), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_SP),
					(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_IMR), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_IFR), m_timer_probe_return_pc);
				expect(m_pipeline_extensions_reads == 0 && m_pipeline_store_writes == 5 && m_vector_acks == 1 &&
					m_pipeline_operand_reads == 1 && m_pipeline_drained_fetches == 1 && m_vector_reads == 1 && m_timer_probe_return_pc == 0x98e4 &&
					m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x98ea &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0xffa && m_cpu->state_int(tms320c54x_device::STATE_IMR) == 0x8c0 &&
					m_cpu->state_int(tms320c54x_device::STATE_IFR) == 8 && m_cpu->space(AS_DATA).read_word(0xa531) == 1,
					"Original zero path drains its mask before timer entry, returns at ORM, commits its flag and sleeps without forced continuation");
			}
			else
			{
			osd_printf_info("Original idle observation: case=%u replay=%u idle=%llu caller_fetch=%u sp=%04llx ar1=%04llx a=%llx b=%llx st1=%04llx xpc=%llx ifr=%04llx\n",
				variant, unsigned(m_pipeline_replay), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_IDLE), m_pipeline_extensions_reads,
				(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_SP), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_AR1),
				(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B),
				(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_ST1), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_XPC),
				(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_IFR));
			expect(!m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_extensions_reads == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == sp + 1 && m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x1357 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x55 && m_cpu->state_int(tms320c54x_device::STATE_B) == sp &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == (service ? 0 : 0x800) && m_cpu->state_int(tms320c54x_device::STATE_XPC) == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == (service ? 0 : skip ? 0x40 : 0x48) &&
				(!service || (m_vector_acks == 1 && m_pipeline_operand_reads == 1 && m_pipeline_drained_fetches == 1 && m_vector_reads == 1)),
				"Original idle wake, FRET and wrapper restore caller SP/AR1/CPL, retaining unserviced enabled IRQ under INTM");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_store_writes = m_pipeline_store_case;
				m_vector_acks = m_recurring_irq_snapshot[0]; m_pipeline_operand_reads = m_recurring_irq_snapshot[1];
				m_pipeline_drained_fetches = m_recurring_irq_snapshot[2]; m_vector_reads = m_recurring_irq_snapshot[3]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[4];
				m_timer_probe_return_pc = m_recurring_irq_snapshot[5];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore original idle call with retained timer and stack state");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252260; m_check_timer->enable(true); return true;
			}
			m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_vector_prefetch_tap.remove();
			m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_pipeline_frontend(false);
			if (variant == 7) osd_printf_info("TMS320C54x staged original idle call: PASS cases=8 original_words=44 setup_paths=2 stack_alignments=2 restore=8 external_wake=1 isr_claim=0\n");
			if (variant == 15) osd_printf_info("TMS320C54x staged original idle timer wake: PASS cases=8 idle_words=44 isr_words=94 stack_alignments=2 restore=8 original_vector=1 original_frete=1\n");
			if (variant < 19) { start_pipeline_original_idle(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged original idle zero/TINT: PASS cases=4 original_words=138 stack_alignments=2 restore=4 mask_drained=1 forced_continuation=0 native_profile_claim=0\n");
			start_pipeline_pmst(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252260)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252250;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_peripheral_logic_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 252470)
	{
		unsigned const variant = m_pipeline_peripheral_case, operation = variant % 3, mode = (variant / 3) % 3;
		u16 const operand = std::array<u16, 3>{0xf0, 0x8000, 0xffff}[(variant / 9) % 3];
		bool const b = (variant / 27) % 2, sxm = (variant / 54) % 2;
		unsigned const tick = ++m_idle_fixture_tick, checkpoint = 7 + variant / 216;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Peripheral logical forms use the shared single-access-clock path");
		if (tick < 9) expect(!m_pipeline_operand_reads, "The paid wait does not issue an early peripheral read");
		if (tick == 9) { auto const disable = machine().disable_side_effects(); m_cpu->set_state_int(tms320c54x_device::STATE_PRD, 0x1234); }
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_pipeline_branch_case = m_pipeline_operand_reads;
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save peripheral logic before wait, held stage, retained read or publication");
		}
		if (tick == 20)
		{
			u64 const expected = operation == 0 ? 0x55 & operand : operation == 1 ? 0x55 | operand : 0x55 ^ operand;
			auto const disable = machine().disable_side_effects();
			u16 const opcode = std::array<u16, 3>{0x1800, 0x1a00, 0x1c00}[operation] | (b ? 0x100 : 0) | (mode == 0 ? 0x25 : mode == 1 ? 0x81 : 0x91);
			expect_opcode(opcode, m_pipeline_operand_reads == 1 && m_cpu->state_int(b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == expected &&
				m_cpu->state_int(b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == 0x2233445566ULL && m_cpu->state_int(tms320c54x_device::STATE_AR1) == (mode == 2 ? 0x26 : 0x25) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x600 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (sxm ? 0x100 : 0)) &&
				m_cpu->state_int(tms320c54x_device::STATE_PRD) == 0x1234,
				"Peripheral AND/OR/XOR zero-extend the sampled word, preserve status and modify an indirect source only once");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_pipeline_branch_case;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore paid peripheral wait and operand without duplicate read or modification");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252480; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 863) { start_pipeline_peripheral_logic(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged peripheral logic: PASS cases=864 operations=AND,OR,XOR addressing_modes=3 operands=3 accumulators=2 sxm_states=2 pages=2 restore=864 extra_cycles=1 duplicate_reads=0 peripheral_writes=0\n");
			if (!strcmp(machine().options().bios(), "logic-peripheral")) throw emu_fatalerror(0, "TMS320C54x isolated peripheral memory logic complete");
			start_pipeline_port_write(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252480)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252470;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252490)
	{
		unsigned const variant = m_pipeline_peripheral_case, mode = variant % 5;
		u16 const value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 5) % 4];
		bool const sxm = (variant / 20) % 2, absolute = mode == 0 || mode == 1 || mode == 4;
		u32 const base = (variant / 40) % 2 ? 0x20000 : 0;
		u16 const source = mode == 0 ? 8 : mode == 1 || mode == 2 ? 0x500 : mode == 3 ? 0x1020 : 0x25;
		unsigned const read_cycle = absolute ? mode == 4 ? 8 : 7 : 6;
		unsigned const tick = ++m_idle_fixture_tick, checkpoint = std::array<unsigned, 4>{4, 6, 7, 9}[variant / 80];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Staged PORTW owns separate source-data and destination-I/O transactions");
		if (tick == (absolute ? 5 : 4))
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(base + (absolute ? 0x602 : 0x601), 0x125);
			if (absolute) m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0x510);
		}
		if (tick == read_cycle)
		{
			auto const disable = machine().disable_side_effects();
			if (mode == 0) m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
			else if (mode == 4) m_cpu->set_state_int(tms320c54x_device::STATE_PRD, 0x1234);
			else m_cpu->space(AS_DATA).write_word(source, 0x1234);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads, m_port_writes};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save PORTW literal assembly, source Read, paid wait or I/O publication");
		}
		if (tick == 20)
		{
			expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_extensions_reads == (absolute ? 2 : 1) && m_port_writes == 1 && m_first_port_value == value &&
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == (mode == 2 ? 0x501 : 0x500) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (sxm ? 0x100 : 0) | (mode == 3 ? 0x4000 : 0)),
				"PORTW retires the original sampled word/literals through a real I/O handler without duplicate access or auxiliary modification");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; m_port_writes = m_recurring_irq_snapshot[3];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore PORTW without re-reading its source or publishing another I/O write");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252500; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 319) { start_pipeline_port_write(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged PORTW: PASS cases=320 source_modes=5 values=4 sxm_states=2 pages=2 restore=320 retained_literals=1 duplicate_source_reads=0 duplicate_io_writes=0\n");
			start_pipeline_direct_immediate_store(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252500)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252490;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_peripheral_long_store_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 252650)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const peripheral = !strcmp(machine().options().bios(), "st-long-periph");
		auto const f = peripheral ? peripheral_long_store_fixture(variant) : immediate_long_store_fixture(variant);
		u32 const base = (variant / 768) % 2 ? 0x20000 : 0;
		unsigned const checkpoint = std::array<unsigned, 4>{4, 5, 7, 8}[variant / 1536];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Long immediate ST keeps partial and complete Decode operands separate");
		if (tick == 4)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0xdead);
		}
		if (tick == 5)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(base + 0x602, 0xbeef);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, m_cpu->state_int(tms320c54x_device::STATE_ST1) ^ 0x4000);
		}
		if (tick == 7)
		{
			u16 const st0 = f.compatibility && f.reg ? (f.st0 & ~0xe000) | (f.reg << 13) : f.st0;
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.selected) == f.next_ar && m_cpu->state_int(tms320c54x_device::STATE_ST0) == st0,
				"Long immediate ST applies only its encoded AR/compatibility modification once");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.selected, 0x2000);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 32);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, (st0 & ~0x1ff) | 9);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_store_writes, m_pipeline_extensions_reads};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save partial/complete long ST Decode or retained address/publication");
		}
		if (tick == 20)
		{
			{
				auto const disable = machine().disable_side_effects();
				expect(m_pipeline_store_writes == 1 && m_pipeline_extensions_reads == 2 && m_cpu->space(AS_DATA).read_word(f.address) == f.value &&
					m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.selected) == 0x2000,
					"Long ST does not resample poisoned operands or overwrite a later pointer update");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_store_writes = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore long ST without operand/address resampling or duplicate write");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252660; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 6143) { start_pipeline_long_immediate_store(variant + 1); return true; }
			if (peripheral)
			{
				osd_printf_info("TMS320C54x staged long peripheral ST: PASS cases=6144 restore=6144 registers=8 forms=offset,preincrement values=4 compatibility=2 offsets=3b,38,49 spans=peripheral,RAM pages=2 snapshots=4 literal_fetches=2 reads=0 writes=1 peripheral_extra=1 ram_extra=0 address_retained=1 device_semantics=unvalidated silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated long peripheral ST complete");
			}
			osd_printf_info("TMS320C54x staged long immediate ST: PASS cases=6144 forms=offset,preincrement,circular registers=8 offsets=4 values=4 compatibility=0,1 pages=2 restore=6144 duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0\n");
			if (!strcmp(machine().options().bios(), "long-mvkd")) { start_pipeline_long_accumulator_store(); return true; }
			start_pipeline_bio_branch(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252660)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252650;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252670)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, transition = (variant / 8) % 4;
		bool const low = variant % 2, delayed = (variant / 2) % 2, initial = (variant / 4) % 2;
		bool const sampled = transition == 1 || transition == 2 ? !initial : initial, taken = low ? !sampled : sampled;
		unsigned const checkpoint = std::array<unsigned, 4>{4, 5, 6, 8}[variant / 64];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "BIO branch retains a physical input predicate, not accumulator state");
		if ((transition == 1 && tick == 4) || (transition == 2 && tick == 5) || (transition == 3 && tick == 6)) m_bio_level = !initial;
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_register_writes, m_pipeline_extensions_reads, m_pipeline_operand_reads, unsigned(m_bio_level)};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save BIO branch before/after line sample or delay retirement");
		}
		if (tick == 20)
		{
			bool const success = m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_register_writes == 1 && m_pipeline_extensions_reads == 1 &&
				m_pipeline_operand_reads == unsigned(taken) && m_cpu->state_int(tms320c54x_device::STATE_A) == (taken ? 0x55 : 0x11) &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == (delayed || !taken ? 0x22 : 0) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800;
			if (!success) osd_printf_info("Staged BIO failure: case=%u replay=%u taken=%u cycle=%llu idle=%u a=%010llx b=%010llx line_reads=%u literal_reads=%u target_reads=%u\n",
				variant, m_pipeline_replay, taken, (unsigned long long)m_cpu->pipeline_cycles(), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IDLE)),
				(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B),
				m_pipeline_register_writes, m_pipeline_extensions_reads, m_pipeline_operand_reads);
			expect(success,
				"BIO/NBIO use active-low/high Read sample and preserve actual plain/delayed execution despite later pin changes");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_register_writes = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				m_pipeline_operand_reads = m_recurring_irq_snapshot[2]; m_bio_level = m_recurring_irq_snapshot[3];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore BIO branch without a duplicate callback or target fetch");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252680; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 255) { start_pipeline_bio_branch(variant + 1); return true; }
			m_bio_level = 1;
			osd_printf_info("TMS320C54x staged BIO branches: PASS cases=256 forms=BC,BCD conditions=BIO,NBIO levels=0,1 transitions=4 pages=2 restore=256 line_sample=Read duplicate_callbacks=0 duplicate_fetches=0 silicon_phase_claim=0\n");
			if (!strcmp(machine().options().bios(), "bio-branch")) throw emu_fatalerror(0, "TMS320C54x isolated BIO branches complete");
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			start_pipeline_mmr_move(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252680)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252670;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_port_read_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 252790)
	{
		unsigned const variant = m_pipeline_peripheral_case, mode = variant % 13;
		u16 const value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 13) % 4];
		bool const absolute = mode <= 8, compiler = mode == 10;
		u16 const target = mode < 8 ? 8 + mode : compiler ? 0x1020 : mode == 9 ? 0x4a0 : 0x500;
		u32 const base = (variant / 52) % 2 ? 0x20000 : 0;
		unsigned const tick = ++m_idle_fixture_tick, read_cycle = absolute ? 7 : 6;
		unsigned const checkpoint = std::array<unsigned, 4>{3, 5, 7, 8}[variant / 104];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "PORTR has retained staged ownership");
		if (tick == read_cycle)
			m_cpu->space(AS_IO).write_word(0x125, value ^ 0xffff); // Retained sample must survive a changing peripheral.
		if (tick == (absolute ? 5 : 4))
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0xf495);
			if (absolute) m_cpu->space(AS_PROGRAM).write_word(base + 0x602, 0xf495);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save PORTR at partial Decode, Access, Read or publication");
		}
		if (tick == 20)
		{
			u64 a = 0x1212345678ULL, b = 0x349abcdef0ULL;
			if (mode < 6)
			{
				unsigned const shift = (mode % 3) * 16;
				u64 const mask = (mode % 3 == 2 ? 0xffULL : 0xffffULL) << shift;
				u64 &destination = mode < 3 ? a : b;
				destination = (destination & ~mask) | ((u64(value) << shift) & mask);
			}
			{
				auto const disable = machine().disable_side_effects();
				expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_extensions_reads == (absolute ? 2 : 1) &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == a && m_cpu->state_int(tms320c54x_device::STATE_B) == b &&
					m_cpu->state_int(tms320c54x_device::STATE_T) == (mode == 6 ? value : 0x1234) &&
					m_cpu->state_int(tms320c54x_device::STATE_TRN) == (mode == 7 ? value : 0x5678) &&
					m_cpu->state_int(tms320c54x_device::STATE_AR1) == (mode == 11 ? 0x501 : mode == 12 ? 0x4ff : 0x500) &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x609 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x900 | (compiler ? 0x4000 : 0)) &&
					(mode < 8 || m_cpu->space(AS_DATA).read_word(target) == value),
					"PORTR preserves status/unselected lanes and replays one port read and one data write");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore PORTR without duplicate port read or data write");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252800; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_operand_data_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 415) { start_pipeline_port_read(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged PORTR: PASS cases=416 destination_modes=13 values=4 pages=2 restore=416 retained_literals=1 duplicate_port_reads=0 duplicate_data_writes=0 destination_reads=0\n");
			start_pipeline_long_accumulator_store(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252800)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252790;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_clkmd_move_edge_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 62070)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		unsigned const checkpoint = 2 + variant / 12;
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE,"Save rejected CLKMD move before its unsupported composition");
		}
		if (tick == 12)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && !m_pipeline_operand_reads && !m_pipeline_store_writes &&
				m_cpu->state_int(tms320c54x_device::STATE_AR6) == (variant % 3 == 1 ? 0x1420 : 0x1406),
				"Repeated/delayed CLKMD moves fail before operand reads, writes or source pointer effects");
			{ auto const disable = machine().disable_side_effects(); expect(!m_cpu->space(AS_DATA).read_word(0x58),"Rejected clock move leaves CLKMD unchanged"); }
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE,"Restore pending CLKMD exclusion");
				m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_phase = 62071; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 23) { start_clkmd_move_edge(variant+1); return true; }
			osd_printf_info("TMS320C54x CLKMD MVDK exclusions: PASS cases=24 restore=24 contexts=RPT,BD_delay source_forms=3 pages=2 snapshots=2 reads=0 writes=0 pointer_changes=0 clock_changes=0 hardware_trap_claim=0\n");
			throw emu_fatalerror(0,"TMS320C54x isolated CLKMD MVDK exclusions complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true;
	}
	if (m_phase == 62071) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62070; m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_mcbsp_control_sequence_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 253950)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, snapshot = variant / 4;
		bool const checkpoint = snapshot == 0 ? tick == 4 : snapshot == 1 ? m_pipeline_store_writes == 1 && !m_pipeline_operand_reads : snapshot == 2 ? m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 : m_pipeline_store_writes == 2;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Actual McBSP control sequence remains staged");
		if (checkpoint && !m_pipeline_replay && !m_idle_fixture_saved_tick) { m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_drained_fetches, m_idle_mcbsp_words}; m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save CPU and real McBSP selection/read/reset sequence"); }
		if (tick == 32)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 2 && m_pipeline_drained_fetches == 1 && m_idle_mcbsp_words == 0 && m_idle_mcbsp->control_r(0) == 1 && m_idle_mcbsp->control_r(1) == 0, "CPU retains the pre-busy status, clears XRST and observes the device's real reset state");
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0 && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xabcdef1234ULL && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1600 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (variant & 2 ? 0x900 : 0x800), "Selector/mask composition preserves CPU address, accumulator and status ownership");
			if (!m_pipeline_replay) { expect(m_idle_fixture_saved_tick != 0, "Each semantic McBSP snapshot boundary must actually occur"); m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_drained_fetches = m_recurring_irq_snapshot[2]; m_idle_mcbsp_words = m_recurring_irq_snapshot[3]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore CPU and McBSP without duplicate device transactions"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253960; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 15) { start_mcbsp_control_sequence(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged McBSP control: PASS cases=16 restore=16 pages=2 sxm_states=2 snapshots=4 selector_writes=1 status_reads=1 reset_writes=1 captured_ready=1 busy_after_read=1 device_reset=1 physical_mapping=unvalidated silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated McBSP control complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253960) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253950; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_ldu_peripheral_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 254420)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, form = (variant / 256) % 3, read = form == 1 ? 7 : 6;
		unsigned const checkpoint = std::array<unsigned, 3>{4, read - 1, read}[variant / 3072];
		u16 const value = std::array<u16, 4>{0, 0xffff, 0x8000, 0x1234}[(variant / 64) % 4];
		bool const b = (variant / 768) % 2;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Peripheral LDU uses retained bus stages");
		if (form == 1 && tick == 4) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(((variant / 1536) % 2 ? 0x20000 : 0) + 0x601, 0xdead); }
		if (form == 2 && tick == read - 1) m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x2222);
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads}; m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save peripheral LDU before, during and after its paid wait"); }
		if (tick == 18)
		{
			expect(m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == unsigned(form == 1) && m_cpu->state_int(tms320c54x_device::STATE_A) == (b ? 0xabcdef1234ULL : value) && m_cpu->state_int(tms320c54x_device::STATE_B) == (b ? value : 0x123456789aULL) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1600 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900, "Peripheral LDU zero-extends one retained word without changing status or the other accumulator");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Replay peripheral LDU without duplicate endpoint transactions"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 254430; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 9215) { start_ldu_peripheral(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged peripheral LDU: PASS cases=9216 restore=9216 addresses=64 forms=direct,absolute,indirect values=4 accumulators=2 pages=2 snapshots=3 source_reads=1 writes=0 peripheral_extra_clock=1 physical_mapping=unvalidated\n");
			throw emu_fatalerror(0, "TMS320C54x isolated peripheral LDU complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254430) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254420; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_shifted_peripheral_edge_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 62122)
	{
		unsigned const tick = ++m_idle_fixture_tick, checkpoint = 2 + m_pipeline_peripheral_case / 32;
		if (tick == checkpoint && !m_pipeline_replay) { m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save excluded shifted peripheral form before its bus access"); }
		if (tick == 14)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && !m_pipeline_operand_reads && !m_pipeline_store_writes, "Excluded mapped shift/control form stops before any data-side effects");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore excluded shifted operand without admitting its endpoint"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 62123; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 63) { start_shifted_peripheral_edge(m_pipeline_peripheral_case + 1); return true; }
			if (!strcmp(machine().options().bios(), "shift-bk-edge"))
			{
				osd_printf_info("TMS320C54x shifted BK exclusions: PASS cases=64 restore=64 accumulators=A,B pages=2 snapshots=2 classes=repeat,block,XC,delay,ST1,BRC,LD,ST data_reads=0 data_writes=0 atomic_fallback=0 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated shifted BK exclusions complete");
			}
			osd_printf_info("TMS320C54x shifted peripheral exclusions: PASS cases=64 restore=64 accumulators=A,B pages=2 snapshots=2 classes=repeat,block,XC,delay,ST1,BRC,ADD,SUB data_reads=0 data_writes=0 atomic_fallback=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated shifted peripheral exclusions complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62123) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62122; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 62120)
	{
		auto const f = shifted_peripheral_case(m_pipeline_peripheral_case, shifted_bsp_fixture(), shifted_unowned_fixture()); unsigned const tick = ++m_idle_fixture_tick;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Shifted peripheral load remains staged");
		expect(m_cpu->state_int(f.b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == (tick > f.read ? f.result : f.b ? 0xfedcba9876ULL : 0x123456789aULL), "Shifted load publishes only in the Execute clock following its owned Read");
		if (tick < f.read) expect(!m_pipeline_operand_reads, "Peripheral wait cannot acknowledge its source early");
		if (tick == f.read - 2)
		{
			auto const disable = machine().disable_side_effects();
			for (unsigned i = 1; i <= f.literals; ++i) m_cpu->space(AS_PROGRAM).write_word(f.base + 0x600 + i, 0xffff);
		}
		if (tick == f.read - 1 && f.form == 3) { expect(m_cpu->state_int(tms320c54x_device::STATE_AR1) == f.address + 1, "Postincrement belongs to the single retained Access before its peripheral wait"); m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x4444); }
		if (tick == f.checkpoint)
		{
			{ auto const disable = machine().disable_side_effects(); expect(m_cpu->space(AS_DATA).read_word(f.address) == (m_pipeline_operand_reads ? u16(f.value ^ 0xffff) : f.value), "Debugger sees the live endpoint without consuming it"); }
			if (f.bsp) expect(m_shifted_bsp.receive_ready && m_shifted_bsp.receive_overrun == !m_pipeline_operand_reads, "Debugger observation preserves the pending BSP word and overrun");
			if (!m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads}; m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save pending/paid/retained shifted peripheral access and device state"); }
		}
		if (tick == 16)
		{
			expect(m_pipeline_operand_reads == 1 && !m_pipeline_store_writes && m_pipeline_extensions_reads == f.literals && m_cpu->state_int(f.b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == f.result && m_cpu->state_int(f.b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == (f.b ? 0x123456789aULL : 0xfedcba9876ULL) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xb800 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0 && m_cpu->state_int(tms320c54x_device::STATE_AR1) == (f.form == 3 ? 0x4444 : f.form == 4 ? u16(f.address - 0x10) : f.address), "Shifted peripheral load retains its original operand/address and exact literals across replay without changing flags");
			if (f.bsp) expect(m_shifted_bsp.receive_ready && !m_shifted_bsp.receive_overrun && m_shifted_bsp.receive == u16(f.value ^ 0xffff), "The next BSP frame remains pending after CPU retirement and replay");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore shifted peripheral operand without repeating an acknowledged bus read"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 62121; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case + 1 < (f.bsp ? 40960U : f.unowned ? 122880U : 245760U)) { start_shifted_peripheral(m_pipeline_peripheral_case + 1); return true; }
			if (f.unowned) osd_printf_info("TMS320C54x shifted unowned low bus: PASS cases=122880 restore=122880 addresses=0002..0005 shifts=32 forms=DP,SP,absolute,postinc,offset flags=SXM,OVM accumulators=A,B patterns=4 pages=2 snapshots=3 extra_clock=0 source_reads=1 data_writes=0 literals_retained=1 pointer_retained=1 endpoint_identity_claim=0 silicon_phase_claim=0\n");
			else if (f.bsp) osd_printf_info("TMS320C54x shifted BSP receive: PASS cases=40960 restore=40960 shifts=32 forms=DP,SP,absolute,postinc,offset flags=SXM,OVM accumulators=A,B patterns=4 pages=2 snapshots=4 extra_clock=1 source_reads=1 data_writes=0 literals_retained=1 pointer_retained=1 original_word=1 next_frame_pending=1 ready_overrun_ack=1 debugger_peek=1 physical_clock_claim=0 silicon_phase_claim=0\n");
			else osd_printf_info("TMS320C54x shifted peripheral load: PASS cases=245760 restore=245760 addresses=64 shifts=-16,-1,4,15 forms=DP,SP,absolute,postinc,offset flags=SXM,OVM accumulators=A,B patterns=4 pages=2 snapshots=3 extra_clock=1 source_reads=1 data_writes=0 literals_retained=1 pointer_retained=1 endpoint_identity_claim=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated shifted peripheral load complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62121) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62120; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_circular_portr_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 62138)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = circular_portr_case(variant);
		expect(bool(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)) == (f.excluded && tick >= 5), "Circular PORTR excludes unsupported mapped destinations before I/O or pointer effects");
		if (tick == 4) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xdead); }
		if (!f.excluded && tick == 5)
		{
			for (unsigned i = 0; i < 8; ++i) { expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == (i == f.selected ? f.final_ar : f.initial[i]), "PORTR circular address modification belongs to Access once"); m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x2200 + i); }
		}
		unsigned const checkpoint = 4 + f.snapshot;
		if (tick == checkpoint && !m_pipeline_replay) { m_saved_pipeline.str({}); m_saved_pipeline.clear(); m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads}; expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save circular PORTR literal, address or sampled I/O value"); }
		if (tick == 16)
		{
			expect(m_pipeline_operand_reads == unsigned(!f.excluded) && m_pipeline_store_writes == unsigned(!f.excluded) && m_pipeline_extensions_reads == 1 && bool(m_cpu->state_int(tms320c54x_device::STATE_IDLE)) == !f.excluded && m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x9876543210ULL, "Circular PORTR retains address and sample across replay without duplicate I/O, writes or flag changes");
			for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == (f.excluded ? f.initial[i] : 0x2200 + i), "PORTR never resamples AR after Access or modifies an excluded pointer");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore circular PORTR without repeating a consumed port sample"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 62139; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_operand_data_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 98303) { start_circular_portr(variant + 1); return true; }
			osd_printf_info("TMS320C54x circular PORTR: PASS cases=98304 restore=98304 modes=4 registers=8 compatibility=2 bk=4 indices=4 samples=4 ports=2 flags=2 pages=2 snapshots=3 literal_retained=1 sample_retained=1 pointer_updates=once destination_reads=0 excluded_io_reads=0 excluded_writes=0 bk_zero=legacy_linear silicon_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated circular PORTR complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62139) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62138; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 62140)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = circular_portr_block_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Circular PORTR composes with the firmware-owned RPTBD block");
		bool const capture = f.snapshot == 1 ? m_pipeline_store_writes == 1 : m_pipeline_operand_reads == (f.snapshot ? f.count / 2 + 1 : 1) && m_pipeline_store_writes + 1 == m_pipeline_operand_reads;
		if (capture && !m_pipeline_expected_cycle && !m_pipeline_replay) { m_pipeline_expected_cycle = tick; m_saved_pipeline.str({}); m_saved_pipeline.clear(); m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes}; expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save circular I/O block before/after publication or at mid-block Read"); }
		if (tick == 96)
		{
			expect(m_pipeline_expected_cycle && m_pipeline_operand_reads == f.count && m_pipeline_store_writes == f.count && m_pipeline_register_writes == 1 && m_cpu->state_int(tms320c54x_device::STATE_IDLE) && !m_cpu->state_int(tms320c54x_device::STATE_BRC) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x900 | (f.compatibility ? 0x20 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x9876543210ULL, "Circular PORTR block drains BRC and preserves status and accumulators without duplicate samples");
			for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == (i == f.selected ? f.buffer + (f.index + f.count) % f.size : 0x3300 + i), "Circular PORTR block modifies only its selected pointer and wraps exactly");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = m_pipeline_expected_cycle; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore active circular I/O block and volatile sample sequence"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 62141; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_operand_data_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 1727) { start_circular_portr_block(variant + 1); return true; }
			osd_printf_info("TMS320C54x circular PORTR block: PASS cases=1728 restore=1728 counts=2,7,17 bk=1,7,16 registers=8 compatibility=2 start_positions=2 pages=2 snapshots=3 source=volatile_io issue_interval=2 sample_retained=1 wrap=1 destination_reads=0 duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated circular PORTR block complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62141) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62140; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 253770)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = t_peripheral_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "STLM/ST peripheral pair remains admitted");
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_register_writes, m_pipeline_store_writes};
			m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save STLM/ST before producer, during peripheral wait or after publication");
		}
		if (tick == 16)
		{
			{ auto const disable = machine().disable_side_effects(); expect(m_cpu->space(AS_DATA).read_word(f.destination) == f.value && m_cpu->space(AS_DATA).read_word(f.source) == f.value, "Peripheral ST preserves its T/TRN source"); }
			expect(!m_pipeline_operand_reads && m_pipeline_register_writes == 1 && m_pipeline_store_writes == 1 && m_cpu->state_int(tms320c54x_device::STATE_AR2) == f.destination + unsigned(f.postincrement) && m_cpu->state_int(tms320c54x_device::STATE_A) == f.value && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfedcba9876ULL && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800, "Peripheral ST preserves status/accumulators and updates its pointer once");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_register_writes = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Replay T/TRN producer and peripheral publication once"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253780; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 863) { start_t_peripheral(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged peripheral T/TRN store: PASS cases=864 restore=864 sources=T,TRN producer=STLM gaps=0,1,2 forms=indirect,postincrement destinations=3b,48,49 pages=2 destination_reads=0 writes=1 peripheral_extra_clock=1 pointer_updates=once silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated peripheral T/TRN store complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253780) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253770; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 253750)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = mvkd_peripheral_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "MVKD peripheral transaction remains admitted");
		if (tick == f.read)
		{
			expect(m_pipeline_operand_reads == 1 && !m_pipeline_store_writes, "Source capture precedes peripheral publication");
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.source, 0xdead);
			m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xbeef);
			if (f.form == 2) m_cpu->space(AS_PROGRAM).write_word(f.base + 0x602, 0xbeef);
		}
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads};
			m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save MVKD before Read, before peripheral wait or after publication");
		}
		if (tick == 14)
		{
			{ auto const disable = machine().disable_side_effects(); expect(m_cpu->space(AS_DATA).read_word(f.destination) == f.value && m_cpu->space(AS_DATA).read_word(f.source) == 0xdead, "MVKD retains its source across the peripheral wait"); }
			expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_extensions_reads == (f.form == 2 ? 2 : 1) && m_cpu->state_int(tms320c54x_device::STATE_AR2) == f.destination + unsigned(f.form == 1) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800 && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfedcba9876ULL, "MVKD peripheral replay preserves status/accumulators and modifies its pointer once");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Replay MVKD without duplicating its peripheral wait or write"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253760; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 647) { start_mvkd_peripheral(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged peripheral MVKD: PASS cases=648 restore=648 sources=AR,BK,RAM forms=indirect,postincrement,absolute destinations=3b,48,49 pages=2 reads=1 writes=1 peripheral_extra_clock=1 pointer_updates=once bk_producer_latency=unvalidated silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated peripheral MVKD complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253760) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253750; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_mvdd_peripheral_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 254320)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = 4 + variant / 384;
		u16 const destination = std::array<u16, 3>{0x20, 0x33, 0x5f}[variant % 3], value = std::array<u16, 4>{0, 0x7f, 0x8000, 0xffff}[(variant / 3) % 4];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "RAM-to-peripheral MVDD remains staged");
		if (tick == 4) { unsigned const xm = (variant / 12) % 4, ym = (variant / 48) % 4; auto delta = [](unsigned mode) { return mode == 1 ? -1 : mode == 2 ? 1 : mode == 3 ? 3 : 0; }; expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) == unsigned(0x14b2 + delta(xm)) && m_cpu->state_int(tms320c54x_device::STATE_AR3) == unsigned(destination + delta(ym)), "MVDD applies each selected dual modifier once before waiting"); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x2222); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x3333); }
		if (tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x14b2, 0xdead); }
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes}; m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save MVDD before, during or after its peripheral wait"); }
		if (tick == 12)
		{
			{ auto const disable = machine().disable_side_effects(); expect(m_cpu->space(AS_DATA).read_word(destination) == value && m_cpu->space(AS_DATA).read_word(0x14b2) == 0xdead, "MVDD uses its captured source and peripheral destination"); }
			expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x2222 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x3333 && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfedcba9876ULL && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900, "Peripheral MVDD preserves status, stack and retained pointers without duplicate accesses");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore MVDD with its peripheral wait already paid when appropriate"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 254330; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 1535) { start_mvdd_peripheral(variant + 1); return true; }
			osd_printf_info("TMS320C54x peripheral MVDD: PASS cases=1536 restore=1536 destinations=3 values=4 dual_modifiers=16 pages=2 snapshots=4 source_reads=1 peripheral_writes=1 extra_cycles=1 retained_address=1 retained_source=1 repeat_acceptance=0 irq_acceptance=0 silicon_phase_claim=0\n");
			osd_printf_info("TMS320C54x staged read identity: PASS cases=1536 restore=1536 logical_instruction=1 captured_opcode=1 wrong_address_excluded=1 peeks_excluded=1\n");
			osd_printf_info("TMS320C54x staged MVDD write identity: PASS cases=1536 restore=1536 logical_instruction=1 captured_opcode=1 retained_source=1 wrong_destination_excluded=1 peeks_excluded=1\n");
			throw emu_fatalerror(0, "TMS320C54x isolated peripheral MVDD complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254330) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254320; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_portr_bypass_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 62144)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = portr_bypass_case(variant);
		unsigned const checkpoint = std::array<unsigned, 4>{4, 6, 7, 11}[f.snapshot];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "PORTR consumer sequence is admitted");
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads, m_port_reads}; m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save PORTR around sample, pending write and consumer");
		}
		if (tick == 24)
		{
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != f.a || m_cpu->state_int(tms320c54x_device::STATE_B) != f.b) osd_printf_info("PORTR bypass mismatch: case=%u a=%010llx expected_a=%010llx b=%010llx expected_b=%010llx reads=%u\n", variant, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), (unsigned long long)f.a, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B), (unsigned long long)f.b, m_port_reads);
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_A) == f.a && m_cpu->state_int(tms320c54x_device::STATE_B) == f.b && m_cpu->state_int(tms320c54x_device::STATE_T) == 0xbeef && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (f.sxm ? 0x100 : 0)) && m_pipeline_operand_reads == (f.chain ? 2U : 1U) && m_pipeline_store_writes == m_pipeline_operand_reads && m_pipeline_extensions_reads == m_pipeline_operand_reads && m_port_reads <= (f.chain ? 2U : 1U), "PORTR consumers use retained incoming data without duplicate I/O or RAM writes");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; m_port_reads = m_recurring_irq_snapshot[3]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore PORTR without replaying an I/O sample or publication"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 62145; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_operand_data_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 6655) { start_portr_bypass(variant + 1); return true; }
			osd_printf_info("TMS320C54x PORTR consumer bypass: PASS cases=6656 restore=6656 generic_cases=6144 dispatcher_cases=512 consumers=LD,AND,OR,XOR gaps=0,1,2 alias_controls=2 accumulators=2 sxm=2 patterns=8 pages=2 snapshots=4 volatile_io=1 real_writes=1 peripheral_bypass_claim=0 physical_banks=unvalidated\n"); throw emu_fatalerror(3, "TMS320C54x isolated PORTR consumer bypass complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62145) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62144; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_stlm_bsp_edge_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 62062)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = 3 + variant / 16;
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes}; m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save excluded STLM BSP before Decode admission");
		}
		if (tick == 12)
		{
			bool const source = (variant / 4) % 2;
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && !m_pipeline_operand_reads && !m_pipeline_store_writes &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == (source ? 0xdead : 0x1111) &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == (source ? 0x1111 : 0xdead), "Excluded STLM BSP composition has no data-bus or accumulator effects");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore excluded STLM BSP before any peripheral publication");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 62063; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 31) { start_stlm_bsp_edge(variant + 1); return true; }
			osd_printf_info("TMS320C54x BSP STLM exclusions: PASS cases=32 restore=32 compositions=delay,repeat,block,XC accumulators=2 pages=2 snapshots=2 data_reads=0 data_writes=0 accumulator_changes=0 silicon_trap_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated BSP STLM exclusions complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62063) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62062; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_staged_bus_stm_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 253590)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const timer = !strcmp(machine().options().bios(), "timer-stm");
		bool const standard = !strcmp(machine().options().bios(), "std-stm");
		bool const clock = !strcmp(machine().options().bios(), "clock-stm");
		bool const serial = !strcmp(machine().options().bios(), "serial-stm");
		u16 const address = serial ? 0x22 : standard ? 0x28 : clock ? 0x58 : (timer ? std::array<u16, 3>{0x24, 0x25, 0x26} : std::array<u16, 3>{0x28, 0x29, 0x2b})[variant % 3];
		u16 const value = (serial ? std::array<u16, 4>{0xc008, 0xc0c8, 0, 0xffff} : clock ? std::array<u16, 4>{0, 0x3146, 0x8006, 0xffff} : standard ? std::array<u16, 4>{0x2000, 0x7fff, 0x8006, 0xffff} : address == 0x26 ? std::array<u16, 4>{0x10, 0x13, 0x30, 0xffff} : std::array<u16, 4>{0, 0x7fff, 0x8006, 0xffff})[(variant / 3) % 4];
		unsigned const checkpoint = std::array<unsigned, 3>{6, 7, 9}[variant / 12];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Only configured bus STM forms execute");
		if (tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(0x601, value ^ 0xffff); }
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads, 0};
			m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save bus STM before/during peripheral wait or after publication");
		}
		if (tick == 12)
		{
			{
				auto const disable = machine().disable_side_effects(); auto &data = m_cpu->space(AS_DATA);
				if (timer)
				{
					u16 const expected_tcr = address == 0x26 ? (value & 0x0c1f) | ((value & 0x20) ? (value & 0xf) << 6 : 0xc0) : 0xd3;
					expect(data.read_word(0x24) == (address == 0x24 ? value : address == 0x26 && (value & 0x20) ? 0x4567 : 0x2345), "Timer STM preserves TIM except explicit write or TRB reload");
					expect(data.read_word(0x25) == (address == 0x25 ? value : 0x4567) && data.read_word(0x26) == expected_tcr, "Timer STM applies PRD, writable TCR mask and TRB-owned PSC reload");
					expect(!m_cpu->state_int(tms320c54x_device::STATE_IFR), "Stopped timer STM cannot generate an interrupt");
				}
				else if (clock) expect(data.read_word(0x58) == ((value & ~u16(1)) | BIT(value, 1)), "CLKMD STM uses the existing unclocked DIV/PLL status contract");
				else expect(data.read_word(address) == (value & (standard ? 0x7fff : address == 0x29 ? 0xf006 : address == 0x2b ? 1 : 0xffff)), "Bus STM retains the device register mask");
			}
			expect(m_pipeline_store_writes == 1 && !m_pipeline_operand_reads && m_pipeline_extensions_reads == 1, "Bus STM has one literal and one write without destination reads");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore bus STM without repeating the wait or write"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253600; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant + (standard || clock || serial ? 3 : 1) < 36) { start_staged_bus_stm(variant + (standard || clock || serial ? 3 : 1)); return true; }
			if (serial)
			{
				osd_printf_info("TMS320C54x staged BSP STM: PASS cases=12 restore=12 literal_fetches=1 writes=1 destination_reads=0 publish_clock=8 peripheral_extra_clock=1 endpoint_semantics=external silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated staged BSP STM complete");
			}
			if (clock)
			{
				osd_printf_info("TMS320C54x staged CLKMD STM: PASS cases=12 restore=12 literal_fetches=1 writes=1 destination_reads=0 publish_clock=8 pll_reference=unconfigured physical_clock_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated staged CLKMD STM complete");
			}
			if (standard)
			{
				osd_printf_info("TMS320C54x standard SWWSR STM: PASS cases=12 restore=12 literal_fetches=1 writes=1 destination_reads=0 publish_clock=8 physical_mapping=unvalidated\n");
				throw emu_fatalerror(0, "TMS320C54x isolated standard SWWSR STM complete");
			}
			if (timer)
			{
				osd_printf_info("TMS320C54x staged timer STM: PASS cases=36 restore=36 registers=TIM,PRD,TCR literal_fetches=1 writes=1 destination_reads=0 publish_clock=8 peripheral_extra_clock=1 stopped_timer=1 trb_reload=1 psc_write_mask=1 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated staged timer STM complete");
			}
			osd_printf_info("TMS320C54x staged bus STM: PASS cases=36 restore=36 registers=SWWSR,BSCR,SWCR literal_fetches=1 writes=1 destination_reads=0 publish_clock=8 peripheral_extra_clock=1 physical_mapping=unvalidated\n");
			throw emu_fatalerror(0, "TMS320C54x isolated staged bus STM complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253600) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253590; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 253570)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		unsigned const transfers = variant / 8 ? 1 : 1 + (variant / 2) % 4;
		bool const write = variant % 2;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Atomic composition remains supported");
		if (tick == 2) m_cpu->set_input_line(0, ASSERT_LINE);
		if (tick == 4 && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, 0, m_vector_acks};
			m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save waited atomic control-flow composition");
		}
		if (tick == 64)
		{
			expect(m_vector_acks == 1 && m_pipeline_operand_reads == (write ? 0 : transfers) && m_pipeline_store_writes == (write ? transfers : 0) && m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x1400 + transfers, "Atomic retirement preserves repeat count, pointer updates and guarded IRQ delivery");
			expect(m_cpu->state_int(tms320c54x_device::STATE_RTN) == (variant / 8 == 1 ? 0x700 : 0x603), "Pending I/O retires to the original sequential or delayed continuation before IRQ entry");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = 4; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_vector_acks = m_recurring_irq_snapshot[3];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Replay pending atomic retirement exactly once"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253580; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_input_line(0, CLEAR_LINE);
			if (variant < 23) { start_atomic_io_composition(variant + 1); return true; }
			osd_printf_info("TMS320C54x atomic I/O composition: PASS cases=24 restore=24 contexts=repeat,delay,INTM pointer_updates=once interrupt_after_transfer=1 physical_mapping=unvalidated\n");
			throw emu_fatalerror(0, "TMS320C54x isolated atomic I/O composition complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253580) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253570; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 253550)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = bus_io_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Staged external wait path has no atomic fallback");
		if (tick == (atomic_bus_io() ? 1 : 6) && f.write) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x1400, 0xdead); }
		if (tick == f.first)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x28, 0); m_cpu->space(AS_DATA).write_word(0x2b, 0);
			m_cpu->space(AS_IO).write_word(0x125, 0xabcd); m_cpu->space(AS_PROGRAM).write_word(0x601, 0xbeef);
			// A zero-wait aliased read has already published AR1; do not erase its result.
			if (bus_io_form(variant) != 4 || f.waits) m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x1500);
			if (bus_io_form(variant) >= 1 && bus_io_form(variant) <= 3) m_cpu->space(AS_PROGRAM).write_word(0x602, 0xdead);
			if (f.interrupt) m_cpu->set_input_line(f.interrupt == 2 ? INPUT_LINE_NMI : 0, ASSERT_LINE);
		}
		if (f.hold && f.waits >= 2 && tick == f.complete - 1) m_bus_ready = true;
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads, m_vector_acks};
			m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save external transfer before/during waits or after its side effect");
		}
		if (tick == f.complete + 12)
		{
			unsigned const form = bus_io_form(variant), literals = 1 + unsigned(form >= 1 && form <= 3);
			u16 const expected_ram = f.value + unsigned(form == 4 && !f.write);
			{ auto const disable = machine().disable_side_effects();
				if ((!f.write && m_cpu->space(AS_DATA).read_word(bus_io_address(variant)) != expected_ram) || m_pipeline_operand_reads != unsigned(!f.write) || m_pipeline_store_writes != unsigned(f.write) || m_pipeline_extensions_reads != literals || m_vector_acks != unsigned(bool(f.interrupt)))
					osd_printf_info("Bus I/O failure: case=%u replay=%u waits=%u write=%u hold=%u irq=%u ram=%04x expected=%04x reads=%u writes=%u literals=%u acks=%u pc=%06x\n", variant, unsigned(m_pipeline_replay), f.waits, unsigned(f.write), unsigned(f.hold), f.interrupt, m_cpu->space(AS_DATA).read_word(0x1400), f.value, m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads, m_vector_acks, unsigned(m_cpu->state_int(STATE_GENPC))); }
			{ auto const disable = machine().disable_side_effects(); expect((f.write || m_cpu->space(AS_DATA).read_word(bus_io_address(variant)) == expected_ram) && m_pipeline_operand_reads == unsigned(!f.write) && m_pipeline_store_writes == unsigned(f.write) && m_pipeline_extensions_reads == literals && m_vector_acks == unsigned(bool(f.interrupt)), "Waited transfer keeps address/data ownership, MMR alias ordering and deferred interrupt delivery"); }
			unsigned const elapsed = 0xffff - unsigned(m_cpu->state_int(tms320c54x_device::STATE_TIM));
			expect(elapsed >= tick - 1 && elapsed <= tick + 1, "Timer clocks continue during external bus suspension");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; m_vector_acks = m_recurring_irq_snapshot[3];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore retained wait/READY state without duplicate I/O"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253560; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			unsigned next = variant + 1;
			// Original SWWSR has no multiplier: omit, rather than relabel,
			// the existing fixture's multiplier-enabled raw case IDs.
			if (standard_bus_io() && (next & 8)) next += 8;
			if (next < (atomic_bus_io() ? 2880 : 576)) { start_bus_io_test(next); return true; }
			m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
			if (standard_bus_io())
			{
				osd_printf_info("TMS320C54x standard SWWSR I/O: PASS frontend=%s cases=%u restore=%u software_waits=0..7 ready_threshold=2 sources=none,IRQ,NMI timer_continues=1 duplicate_io=0 program_data_wait_claim=0 physical_mapping=unvalidated\n", atomic_bus_io() ? "atomic" : "staged", atomic_bus_io() ? 1440 : 288, atomic_bus_io() ? 1440 : 288);
				throw emu_fatalerror(0, "TMS320C54x isolated standard SWWSR I/O complete");
			}
			if (atomic_bus_io())
			{
				osd_printf_info("TMS320C54x atomic external I/O waits: PASS cases=2880 restore=2880 forms=short,offset,absolute,peripheral,AR_alias software_waits=0..14 ready_threshold=2 directions=read,write sources=none,IRQ,NMI timer_continues=1 duplicate_io=0 physical_mapping=unvalidated\n");
				throw emu_fatalerror(0, "TMS320C54x isolated atomic external I/O complete");
			}
			osd_printf_info("TMS320C54x staged external I/O waits: PASS cases=576 restore=576 software_waits=0..14 ready_threshold=2 directions=read,write sources=none,IRQ,NMI timer_continues=1 duplicate_io=0 atomic_acceptance=separate physical_mapping=unvalidated\n");
			throw emu_fatalerror(0, "TMS320C54x isolated staged external I/O complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253560) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253550; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

} // namespace c54x_test
