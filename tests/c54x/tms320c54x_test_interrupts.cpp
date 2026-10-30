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


void tms320c54x_test_state::start_mvmd_status_irq(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const stlm = profile_is("stlm-st1-irq");
	bool const two_word = variant >= 288;
	bool const masked = variant & 2, enabled = variant & 4, nmi = variant & 8;
	u32 const base = variant & 16 ? 0x20000 : 0, vector = base + (nmi ? 4 : 0x40);
	u16 const status = masked ? 0x800 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	// A masked pending source may wake IDLE without being serviced. Keep
	// the bounded continuation valid rather than running into erased words.
	for (unsigned i = 0; i < 64; ++i) program.write_word(base + 0x600 + i, i >= 4 ? 0xf4e1 : 0xf495);
	program.write_word(base + 0x600, 0x730b); program.write_word(base + 0x601, 7);
	if (stlm) { program.write_word(base + 0x600, 0x8907); program.write_word(base + 0x601, 0xf495); }
	program.write_word(base + 0x602, two_word ? 0x770e : 0x880e); program.write_word(base + 0x603, two_word ? 0x55 : 0xf495);
	program.write_word(base + 0x604, 0xf4e1); program.write_word(base + 0x620, 0xf4e1);
	program.write_word(vector, 0xf495); program.write_word(vector + 1, 0xf4eb);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x2000); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, variant & 1 ? 0x800 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, enabled ? 1 : 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x55); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234560000ULL | status); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_vector_acks = m_fast_stack_reads = m_pipeline_extensions_reads = 0; m_timer_probe_return_pc = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "mvmd_status_irq_reads",
		[this, status](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0xb) { expect(!m_pipeline_operand_reads && word == status && m_cpu->pipeline_cycles() == 5, "Status source read survives pending IRQ/NMI drain"); ++m_pipeline_operand_reads; } else { expect(at == 0xfff && !m_fast_stack_reads && m_vector_acks == 1 && word == m_timer_probe_return_pc, "RETE pops only the owned interrupt continuation"); ++m_fast_stack_reads; } });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "mvmd_status_irq_writes",
		[this, status](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 7) { expect(!m_pipeline_store_writes && word == status && m_cpu->pipeline_cycles() == 6, "Full status publishes once despite pending interrupts"); ++m_pipeline_store_writes; } else if (at == 0xe) { expect(!m_pipeline_register_writes && word == 0x55, "Foreground marker executes exactly once across interrupt return"); ++m_pipeline_register_writes; } else { expect(at == 0xfff && !m_pipeline_extensions_reads && m_vector_acks == 1 && word == m_timer_probe_return_pc, "IRQ/NMI stacks one owned continuation"); ++m_pipeline_extensions_reads; } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 253890;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
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






bool tms320c54x_test_state::check_status_write_interrupt_results()
{
	if (m_phase == 253890)
	{
		bool const stlm = profile_is("stlm-st1-irq");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, arrival = std::array<unsigned, 3>{4, 6, 8}[(variant / 32) % 3], checkpoint = std::array<unsigned, 3>{5, 7, 12}[(variant % 288) / 96];
		bool const old_masked = variant & 1, masked = variant & 2, enabled = variant & 4, nmi = variant & 8;
		// The two-word follower has an incomplete Decode at clock five;
		// the one-word follower permits selection before clock-six INTM.
		bool const effective_masked = old_masked && masked;
		bool const accepted = nmi || (enabled && !effective_masked);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Status/IRQ composition remains staged");
		if (tick == arrival) m_cpu->set_input_line(nmi ? INPUT_LINE_NMI : 0, ASSERT_LINE);
		if (tick == arrival + 1) m_cpu->set_input_line(nmi ? INPUT_LINE_NMI : 0, CLEAR_LINE);
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_vector_acks, m_fast_stack_reads, m_pipeline_extensions_reads, m_timer_probe_return_pc}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save status mask change and pending/accepted interrupt"); }
		if (tick == 96)
		{
			if (m_vector_acks != unsigned(accepted) || m_pipeline_register_writes != 1 || m_fast_stack_reads != unsigned(accepted) || m_pipeline_extensions_reads != unsigned(accepted))
				osd_printf_info("MVMD status IRQ mismatch: case=%u replay=%u accepted=%u source=%u status=%u marker=%u ack=%u stack_read=%u stack_write=%u pc=%06x sp=%04x st1=%04x ifr=%04x t=%04x return=%04x\n", variant, unsigned(m_pipeline_replay), unsigned(accepted), m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_vector_acks, m_fast_stack_reads, m_pipeline_extensions_reads, unsigned(m_cpu->state_int(STATE_GENPC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_T)), m_timer_probe_return_pc);
			expect(m_pipeline_operand_reads == unsigned(!stlm) && m_pipeline_store_writes == 1 && m_pipeline_register_writes == 1 && m_vector_acks == unsigned(accepted) && m_fast_stack_reads == unsigned(accepted) && m_pipeline_extensions_reads == unsigned(accepted) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x55 && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x55 && m_cpu->state_int(tms320c54x_device::STATE_B) == (0x1234560000ULL | (masked ? 0x800 : 0)), "Full status mask transitions preserve pending selection, one frame and the foreground continuation");
			expect(m_cpu->state_int(tms320c54x_device::STATE_ST1) == (effective_masked && !accepted ? 0x800 : 0) && (m_cpu->state_int(tms320c54x_device::STATE_IFR) & 1) == unsigned(!nmi && !accepted), "Memory writes cannot set INTM; masked IRQ remains pending and RETE clears INTM after accepted service");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2]; m_vector_acks = m_recurring_irq_snapshot[3]; m_fast_stack_reads = m_recurring_irq_snapshot[4]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[5]; m_timer_probe_return_pc = m_recurring_irq_snapshot[6]; return restore_pipeline_at(253900, "Restore mask publication and IRQ/NMI without duplicate service"); }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 575) { start_mvmd_status_irq(variant + 1); return true; }
			if (stlm)
			{
				osd_printf_info("TMS320C54x staged STLM ST1 IRQ: PASS cases=576 restore=576 sources=IRQ,NMI accumulator=B gap_nops=1 intm_requests=4 memory_cannot_set_intm=1 clear_policy=legacy_unverified imr_states=2 arrivals=4,6,8 pages=2 snapshots=3 followers=STLM,STM source_reads=0 status_writes=1 accepted_frame=1 masked_frame=0 foreground_once=1 rsbx_guard_inherited=0 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated STLM ST1 IRQ complete");
			}
			osd_printf_info("TMS320C54x staged MVMD status IRQ: PASS cases=576 restore=576 sources=IRQ,NMI intm_requests=4 memory_cannot_set_intm=1 clear_policy=legacy_unverified imr_states=2 arrivals=4,6,8 pages=2 snapshots=3 followers=STLM,STM source_reads=1 status_writes=1 accepted_frame=1 masked_frame=0 foreground_once=1 rsbx_guard_inherited=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated MVMD status IRQ complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253900) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253890; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
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
