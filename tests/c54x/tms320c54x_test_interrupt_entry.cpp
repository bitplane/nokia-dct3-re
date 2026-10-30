// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

void tms320c54x_test_state::start_interrupt_entry_spacing_case(unsigned index)
{
	m_cpu->reset();
	// Retire predecessor observers before installing this family's own taps.
	m_vector_prefetch_tap.remove();
	m_vector_stack_tap.remove();
	m_entry.fetch_tap.remove();
	m_entry.stack_tap.remove();
	m_entry.reads = m_entry.stack_writes = 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	program.write_word(index ? 4 : 0x40, 0xf4e1);
	data.write_word(0x0fff, 0xbeef);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_IMR, 1},
		{STATE_GENPC, 0x0800},
	});
	m_entry.fetch_tap = program.install_read_tap(index ? 4 : 0x40, index ? 4 : 0x40,
		"entry_fetch_cycle", [this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			++m_entry.reads;
			m_entry.fetch_cycle = m_cpu->total_cycles();
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff &&
				m_cpu->space(AS_DATA).read_word(0x0fff) == 0xbeef,
				"Entry Fetch follows SP Access and precedes the stack write");
		});
	m_entry.stack_tap = data.install_write_tap(0x0fff, 0x0fff,
		"entry_write_cycle", [this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			++m_entry.stack_writes;
			m_entry.stack_cycle = m_cpu->total_cycles();
			expect(value == 0x0800 && m_entry.reads == 1,
				"Entry Execute stacks the accepted continuation after one vector read");
		});
	m_phase = 180000 + index;
	if (index) m_cpu->set_input_line(INPUT_LINE_NMI, ASSERT_LINE);
	else m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 1);
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_interrupt_acceptance_checkpoint()
{
	m_cpu->reset();
	m_entry.acknowledged_source = -1;
	m_cpu->space(AS_PROGRAM).write_word(0x40, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x0fff, 0xbeef);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_IMR, 1},
		{cpu_regs::STATE_IFR, 3}, {STATE_GENPC, 0x0800},
	});
	m_phase = 190000;
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_entry_stage_restore_case(unsigned index)
{
	m_cpu->reset();
	// Retire predecessor observers before installing this family's own taps.
	m_vector_prefetch_tap.remove();
	m_vector_stack_tap.remove();
	m_entry.fetch_tap.remove(); m_entry.stack_tap.remove();
	m_entry.reads = m_entry.acknowledgements = m_entry.stack_writes = 0;
	m_entry.restore_case = index;
	bool const nmi = index >= 5;
	u16 const vector = nmi ? 4 : 0x40;
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(vector, 0xf020); // LD #1234,A, then IDLE.
	program.write_word(vector + 1, 0x1234);
	program.write_word(vector + 2, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x0fff, 0xbeef);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_ST1, nmi ? 0x0800 : 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_IMR, 1}, {cpu_regs::STATE_IFR, 3}, {STATE_GENPC, 0x0800},
	});
	m_entry.fetch_tap = program.install_read_tap(vector, vector, "entry_stage_restore_fetch",
		[this](offs_t, u16 &, u16) {
			if (!machine().side_effects_disabled()) ++m_entry.reads;
		});
	m_entry.stack_tap = m_cpu->space(AS_DATA).install_write_tap(0x0fff, 0x0fff,
		"entry_stage_restore_stack", [this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			++m_entry.stack_writes;
			expect(value == 0x0800 && m_entry.reads == 1 && m_entry.acknowledgements == 1,
				"Restored entry stacks the original continuation after one Fetch/IACK");
		});
	m_phase = 450000;
	if (nmi) m_cpu->set_input_line(INPUT_LINE_NMI, ASSERT_LINE);
	m_check_timer->adjust(attotime::from_ticks(index % 5 + 1, m_cpu->clock()));
}

void tms320c54x_test_state::start_vector_prefetch_case(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	// Retire predecessor observers before installing this family's own taps.
	m_vector_prefetch_tap.remove();
	m_vector_stack_tap.remove();
	m_entry.fetch_tap.remove();
	m_entry.stack_tap.remove();
	m_entry.reads = m_entry.acknowledgements = 0;
	m_entry.vector_case = index;
	u32 const address = index ? 4 : 0x40;
	program.write_word(address, 0xf020); // LD #1234,A: retain opcode, fetch extension normally.
	program.write_word(address + 1, 0x1234);
	program.write_word(address + 2, 0xf4e1);
	program.write_word(0x0800, 0xf4e1);
	data.write_word(0x0fff, 0xbeef);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_SP, 0x1000},
		{cpu_regs::STATE_ST1, index ? 0x0800 : 0}, {cpu_regs::STATE_IMR, 1}, {cpu_regs::STATE_IFR, 3},
		{cpu_regs::STATE_A, 0}, {STATE_GENPC, 0x0800}, {cpu_regs::STATE_IDLE, 0},
	});
	m_phase = 80000;
	m_entry.fetch_tap = program.install_read_tap(address, address, "vector_prefetch",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff &&
				m_cpu->space(AS_DATA).read_word(0x0fff) == 0xbeef,
				"SPRU131G INTR Access decrements SP before vector Fetch, but stack memory is not written yet");
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == (m_entry.vector_case ? 3 : 2),
				"Vector acknowledgement clears only the accepted maskable source");
			++m_entry.reads;
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
		});
	m_entry.stack_tap = data.install_write_tap(0x0fff, 0x0fff, "vector_stack_write",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled() || m_phase != 80000) return;
			expect(m_entry.reads == 1 && m_entry.acknowledgements == 1 && value == 0x0800 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff,
				"SPRU131G INTR Execute writes RTN after vector Fetch without decrementing SP again");
		});
	if (index) { m_cpu->set_input_line(INPUT_LINE_NMI, ASSERT_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); }
	m_check_timer->adjust(attotime::from_usec(100));
}

bool tms320c54x_test_state::check_vector_prefetch_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 80000 && m_phase <= 80003)
	{
		u32 const address = m_entry.vector_case ? 4 : 0x40;
		if (m_phase == 80003)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0 && m_cpu->state_int(STATE_GENPC) == 0x0801,
				"Reset discards the retained hardware vector opcode");
			m_entry.fetch_tap.remove();
			if (m_entry.vector_case == 2) { start_vector_prefetch_case(3); return true; }
			m_entry.stack_tap.remove();
			osd_printf_info("TMS320C54x vector prefetch: PASS sources=irq,nmi before_stack=1 sp_access=1 retained_opcode=1 extension_fetch=1 save_replay=2 single_ack=1 later_mask=1 reset=1 debugger_redirect=1 physical_latency_claim=0\n");
			start_xc_irq_case(0); return true;
		}
		if (m_phase == 80000)
		{
			expect(m_entry.reads == 1 && m_entry.acknowledgements == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff && data.read_word(0x0fff) == 0xbeef,
				"Suspension after vector Fetch leaves the later continuation write pending");
			expect(machine().scheduler().can_save(), "Vector prefetch checkpoint has no synchronized inputs");
			if (m_entry.vector_case >= 2)
			{
				if (m_entry.vector_case == 2) { m_cpu->reset(); m_cpu->set_state_int(STATE_GENPC, 0x0800); }
				else m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0800);
				m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 80003;
				m_check_timer->adjust(attotime::from_usec(100)); return true;
			}
			expect(capture_snapshot(machine(), m_entry.saved) == STATERR_NONE, "Save retained vector opcode");
			program.write_word(address, 0xf4e1); // Must not replace the already fetched LD.
			m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); // A later mask cannot revoke accepted entry.
			m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 80001;
		}
		else
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				data.read_word(0x0fff) == 0x0800 &&
				m_entry.reads == 1 && m_entry.acknowledgements == 1,
				"Retained vector opcode executes once with its normally fetched extension");
			if (m_phase == 80001)
			{
				expect(restore_snapshot(machine(), m_entry.saved) == STATERR_NONE, "Restore retained vector opcode");
				program.write_word(address, 0xf4e1);
				m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
				m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 80002;
			}
			else
			{
				m_entry.fetch_tap.remove();
				start_vector_prefetch_case(m_entry.vector_case + 1); return true;
			}
		}
		m_check_timer->adjust(attotime::from_usec(100)); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_interrupt_entry_results()
{
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 180000 && m_phase < 180002)
	{
		expect(m_entry.reads == 1 && m_entry.stack_writes == 1 &&
			m_entry.stack_cycle == m_entry.fetch_cycle + 1 &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE),
			"IRQ/NMI entry stack write is one CPU cycle after vector Fetch");
		m_entry.fetch_tap.remove();
		m_entry.stack_tap.remove();
		if (m_phase == 180000) { start_interrupt_entry_spacing_case(1); return true; }
		osd_printf_info("TMS320C54x interrupt entry stages: PASS sources=irq,nmi fetch_to_stack_cycles=1 pipeline_drain_claim=0\n");
		start_interrupt_acceptance_checkpoint(); return true;
	}
	if (m_phase >= 190000 && m_phase <= 190003)
	{
		if (m_phase == 190000)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == 3 &&
				!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800) &&
				m_entry.acknowledged_source == -1 && data.read_word(0x0fff) == 0xbeef,
				"Accepted maskable interrupt retains IFR and INTM before IACK, with no early SP/stack write");
			m_phase = 190003;
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		if (m_phase == 190003)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == 3 &&
				!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800) &&
				m_entry.acknowledged_source == -1 && data.read_word(0x0fff) == 0xbeef,
				"INTR Decode captures continuation before Access decrements SP");
			expect(capture_snapshot(machine(), m_entry.saved) == STATERR_NONE,
				"Save accepted interrupt before Access");
			m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 2);
			m_phase = 190001;
			m_check_timer->adjust(attotime::from_usec(100)); return true;
		}
		expect(m_entry.acknowledged_source == 0 && m_cpu->state_int(tms320c54x_device::STATE_IFR) == 2 &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff &&
			data.read_word(0x0fff) == 0x0800 && m_cpu->state_int(tms320c54x_device::STATE_IDLE),
			"Later IMR changes do not reselect the accepted source; vector Fetch clears only its IFR bit");
		if (m_phase == 190001)
		{
			expect(restore_snapshot(machine(), m_entry.saved) == STATERR_NONE,
				"Restore accepted interrupt before Access");
			expect(!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800) &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == 3,
				"Restored accepted maskable interrupt retains pre-IACK INTM and IFR");
			m_entry.acknowledged_source = -1;
			m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 2);
			m_phase = 190002;
			m_check_timer->adjust(attotime::from_usec(100)); return true;
		}
		osd_printf_info("TMS320C54x accepted interrupt checkpoint: PASS ifr_until_fetch=1 intm_at_iack=1 decode_before_access=1 later_imr=1 source_latched=1 save_replay=1 pipeline_drain_claim=0\n");
		start_entry_stage_restore_case(0); return true;
	}
	if (m_phase >= 450000 && m_phase <= 450002)
	{
		unsigned const stage = m_entry.restore_case % 5;
		bool const nmi = m_entry.restore_case >= 5;
		u16 const vector = nmi ? 4 : 0x40;
		if (m_phase == 450000)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == (stage >= 2 ? 0x0fff : 0x1000) &&
				data.read_word(0x0fff) == (stage >= 4 ? 0x0800 : 0xbeef) &&
				m_entry.reads == unsigned(stage >= 3) && m_entry.acknowledgements == unsigned(stage >= 3) &&
				m_entry.stack_writes == unsigned(stage >= 4) &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == (nmi || stage < 3 ? 3 : 2),
				"Checkpoint separates acceptance, Decode, Access, Fetch/IACK and stack Execute");
			expect(capture_snapshot(machine(), m_entry.saved) == STATERR_NONE,
				"Save each IRQ/NMI entry boundary");
		}
		else
		{
			expect(m_entry.reads == 1 && m_entry.acknowledgements == 1 && m_entry.stack_writes == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff &&
				data.read_word(0x0fff) == 0x0800 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == vector + 3 &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == (nmi ? 3 : 2) &&
				m_cpu->state_int(tms320c54x_device::STATE_IDLE),
				"Reference and restored entry finish once with retained opcode and original source");
			if (m_phase == 450002)
			{
				m_entry.fetch_tap.remove(); m_entry.stack_tap.remove();
				if (m_entry.restore_case < 9) { start_entry_stage_restore_case(m_entry.restore_case + 1); return true; }
				osd_printf_info("TMS320C54x entry stage restoration: PASS cases=10 sources=irq,nmi boundaries=5 one_fetch=1 one_ack=1 one_stack_write=1 retained_opcode=1 pipeline_drain_claim=0\n");
				return true;
			}
			expect(restore_snapshot(machine(), m_entry.saved) == STATERR_NONE,
				"Restore each IRQ/NMI entry boundary");
			m_entry.reads = m_entry.acknowledgements = unsigned(stage >= 3);
			m_entry.stack_writes = unsigned(stage >= 4);
		}
		// Accepted sources survive later masking. After Fetch, changing memory
		// must not replace the already retained first vector opcode.
		m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 2);
		if (stage >= 3) m_cpu->space(AS_PROGRAM).write_word(vector, 0xf4e1);
		m_phase = m_phase == 450000 ? 450001 : 450002;
		m_check_timer->adjust(attotime::from_usec(100)); return true;
	}
	return false;
}

void tms320c54x_test_state::acknowledge_entry_interrupt(int line)
{
	if (m_phase >= 450000 && m_phase <= 450002)
	{
		expect(line == (m_entry.restore_case >= 5 ? INPUT_LINE_NMI : 0),
			"Restored entry acknowledges only the originally accepted IRQ/NMI source");
		++m_entry.acknowledgements;
	}
	if (m_phase >= 190000 && m_phase <= 190003)
	{
		expect(m_entry.acknowledged_source == -1 && m_cpu->state_int(tms320c54x_device::STATE_IFR) == 2,
			"Vector acknowledgement clears IFR exactly once at Fetch");
		expect(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800,
			"Maskable vector acknowledgement sets INTM with IFR clearance");
		m_entry.acknowledged_source = line;
	}
	if (m_phase >= 80000 && m_phase <= 80003)
	{
		expect(line == (m_entry.vector_case ? INPUT_LINE_NMI : 0), "Vector acknowledgement retains accepted source identity");
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff &&
			m_cpu->space(AS_DATA).read_word(0x0fff) == 0xbeef,
			"SPRU131G vector acknowledgement follows SP Access but precedes the continuation memory write");
		++m_entry.acknowledgements;
	}
}

} // namespace c54x_test
