// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {












void tms320c54x_test_state::start_repeat_mvdp_irq(unsigned variant)
{
	unsigned const index = variant % 4, count_index = std::array<unsigned, 4>{0, 1, 3, 4}[index];
	unsigned const count = std::array<unsigned, 4>{0, 1, 7, 255}[index];
	bool const nmi = (variant / 16) % 2, long_count = (variant / 8) % 2;
	u32 const base = (variant / 4) % 2 ? 0x20000 : 0;
	start_repeat_mvdp(13 + count_index * 40 + (base ? 200 : 0) + (long_count ? 400 : 0));
	m_pipeline_peripheral_case = variant;
	m_pipeline_operand_tap.remove();
	// Interrupt return legitimately reads the stack; isolate copy-source
	// observations from those independent data transactions.
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x1230, 0x132f, "repeat_mvdp_irq_source",
		[this, count](offs_t at, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_pipeline_operand_reads <= count && at == 0x1230 + m_pipeline_operand_reads && word == 0x2468 + m_pipeline_operand_reads, "Interrupted repeated MVDP reads each source once");
			++m_pipeline_operand_reads;
		});
	{ auto const disable = machine().disable_side_effects();
		u32 const vector = nmi ? 4 : base + 0x40;
		auto &program = m_cpu->space(AS_PROGRAM);
		program.write_word(vector, 0x4a1e); // PSHM RTN
		program.write_word(vector + 1, 0xe955); // LD #55,B
		program.write_word(vector + 2, 0xf4e5); // RETE
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, nmi ? 0x800 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, nmi ? 0 : 1);
	m_vector_acks = 0; m_phase = 259960;
}



u16 tms320c54x_test_state::repeat_irq_r(offs_t offset)
{
	if (m_phase == 6517 || m_phase == 6518 || m_phase == 251210 || m_phase == 62162 || m_phase == 62163)
		return m_stack_mmr[offset];
	if (offset) return 0;
	m_last_operand_cycle = m_cpu->total_cycles();
	if (++m_repeat_reads == 1)
	{
		m_first_operand_cycle = m_last_operand_cycle;
	}
	if (m_repeat_reads == m_irq_trigger_read)
		m_cpu->set_input_line(2, ASSERT_LINE);
	return 1;
}


bool tms320c54x_test_state::check_idle_pll_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 79700 && m_phase <= 79705)
	{
		if (m_phase == 79700)
		{
			expect(data.read_word(0x58) == 0x77ff, "PLL locks before software-managed idle transition");
			data.write_word(0x58, 0x77fc);
			m_phase = 79701; m_check_timer->adjust(attotime::from_usec(1)); return true;
		}
		if (m_phase == 79701)
		{
			expect(data.read_word(0x58) == 0x77fc, "DIV source with PLL power held before IDLE3");
			auto &program = m_cpu->space(AS_PROGRAM);
			program.write_word(0x0b80, 0xf5e1); program.write_word(0x0b81, 0xf4e1);
			set_registers(*m_cpu, {
				{STATE_GENPC, 0x0b80}, {cpu_regs::STATE_ST1, 0x0800}, {cpu_regs::STATE_IMR, 1},
				{cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IDLE, 0},
			});
			m_cpu->resume(SUSPEND_REASON_DISABLE);
			m_phase = 79702; m_check_timer->adjust(attotime::from_usec(10)); return true;
		}
		if (m_phase == 79702)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == 0x0b81,
				"original IDLE3 instruction stops before continuation");
			m_cpu->set_input_line(0, ASSERT_LINE);
			m_phase = 79703; m_check_timer->adjust(attotime::from_usec(10)); return true;
		}
		if (m_phase == 79703)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == 0x0b82,
				"enabled IRQ wakes IDLE3 to masked-interrupt continuation in DIV mode");
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
			m_cpu->set_input_line(0, CLEAR_LINE);
			data.write_word(0x58, 0x77ff);
			m_phase = 79704; m_check_timer->adjust(attotime::from_usec(1)); return true;
		}
		if (m_phase == 79704)
		{
			expect(data.read_word(0x58) == 0x77fe && m_cpu->clock() == 5'997'600,
				"IDLE3 invalidates retained PLL lock: software re-entry cannot skip the lock count");
			m_phase = 79705; m_check_timer->adjust(attotime::from_usec(340)); return true;
		}
		expect(data.read_word(0x58) == 0x77ff && m_cpu->clock() == 95'961'600,
			"software-managed PLL re-entry completes its new lock delay after IDLE3");
		m_cpu->set_follow_clkmd(false); m_cpu->set_pll_reference_clock(0);
		m_cpu->set_unscaled_clock(13'000'000);
		m_cpu->resume(SUSPEND_REASON_DISABLE);
		osd_printf_info("TMS320C54x IDLE3 PLL relock: PASS div_wake=1 power_held=1 lock_invalidated=1 software_reentry=1 automatic_pll_wake_claim=0\n");
		return true;
	}
	return false;
}







bool tms320c54x_test_state::check_repeat_mvdp_irq_results()
{
	if (m_phase == 259960)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		unsigned const count = std::array<unsigned, 4>{0, 1, 7, 255}[variant % 4];
		unsigned const arrival = std::array<unsigned, 3>{3, 6, count >= 7 ? 12U : 8U}[(variant / 32) % 3];
		// set_input_line queues a scheduler synchronization. Snapshot after
		// that edge has reached the CPU, not inside its enqueue callback.
		unsigned const checkpoint = std::array<unsigned, 3>{4, 9, 14}[variant / 96];
		bool const nmi = (variant / 16) % 2;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Interrupted repeated MVDP remains admitted");
		if (tick == arrival)
		{
			int const line = nmi ? INPUT_LINE_NMI : 0;
			m_cpu->set_input_line(line, ASSERT_LINE); m_cpu->set_input_line(line, CLEAR_LINE);
		}
		if (m_pipeline_store_writes <= count) expect(!m_vector_acks, "Repeated MVDP does not acknowledge during an unfinished copy");
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, unsigned(m_first_port_cycle), m_vector_acks};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save repeated MVDP with pending IRQ or NMI");
		}
		if (tick == count + 100)
		{
			if (m_pipeline_operand_reads != count + 1 || m_pipeline_store_writes != count + 1 || m_vector_acks != 1 || !m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(tms320c54x_device::STATE_AR2) != 0x1230 + count + 1 || m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 || m_cpu->state_int(tms320c54x_device::STATE_B) != 0x55)
				osd_printf_info("MVDP interrupt mismatch: case=%u replay=%u reads=%u writes=%u acks=%u idle=%u AR2=%04x SP=%04x B=%llx PC=%06x IFR=%04x\n", variant, unsigned(m_pipeline_replay), m_pipeline_operand_reads, m_pipeline_store_writes, m_vector_acks, unsigned(m_cpu->state_int(tms320c54x_device::STATE_IDLE)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B), unsigned(m_cpu->state_int(STATE_GENPC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)));
			expect(m_pipeline_operand_reads == count + 1 && m_pipeline_store_writes == count + 1 && m_vector_acks == 1 && m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1230 + count + 1 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1200 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x55, "Repeated MVDP retires completely before one balanced ISR and continuation");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_first_port_cycle = m_recurring_irq_snapshot[2]; m_vector_acks = m_recurring_irq_snapshot[3];
				return restore_pipeline_at(259970, "Restore repeated MVDP and its original pending interrupt");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 287) { start_repeat_mvdp_irq(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged repeated MVDP interrupts: PASS cases=288 restore=288 sources=IRQ,NMI counts=0,1,7,255 count_forms=2 pages=2 arrivals=3 snapshots=3 deferred_until_final_write=1 balanced_stack=1 duplicate_transfers=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated repeated MVDP interrupts complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 259970) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 259960; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}





} // namespace c54x_test
